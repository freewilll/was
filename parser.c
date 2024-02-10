#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "branches.h"
#include "elf.h"
#include "expr.h"
#include "instr.h"
#include "lexer.h"
#include "list.h"
#include "parser.h"
#include "relocations.h"
#include "strmap.h"
#include "symbols.h"
#include "utils.h"
#include "was.h"

typedef struct simple_expression {
    Symbol *symbol; // Optional symbol
    long value;     // Optional value. If symbol is set, it's an offset
} SimpleExpression;

static List *cur_chunks;       // Chunks list for current section

// Lookup or create section by name and make it the current section things are being added to
static void set_current_section(char *name) {
    Section *section = get_section(name);
    if (!section) section = add_section(name, SHT_PROGBITS, 0, 1);
    if (!section->chunks) section->chunks = new_list(10240);
    cur_chunks = section->chunks;
}

static long parse_signed_integer(void) {
    int negative = 0;
    if (cur_token == TOK_MINUS) {
        negative = 1;
        next();
    }
    expect(TOK_INTEGER, "integer");
    long result = negative ? -cur_long : cur_long;
    next();

    return result;
}

// Parse an expression that does not need a second pass to be evaluated.
// Suitable for anything that requires either a number or a symbol + offset
// that can be used to create a relocation.
static SimpleExpression parse_simple_expression(void) {
    Node *root = parse_expression();

    if (!root->value) error("Expected an expression not requiring symbol resolution");

    SimpleExpression result;
    result.symbol = root->value->symbol;
    result.value = root->value->number;

    return result;
}

// Parse .byte, .word, .long, .quad, etc
static Chunk *parse_data_directive(int size) {
    Chunk *chunk = calloc(1, sizeof(Chunk));
    chunk->type = CT_DATA;
    chunk->dac.expr = parse_expression();
    chunk->dac.size = size;

    append_to_list(cur_chunks, chunk);

    return chunk;
}

// Parse and encode .uleb128
static Chunk *parse_uleb128(void) {
    Chunk *chunk = calloc(1, sizeof(Chunk));
    chunk->type = CT_DATA;

    expect(TOK_INTEGER, "integer");
    int value = cur_long;
    next();

    char *data = malloc(8);

    int pos = 0;
    while (1) {
        unsigned char c = value & 0x7f;
        value >>= 7;
        if (value) c |= 0x80;
        data[pos++] = c;
        if (!value) break;
    }

    chunk->dac.data = data;
    chunk->dac.size = pos;

    append_to_list(cur_chunks, chunk);

    return chunk;
}

Chunk *parse_directive_statement(void) {
    Chunk *result = NULL;
    int directive = cur_token;
    next();

    switch (directive) {
        case TOK_DIRECTIVE_ALIGN: {
            long value = parse_signed_integer();
            if ((value & (value - 1)) != 0) panic(".align is not a power of 2");

            Chunk *result = calloc(1, sizeof(Chunk));
            result->type = CT_ALIGN;
            result->aic.alignment = value;

            append_to_list(cur_chunks, result);

            break;
        }

        case TOK_DIRECTIVE_BYTE:
            result = parse_data_directive(1);
            break;

        case TOK_DIRECTIVE_WORD:
        case TOK_DIRECTIVE_VALUE:
            result = parse_data_directive(2);
            break;

        case TOK_DIRECTIVE_LONG:
            result = parse_data_directive(4);
            break;

        case TOK_DIRECTIVE_QUAD:
            result = parse_data_directive(8);
            break;

        case TOK_DIRECTIVE_DATA:
            set_current_section(".data");
            break;

        case TOK_DIRECTIVE_FILE:
            expect(TOK_STRING_LITERAL, "filename");
            add_file_symbol(strdup(cur_string_literal.data));
            next();
            break;

        case TOK_DIRECTIVE_COMM: {
            expect(TOK_IDENTIFIER, "symbol");

            // Need to see if the symbol is preexisting and was flagged as a local
            Symbol *symbol = get_symbol(cur_identifier);
            int was_local = 0;
            if (!symbol) {
                symbol = add_symbol(strdup(cur_identifier));
            }
            else {
                was_local = 1;
                symbol->binding = STB_LOCAL;
            }

            next();
            consume(TOK_COMMA, ",");
            long size = parse_signed_integer();
            consume(TOK_COMMA, ",");
            long alignment = parse_signed_integer();

            symbol->type = STT_OBJECT;
            symbol->size = size;

            if (was_local) {
                // The symbol was already declared as .local. Adding a .comm to that
                // allocates local bss storage for it.
                symbol->section = section_bss;
                symbol->value = section_bss->size;
                section_bss->size += symbol->size;
            }
            else {
                // The symbol may be merged with other symbols and so becomes global
                symbol->section_index = SHN_COMMON;
                symbol->value = alignment;
                symbol->binding = STB_GLOBAL;
            }

            break;
        }

        case TOK_DIRECTIVE_GLOBL: {
            expect(TOK_IDENTIFIER, "symbol");
            Symbol *symbol = get_or_add_symbol(strdup(cur_identifier));
            symbol->binding = STB_GLOBAL;
            next();
            break;
        }

        case TOK_DIRECTIVE_LOCAL: {
            expect(TOK_IDENTIFIER, "symbol");
            Symbol *symbol = get_or_add_symbol(strdup(cur_identifier));
            if (symbol->binding != STB_GLOBAL) symbol->binding = STB_LOCAL; // Global trumps local
            next();
            break;
        }

        case TOK_DIRECTIVE_SECTION:
            // Parse:
            //.- section .testing1
            //.- section .testing1, ""
            //.- section .debug_info,"",@progbits
            //.- section .debug_str,"MS"
            //.- section .debug_str,"MS",@progbits,1
            //.- section .debug_strx,"S",@progbits

            expect(TOK_IDENTIFIER, "section name");
            char *name = strdup(cur_identifier);
            next();

            int flags = 0;
            if (cur_token == TOK_COMMA) {
                next();
                expect(TOK_STRING_LITERAL, "flags string literal");

                for (int i = 0; i < cur_string_literal.size - 1; i++) {
                    char c = cur_string_literal.data[i];

                    switch (c) {
                        case 'a': flags |= SHF_ALLOC;     break;
                        case 'w': flags |= SHF_WRITE;     break;
                        case 'x': flags |= SHF_EXECINSTR; break;
                        case 'M': flags |= SHF_MERGE;     break;
                        case 'S': flags |= SHF_STRINGS;   break;
                        default: error("Invalid flag %c", c);

                    }
                }

                next();
            }

            int type = SHT_PROGBITS;
            if (cur_token == TOK_COMMA) {
                next();
                expect(TOK_IDENTIFIER, "Expected @progbits"); // Other types aren't implemented
                if (strcmp(cur_identifier, "@progbits")) error("Expected @progbits; others aren't implemented");
                next();
            }

            if (cur_token == TOK_COMMA) {
                next();
                expect(TOK_INTEGER, "entsize");
                if (cur_long != 1) error("Values other than 1 for entsise aren't implemented");
                next();
            }

            if (!get_section(name)) add_section(name, type, flags, 1);

            set_current_section(name); // Auto creates the section

            break;

        case TOK_DIRECTIVE_SIZE: {
            expect(TOK_IDENTIFIER, "identifier");
            Symbol *symbol = get_or_add_symbol(strdup(cur_identifier));
            next();
            consume(TOK_COMMA, ",");
            Node *root = parse_expression();

            if (root->value) {
                if (root->value->symbol) panic("Cannot handle a size for a symbol + offset");
                symbol->size = root->value->number;
            }
            else {
                Chunk *result = calloc(1, sizeof(Chunk));
                result->type = CT_SIZE_EXPR;
                result->sic.size_expr = root;
                result->sic.size_symbol = symbol;
                append_to_list(cur_chunks, result);
            }

            break;
        }

        case TOK_DIRECTIVE_STRING: {
            expect(TOK_STRING_LITERAL, "string literal");

            result = calloc(1, sizeof(Chunk));
            result->type = CT_DATA;
            result->dac.data = strdup(cur_string_literal.data);
            result->dac.size = cur_string_literal.size;
            append_to_list(cur_chunks, result);

            next();

            break;
        }

        case TOK_DIRECTIVE_TEXT:
            set_current_section(".text");
            break;

        case TOK_DIRECTIVE_TYPE:
            expect(TOK_IDENTIFIER, "identifier");
            Symbol *symbol = get_or_add_symbol(strdup(cur_identifier));
            next();
            consume(TOK_COMMA, ",");
            expect(TOK_IDENTIFIER, "symbol type");

            if (!strcmp(cur_identifier, "@function"))
                symbol->type = STT_FUNC;
            else if (!strcmp(cur_identifier, "@object"))
                symbol->type = STT_OBJECT;
            else
                error("Unknown symbol type %s", cur_identifier);

            next();

            break;

        case TOK_DIRECTIVE_ULEB128:
            result = parse_uleb128();
            break;

        case TOK_DIRECTIVE_ZERO: {
            Chunk *result = calloc(1, sizeof(Chunk));
            result->type = CT_ZERO;
            result->zec.size = cur_long;

            append_to_list(cur_chunks, result);

            next();

            break;
        }

        default:
            error("Unknown token %d", directive);
    }

    return result;
}

// Truncate the register to a range 0-15 if it's one of the common registers.
static int get_cur_register_reg(void) {
    return cur_register < REG_RIP ? cur_register & 0xf : cur_register;
}

// Parse register, putting the details in op.
static void parse_register(Operand *op) {
    memset(op, 0, sizeof(Operand));

    // A pointer in a register is treated just like a register.
    if (cur_token == TOK_MULTIPLY) next();

    // Truncate unless it's RIP
    op->reg = get_cur_register_reg();

    op->type =
          cur_register < REG_WORD ? REG08
        : cur_register < REG_LONG ? REG16
        : cur_register < REG_QUAD ? REG32
        : cur_register < REG_XMM  ? REG64
        : cur_register < REG_ST   ? REGXM
        : cur_register < REG_RIP  ? REGST
                                  : REG64;

    if (cur_register_alt_8bit) op->type |= ALT_8BIT;

    next();
}

// Parse a parenthesis expression of the form
// (%rax)
// (%rax, %rbx, 2)
static void parse_indirect_operand(Operand *op) {
    consume(TOK_LPAREN, "(");
    parse_register(op);

    if (cur_token == TOK_COMMA) {
        // Parse (base, index, scale)

        op->has_sib = 1;
        op->base = op->reg;
        next();

        expect(TOK_REGISTER, "register");
        op->index = get_cur_register_reg();
        next();

        consume(TOK_COMMA, ",");
        expect(TOK_INTEGER, "integer");

        switch (cur_long) {
            case 1: op->scale = 0; break;
            case 2: op->scale = 1; break;
            case 4: op->scale = 2; break;
            case 8: op->scale = 3; break;
            default: error("Invalid scale");
        }

        next();
    }

    consume(TOK_RPAREN, ")");

    op->indirect = 1;
}

// Register/look up the symbol for a relocation and store it in the op.
static void preprocess_op_relocation(Operand *op, char *identifier) {
    if (string_ends_with(identifier, "@PLT")) {
        char *symbol_name = strdup(identifier);
        symbol_name[strlen(identifier) - 4] = 0;
        identifier = symbol_name;
    }
    else if (string_ends_with(identifier, "@GOTPCREL")) {
        char *symbol_name = strdup(identifier);
        symbol_name[strlen(identifier) - 9] = 0;
        identifier = symbol_name;
        op->relocation_type = R_X86_64_REX_GOTP;
    }
    else
        identifier = strdup(identifier);

    Symbol *symbol = get_or_add_symbol(identifier);
    op->relocation_symbol = symbol;
}

// Determine integer size
static int get_integer_size(long value) {
    if (value >= -0x80 && value <= 0xff) return SIZE08;
    else if (value >= -0x10000 && value <= 0xffff) return SIZE16;
    else if (value >= -0x80000000L && value <= 0xffffffff) return SIZE32;
    else return SIZE64;
}

// Parse an operand
static void parse_operand(Operand *op) {
    memset(op, 0, sizeof(Operand));

    if (cur_token == TOK_REGISTER || cur_token == TOK_MULTIPLY) {
        parse_register(op);
    }

    else if (cur_token == TOK_DOLLAR) {
        // Immediate
        next();
        long value = parse_signed_integer();
        op->type = get_integer_size(value) + IMM08 -  SIZE08;
        op->imm_or_mem_value = value;
    }

    else if (cur_token == TOK_INTEGER || cur_token == TOK_MINUS) {
        // Memory
        SimpleExpression expr = parse_simple_expression();
        if (expr.symbol) error("Unexpected symbol in expression"); // Not implemented

        int value = expr.value;
        op->type = MEM32; // Default memory address size

        if (cur_token == TOK_LPAREN) {
            // Parse 5(...)
            parse_indirect_operand(op);

            // Only add displacement if the value is non zero
            if (value) {
                op->displacement = value;
                op->displacement_size = get_integer_size(value);

                // Displacements are only possible with 8 and 32 bits
                if (op->displacement_size == SIZE16)
                    op->displacement_size = SIZE32;
                else if (op->displacement_size == SIZE64)
                    error("Invalid operand size");
            }
        }

        else
            op->imm_or_mem_value = value;
    }

    else if (cur_token == TOK_IDENTIFIER) {
        // Parse:
        // identifier
        // identifier+n
        // identifier(%reg...)
        // identifier+n(%reg...)

        op->type = MEM32; // Default memory address size
        char *identifier_copy = strdup(cur_identifier);
        next();

        // identifier+n
        int relocation_addend = 0;
        if (cur_token == TOK_PLUS || cur_token == TOK_MINUS) {
            int negative = cur_token == TOK_MINUS;
            next();

            relocation_addend = parse_signed_integer();
            if (negative) relocation_addend = - relocation_addend;
        }

        preprocess_op_relocation(op, identifier_copy);

        if (cur_token == TOK_LPAREN) {
            // (...)
            parse_indirect_operand(op);
            op->displacement_size = SIZE32;
            op->relocation_addend = relocation_addend;

            preprocess_op_relocation(op, identifier_copy);
            free(identifier_copy);
        }
    }

    else if (cur_token == TOK_LPAREN) {
        // Indirect without an identifier/displacement
        parse_indirect_operand(op);
    }

    else
        error("Unable to parse operand for token %d", cur_token);
}

Chunk *parse_instruction_statement(void) {
    char *mnemonic = strdup(cur_identifier);
    next();

    // Only one instruction will ever be processed at the same time, so
    // use static memory for the operands.
    static Operand static_op1;
    static Operand static_op2;
    static Operand static_op3;

    Operand *op1 = NULL;
    Operand *op2 = NULL;
    Operand *op3 = NULL;

    if (cur_token != TOK_EOL && cur_token != TOK_EOF) {
        parse_operand(&static_op1);
        op1 = &static_op1;
    }

    if (cur_token == TOK_COMMA) {
        next();
        parse_operand(&static_op2);
        op2 = &static_op2;
    }

    if (cur_token == TOK_COMMA) {
        next();
        parse_operand(&static_op3);
        op3 = &static_op3;
    }

    Instructions instr = make_instructions(mnemonic, op1, op2, op3);

    Chunk *chunk = calloc(1, sizeof(Chunk));
    append_to_list(cur_chunks, chunk);
    chunk->coc.primary = malloc(sizeof(Instructions));
    *chunk->coc.primary = instr;
    chunk->coc.using_primary = 1;
    chunk->type = CT_CODE;

    if (instr.branch && op1 && op1->type == MEM32) {
        op1->type = MEM08;
        Instructions alt_instr = make_instructions(mnemonic, op1, op2, op3);

        chunk->coc.secondary = calloc(1, sizeof(Instructions));
        *chunk->coc.secondary = alt_instr;
    }

    free(mnemonic);

    Operand *relocation_op = NULL;
    int relocation_addend = 0;
    if (op1 && op1->relocation_symbol) {
        relocation_op = op1;
        relocation_addend = op1->relocation_addend;
    }
    else if (op2 && op2->relocation_symbol) {
        relocation_op = op2;
        relocation_addend = op2->relocation_addend;
    }
    else if (op3 && op3->relocation_symbol) {
        relocation_op = op3;
        relocation_addend = op3->relocation_addend;
    }

    if (relocation_op) {
        int relocation_type;
        if (relocation_op->relocation_type)
            relocation_type = relocation_op->relocation_type;
        else if (chunk->coc.primary->branch) // This is set for branch opcodes
            relocation_type = R_X86_64_PLT32;
        else
            relocation_type = R_X86_64_PC32;

        chunk->coc.primary->relocation.type = relocation_type;
        chunk->coc.primary->relocation.symbol = relocation_op->relocation_symbol;
        chunk->coc.primary->relocation.addend = relocation_addend;

        if (chunk->coc.secondary) {
            chunk->coc.secondary->relocation.type = relocation_type;
            chunk->coc.secondary->relocation.symbol = relocation_op->relocation_symbol;
            chunk->coc.secondary->relocation.addend = relocation_addend;
        }
    }

    return chunk;
}

void parse(void) {
    while (cur_token != TOK_EOF) {
        while (cur_token == TOK_EOL) next();

        List *labels = new_list(4);

        // Collect labels
        while (cur_token == TOK_LABEL) {
            append_to_list(labels, strdup(cur_identifier));

            Chunk *chunk = calloc(1, sizeof(Chunk));
            chunk->type = CT_LABEL;
            chunk->lac.symbol = get_or_add_symbol(strdup(cur_identifier));
            append_to_list(cur_chunks, chunk);

            next();
            while (cur_token == TOK_EOL) next(); // More labels can follow
        }

        // Parse statement
        if (cur_token >= TOK_DIRECTIVE_ALIGN && cur_token <= TOK_DIRECTIVE_ZERO)
            parse_directive_statement();
        else if (cur_token == TOK_INSTRUCTION)
            parse_instruction_statement();
        else if (cur_token == TOK_EOF)
            break;
        else
            error("Syntax error at token %d", cur_token);

        for (int i = 0; i < labels->length; i++) free(labels->elements[i]);
        free_list(labels);

        while (cur_token == TOK_EOL) next();
    }
}

void emit_section_code(Section *section) {
    List *chunks = section->chunks;

    layout_section(section);

    for (int i = 0; i < chunks->length; i++) {
        Chunk *chunk = chunks->elements[i];
        Instructions *instr = chunk->coc.using_primary ? chunk->coc.primary : chunk->coc.secondary;

        // All branch instructions use 32 bit memory addresses for the time being,
        // so we're only taking the primary instructions into account.
        int base_offset = section->size;

        // chunk->offset is really only used to assert that the code in branches.c
        // is consistent with the code here. Admittedly, this reeks of duplication
        // so something isn't pretty here.
        if (base_offset != chunk->offset)
            panic("Internal error: mismatch in running offset (%#x) vs chunk offset (%#x)", base_offset, chunk->offset);

        if (!chunk->type) panic("Internal error: zero chunk->type");

        if (chunk->type == CT_SIZE_EXPR) {
            Value value = evaluate_node(chunk->sic.size_expr, base_offset);
            if (value.symbol) panic("Unexpectedly got a symbol when evaluating .size");
            chunk->sic.size_symbol->size = value.number;
        }

        else if (chunk->type == CT_CODE && instr->relocation.symbol) {
            int relocation_type;
            if (instr->relocation.type)
                relocation_type = instr->relocation.type; // Set by data handling code
            else if (instr->branch) // This is set for branch opcodes
                relocation_type = R_X86_64_PLT32;
            else
                relocation_type = R_X86_64_PC32;

            // Does the symbol need an entry in the relocation table?
            if (
                    instr->relocation.symbol->section != section ||
                    instr->relocation.symbol->binding == STB_GLOBAL ||
                    instr->relocation.type == R_X86_64_REX_GOTP
                    ) {

                // For code relocations , a relative relocation is calculated from the end of the instruction.
                // The linker doesn't know this though, so it needs to get an
                // addend = -(instr->size - instr->relocation.offset)
                int relocation_addend_offset = chunk->type == CT_CODE ? instr->relocation.offset - instr->size : 0;
                add_relocation(
                    get_relocation_section(section), instr->relocation.symbol, relocation_type,
                    base_offset + instr->relocation.offset, instr->relocation.addend + relocation_addend_offset);
            }

            // The symbol address is known and can be used directly.
            else {
                if (chunk->coc.using_primary) {
                    int relative_offset = instr->relocation.symbol->value - (base_offset + instr->relocation.offset + 4) + instr->relocation.addend;
                    memcpy(instr->data + instr->relocation.offset, &relative_offset, 4); // 32 bit address
                }
                else {
                    instr = chunk->coc.secondary;

                    // Double check relative offset doesn't exceed the limits of a signed char.
                    int relative_offset_int = instr->relocation.symbol->value - (base_offset + instr->relocation.offset + 1) + instr->relocation.addend;
                    if (relative_offset_int < -128 || relative_offset_int > 127)
                        panic("Relative offset for code at %#lx out of bounds for symbol %s@%#x: %d",
                            base_offset, instr->relocation.symbol->name, instr->relocation.symbol->value, relative_offset_int);

                    char relative_offset = relative_offset_int;
                    memcpy(instr->data + instr->relocation.offset, &relative_offset, 1); // 8 bit address
                }
            }
        }

        // Add the chunk to the section
        switch (chunk->type)  {
            case CT_CODE:
                add_to_section(section, instr->data, instr->size);
                break;

            case CT_DATA: {
                if (chunk->dac.expr) {
                    Value value = evaluate_node(chunk->dac.expr, base_offset);
                    chunk->dac.data = (char *) &value.number;

                    if (value.symbol) {
                        int relocation_type;
                        switch (chunk->dac.size) {
                            case 1: relocation_type = R_X86_64_8;  break;
                            case 2: relocation_type = R_X86_64_16; break;
                            case 4: relocation_type = R_X86_64_32; break;
                            case 8: relocation_type = R_X86_64_64; break;
                            default: panic("Missing case for data relocation size");
                        }

                        add_relocation(
                            get_relocation_section(section), value.symbol, relocation_type,
                            base_offset, value.number);

                        value.number = 0; // Write a zero - it will be replaced by the linker
                    }
                }

                add_to_section(section, chunk->dac.data, chunk->dac.size);

                break;
            }

            case CT_ZERO:
                add_zeros_to_section(section, chunk->zec.size);
                break;

            case CT_ALIGN: {
                int padding =  PADDING_FOR_ALIGN_UP(section->size, chunk->aic.alignment);
                if (padding) {
                    // Insert NOPs (0x90) as padding in a text section, otherwise zeros
                    char value = section == section_text ? 0x90 : 0;
                    add_repeated_value_to_section(section, value, padding);
                }
                break;
            }

            case CT_SIZE_EXPR:
            case CT_LABEL:
                // Nothing to do here
                break;

            default:
                panic("Unhandled chunk->type");
        }
    }
}

void emit_code(void) {
    for (int i = 0; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];
        if (section->chunks) layout_section(section);
    }

    for (int i = 0; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];
        if (section->chunks) emit_section_code(section);
    }
}

void init_parser(void) {
    set_current_section(".text");
}
