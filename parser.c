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

#define MAX_BRANCH_REDUCTIONS 0 // TODO: Disabled due to bugs in the algorithm

typedef struct simple_expression {
    Symbol *symbol; // Optional symbol
    long value;     // Optional value. If symbol is set, it's an offset
} SimpleExpression;

List *text_chunks;

// TODO remove skip
static void skip() {
    while (cur_token != TOK_EOL && cur_token != TOK_EOF) next();
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

// Add primary instructions to new instruction set and add it to the list
static TextChunk *add_data_to_text_chunks(Instructions instr) {
    TextChunk *text_chunk = calloc(1, sizeof(TextChunk));
    text_chunk->type = CT_DATA;
    append_to_list(text_chunks, text_chunk);
    text_chunk->cdc.primary = malloc(sizeof(Instructions));
    *text_chunk->cdc.primary = instr;
    text_chunk->cdc.using_primary = 1;
}

// Parse .byte, .word, .long and .quad in .text section
// Data in the .text segment is processed in a second phase in emit_code(), like
// instructions.
static TextChunk *parse_data_directive_text(int relocation_type, int size) {
    SimpleExpression expr = parse_simple_expression();

    Instructions instr = {0};
    instr.size = size;

    if (expr.symbol) {
        instr.relocation_symbol = expr.symbol;
        instr.relocation_type = relocation_type;
        instr.relocation_addend = expr.value;
    }
    else
        memcpy(instr.data, &expr.value, size);

    return add_data_to_text_chunks(instr);
}

// Parse .byte, .word, .long and .quad in .data and .rodata section
// Data in these sections are added to the section immediately. Also, relocations
// can be added straight away since the offsets in the segment are known at his point.
static TextChunk *parse_data_directive_data(int relocation_type, int size) {
    SimpleExpression expr = parse_simple_expression();

    if (expr.symbol) {
        ElfSection *cur = get_current_section();
        ElfSection *rel;

        if (cur == &section_data)
            rel = &section_rela_data;
        else if (cur == &section_rodata)
            rel = &section_rela_rodata;
        else
            error("Cannot add data to sections other than .data and .rodata");

        add_relocation(rel, expr.symbol, relocation_type, cur->size, expr.value);
        add_zeros_to_current_section(size);
    }
    else
        add_to_current_section(&expr.value, size);

    return NULL;
}

// Parse .byte, .word, .long and .quad
static TextChunk *parse_data_directive(int relocation_type, int size) {
    if (get_current_section() == &section_text)
        return parse_data_directive_text(relocation_type, size);
    else
        return parse_data_directive_data(relocation_type, size);
}

TextChunk *parse_directive_statement(void) {
    TextChunk *result = NULL;
    int directive = cur_token;
    next();

    switch (directive) {
        case TOK_DIRECTIVE_ALIGN: {
            long value = parse_signed_integer();
            ElfSection *current_section = get_current_section();
            if (current_section != &section_data && current_section != &section_rodata)
                panic("Can only use .align in .data and .rodata sections");

            if ((value & (value - 1)) != 0) panic(".align is not a power of 2");

            int new_size = (current_section->size + value - 1) & ~(value - 1);
            int needed_zeros =  new_size - current_section->size;
            if (needed_zeros)
                add_zeros_to_current_section(needed_zeros);

            break;
        }

        case TOK_DIRECTIVE_BYTE:
            result = parse_data_directive(R_X86_64_8, 1);
            break;

        case TOK_DIRECTIVE_WORD:
            result = parse_data_directive(R_X86_64_16, 2);
            break;

        case TOK_DIRECTIVE_LONG:
            result = parse_data_directive(R_X86_64_32, 4);
            break;

        case TOK_DIRECTIVE_QUAD:
            result = parse_data_directive(R_X86_64_64, 8);
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
            if (!symbol)
                symbol = add_symbol(strdup(cur_identifier));
            else
                was_local = symbol->binding == STB_LOCAL;

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
                symbol->section_index = section_bss.index;
                symbol->value = section_bss.size;
                section_bss.size += symbol->size;
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
            expect(TOK_IDENTIFIER, "section name");
            set_current_section(cur_identifier);
            next();
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
                TextChunk *text_chunk = calloc(1, sizeof(TextChunk));
                text_chunk->type = CT_SIZE_EXPR;
                text_chunk->sic.size_expr = root;
                text_chunk->sic.size_symbol = symbol;
                append_to_list(text_chunks, text_chunk);
            }

            break;
        }

        case TOK_DIRECTIVE_STRING:
            expect(TOK_STRING_LITERAL, "string literal");
            add_to_current_section(cur_string_literal.data, cur_string_literal.size);
            next();
            break;

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
            printf("TODO: .uleb128\n");
            skip();
            break;

        case TOK_DIRECTIVE_ZERO: {
            if (get_current_section() == &section_text) {
                TextChunk *text_chunk = calloc(1, sizeof(TextChunk));
                append_to_list(text_chunks, text_chunk);

                text_chunk->type = CT_ZERO;
                text_chunk->zec.size = cur_long;
            }
            else
                add_zeros_to_current_section(cur_long);

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

TextChunk *parse_instruction_statement(void) {
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

    TextChunk *text_chunk = calloc(1, sizeof(TextChunk));
    append_to_list(text_chunks, text_chunk);
    text_chunk->cdc.primary = malloc(sizeof(Instructions));
    *text_chunk->cdc.primary = instr;
    text_chunk->cdc.using_primary = 1;
    text_chunk->type = CT_CODE;

    if (instr.branch && op1 && op1->type == MEM32) {
        op1->type = MEM08;
        Instructions alt_instr = make_instructions(mnemonic, op1, op2, op3);

        text_chunk->cdc.secondary = calloc(1, sizeof(Instructions));
        *text_chunk->cdc.secondary = alt_instr;
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
        else if (text_chunk->cdc.primary->branch) // This is set for branch opcodes
            relocation_type = R_X86_64_PLT32;
        else
            relocation_type = R_X86_64_PC32;

        text_chunk->cdc.primary->relocation_type = relocation_type;
        text_chunk->cdc.primary->relocation_symbol = relocation_op->relocation_symbol;
        text_chunk->cdc.primary->relocation_addend = relocation_addend;

        if (text_chunk->cdc.secondary) {
            text_chunk->cdc.secondary->relocation_type = relocation_type;
            text_chunk->cdc.secondary->relocation_symbol = relocation_op->relocation_symbol;
            text_chunk->cdc.secondary->relocation_addend = relocation_addend;
        }
    }

    return text_chunk;
}

static void add_labels_to_text_chunk(TextChunk *text_chunk, List *labels) {
    for (int i = 0; i < labels->length; i++) {
        char *name = labels->elements[i];
        Symbol *symbol = get_or_add_symbol(strdup(name));

        if (!text_chunk->symbols) text_chunk->symbols = new_list(labels->length);
        append_to_list(text_chunk->symbols, symbol);
    }
}

void parse(void) {
    while (cur_token != TOK_EOF) {
        while (cur_token == TOK_EOL) next();

        List *labels = new_list(4);

        // Collect labels
        while (cur_token == TOK_LABEL) {
            append_to_list(labels, strdup(cur_identifier));
            next();
            while (cur_token == TOK_EOL) next(); // More labels can follow
        }

        // If we're not in .text, add symbols for the labels
        if (get_current_section() != &section_text) {
            for (int i = 0; i < labels->length; i++) {
                char *name = labels->elements[i];
                Symbol *symbol = get_or_add_symbol(strdup(name));
                associate_symbol_with_current_section(symbol);
            }
        }

        // Parse statement
        if (cur_token >= TOK_DIRECTIVE_ALIGN && cur_token <= TOK_DIRECTIVE_ZERO) {
            TextChunk *text_chunk = parse_directive_statement();

            if (text_chunk) add_labels_to_text_chunk(text_chunk, labels);
        }
        else if (cur_token == TOK_INSTRUCTION) {
            if (get_current_section() != &section_text)
                panic("Instructions can only be added to the .text section");

            TextChunk *text_chunk = parse_instruction_statement();

            add_labels_to_text_chunk(text_chunk, labels);
        }
        else if (cur_token == TOK_EOF)
            break;
        else {
            skip();
            error("Syntax error at token %d", cur_token);
        }

        for (int i = 0; i < labels->length; i++) free(labels->elements[i]);
        free_list(labels);

        while (cur_token == TOK_EOL) next();
    }
}

void emit_code(void) {
    reduce_branch_instructions();

    // Add the code to the .text section
    for (int i = 0; i < text_chunks->length; i++) {
        TextChunk *tc = text_chunks->elements[i];
        Instructions *instr = tc->cdc.using_primary ? tc->cdc.primary : tc->cdc.secondary;

        // All branch instructions use 32 bit memory addresses for the time being,
        // so we're only taking the primary instructions into account.
        int base_offset = get_current_section_size();

        if (!tc->type) panic("Internal error: zero tc->type");

        if (tc->type == CT_SIZE_EXPR) {
            Value value = evaluate_node(tc->sic.size_expr, base_offset);
            if (value.symbol) panic("Unexpectedly got a symbol when evaluating .size");
            tc->sic.size_symbol->size = value.number;
        }

        else if ((tc->type == CT_CODE || tc->type == CT_DATA) && instr->relocation_symbol) {
            int relocation_type;
            if (instr->relocation_type)
                relocation_type = instr->relocation_type; // Set by data handling code
            else if (instr->branch) // This is set for branch opcodes
                relocation_type = R_X86_64_PLT32;
            else
                relocation_type = R_X86_64_PC32;

            // Does the symbol need an entry in the relocation table?
            if (
                    instr->relocation_symbol->section_index != section_text.index ||
                    tc->type == CT_DATA ||
                    instr->relocation_symbol->binding == STB_GLOBAL ||
                    instr->relocation_type == R_X86_64_REX_GOTP
                    ) {

                // For code relocations , a relative relocation is calculated from the end of the instruction.
                // The linker doesn't know this though, so it needs to get an
                // addend = -(instr->size - instr->relocation_offset)
                int relocation_addend_offset = tc->type == CT_CODE ? instr->relocation_offset - instr->size : 0;
                add_relocation(
                    &section_rela_text, instr->relocation_symbol, relocation_type,
                    base_offset + instr->relocation_offset, instr->relocation_addend + relocation_addend_offset);
            }

            // The symbol address is known and can be used directly.
            else {
                if (tc->cdc.using_primary) {
                    int relative_offset = instr->relocation_symbol->value - (base_offset + instr->relocation_offset + 4) + instr->relocation_addend;
                    memcpy(instr->data + instr->relocation_offset, &relative_offset, 4); // 32 bit address
                }
                else {
                    instr = tc->cdc.secondary;

                    // Double check relative offset doesn't exceed the limits of a signed char.
                    int relative_offset_int = instr->relocation_symbol->value - (base_offset + instr->relocation_offset + 1) + instr->relocation_addend;
                    if (relative_offset_int < -128 || relative_offset_int > 127)
                        panic("Relative offset for code at %#lx out of bounds for symbol %s@%#x: %d",
                            base_offset, instr->relocation_symbol->name, instr->relocation_symbol->value, relative_offset_int);

                    char relative_offset = relative_offset_int;
                    memcpy(instr->data + instr->relocation_offset, &relative_offset, 1); // 8 bit address
                }
            }
        }

        if (tc->type != CT_SIZE_EXPR) {
            if (tc->type == CT_ZERO)
                add_zeros_to_section(&section_text, tc->zec.size);
            else
                add_to_section(&section_text, instr->data, instr->size);
        }
    }
}

void init_parser(void) {
    text_chunks = new_list(10240);
}
