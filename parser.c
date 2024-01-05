#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "elf.h"
#include "instr.h"
#include "lexer.h"
#include "list.h"
#include "relocations.h"
#include "strmap.h"
#include "symbols.h"
#include "utils.h"
#include "was.h"

static List *instruction_sets;

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

void parse_directive_statement(void) {
    int directive = cur_token;
    next();

    switch (directive) {
        case TOK_DIRECTIVE_ALIGN:
            printf("TODO: .align\n");
            skip();
            break;

        case TOK_DIRECTIVE_BYTE: {
            char value = parse_signed_integer();
            add_to_current_section(&value, 1);
            break;
        }

        case TOK_DIRECTIVE_COMM:
            printf("TODO: .comm\n");
            skip();
            break;

        case TOK_DIRECTIVE_DATA:
            set_current_section(".data");
            break;

        case TOK_DIRECTIVE_FILE:
            expect(TOK_STRING_LITERAL, "filename");
            add_file_symbol(strdup(cur_string_literal.data));
            next();
            break;

        case TOK_DIRECTIVE_GLOBL: {
            expect(TOK_IDENTIFIER, "symbol");
            Symbol *symbol = get_or_add_symbol(strdup(cur_identifier));
            symbol->binding = STB_GLOBAL;
            next();
            break;
        }

        case TOK_DIRECTIVE_LOCAL:
            printf("TODO: .local\n");
            skip();
            break;

        case TOK_DIRECTIVE_LONG: {
            int value = parse_signed_integer();
            add_to_current_section(&value, 4);
            break;
        }

        case TOK_DIRECTIVE_QUAD: {
            long value = parse_signed_integer();
            add_to_current_section(&value, 8);
            break;
        }

        case TOK_DIRECTIVE_SECTION:
            expect(TOK_IDENTIFIER, "section name");
            set_current_section(cur_identifier);
            next();
            break;

        case TOK_DIRECTIVE_SIZE:
            printf("TODO: .size\n");
            skip();
            break;

        case TOK_DIRECTIVE_STRING:
            expect(TOK_STRING_LITERAL, "string literal");
            add_to_current_section(cur_string_literal.data, cur_string_literal.size);
            next();
            break;

        case TOK_DIRECTIVE_TEXT:
            set_current_section(".text");
            break;

        case TOK_DIRECTIVE_TYPE:
            printf("TODO: .type\n");
            skip();
            break;

        case TOK_DIRECTIVE_ULEB128:
            printf("TODO: .uleb128\n");
            skip();
            break;

        case TOK_DIRECTIVE_WORD: {
            short value = parse_signed_integer();
            add_to_current_section(&value, 2);
            break;
        }

        case TOK_DIRECTIVE_ZERO:
            add_zeros_to_current_section(cur_long);
            next();
            break;

        default:
            error("Unknown token %d", directive);
    }
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
    else
        identifier = strdup(identifier);

    Symbol *symbol = get_or_add_symbol(identifier);
    op->relocation_symbol = symbol;
}

// Determine integer size
static int get_integer_size(long value) {
    if (value >= -128 && value <= 255) return SIZE08;
    else if (value >= -65536 && value <= 65535) return SIZE16;
    else if (value >= -2147483648L && value <= 4294967295L) return SIZE32;
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
        expect(TOK_INTEGER, "integer");
        op->type = get_integer_size(cur_long) + IMM08 -  SIZE08;
        op->imm_or_mem_value = cur_long;
        next();
    }

    else if (cur_token == TOK_INTEGER || cur_token == TOK_MINUS) {
        // Memory
        int value = parse_signed_integer();
        op->type = MEM32; // Default memory address size

        if (cur_token == TOK_LPAREN) {
            // Parse 5(...)
            parse_indirect_operand(op);
            op->displacement = value;
            op->displacement_size = get_integer_size(value);

            // Displacements are only possible with 8 and 32 bits
            if (op->displacement_size == SIZE16)
                op->displacement_size = SIZE32;
            else if (op->displacement_size == SIZE64)
                error("Invalid operand size");
        }

        else
            op->imm_or_mem_value = value;
    }

    else if (cur_token == TOK_LPAREN) {
        // Indirect without a displacement
        parse_indirect_operand(op);
    }

    else if (cur_token == TOK_IDENTIFIER) {
        // Identifier with potential indirect
        op->type = MEM32; // Default memory address size
        char *identifier_copy = strdup(cur_identifier);
        preprocess_op_relocation(op, cur_identifier);
        next();

        if (cur_token == TOK_LPAREN) {
            // Parse identifier(...)
            parse_indirect_operand(op);
            op->displacement_size = SIZE32;

            preprocess_op_relocation(op, identifier_copy);
            free(identifier_copy);
        }
    }

    else
        error("Unable to parse operand for token %d", cur_token);
}

InstructionsSet *parse_instruction_statement(void) {
    char *mnemonic = strdup(cur_identifier);
    next();

    // Only one instruction will ever be processed at the same time, so
    // use static memory for the operands.
    static Operand static_op1;
    static Operand static_op2;

    Operand *op1 = NULL;
    Operand *op2 = NULL;

    if (cur_token != TOK_EOL && cur_token != TOK_EOF) {
        parse_operand(&static_op1);
        op1 = &static_op1;
    }

    if (cur_token == TOK_COMMA) {
        next();
        parse_operand(&static_op2);
        op2 = &static_op2;
    }

    Instructions instr = make_instructions(mnemonic, op1, op2);

    InstructionsSet *instructions_set = malloc(sizeof(InstructionsSet));
    append_to_list(instruction_sets, instructions_set);
    instructions_set->primary = calloc(1, sizeof(Instructions));
    *instructions_set->primary = instr;
    instructions_set->using_primary = 1;

    if (instr.branch && op1 && op1->type == MEM32) {
        op1->type = MEM08;
        Instructions alt_instr = make_instructions(mnemonic, op1, op2);

        instructions_set->secondary = calloc(1, sizeof(Instructions));
        *instructions_set->secondary = alt_instr;
    }

    free(mnemonic);

    Operand *relocation_op = NULL;
    if (op1 && op1->relocation_symbol)
        relocation_op = op1;
    else if (op2 && op2->relocation_symbol)
        relocation_op = op2;

    if (relocation_op) {
        instructions_set->primary->relocation_symbol = relocation_op->relocation_symbol;
        if (instructions_set->secondary)
            instructions_set->secondary->relocation_symbol = relocation_op->relocation_symbol;
    }

    return instructions_set;
}

int parse(void) {
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
        if (cur_token >= TOK_DIRECTIVE_ALIGN && cur_token <= TOK_DIRECTIVE_ZERO)
            parse_directive_statement();
        else if (cur_token == TOK_INSTRUCTION) {
            if (get_current_section() != &section_text)
                panic("Instructions can only be added to the .text section");

            InstructionsSet *instr_set = parse_instruction_statement();

            for (int i = 0; i < labels->length; i++) {
                char *name = labels->elements[i];
                Symbol *symbol = get_or_add_symbol(strdup(name));

                if (!instr_set->symbols) instr_set->symbols = new_list(labels->length);
                append_to_list(instr_set->symbols, symbol);
            }

        }
        else if (cur_token == TOK_EOF)
            break;
        else {
            skip();
            error("Don't know what do do with token %d", cur_token);
        }

        for (int i = 0; i < labels->length; i++) free(labels->elements[i]);
        free_list(labels);

        while (cur_token == TOK_EOL) next();
    }
}

// For all instructions that have symbols, update the offset of the symbol
// based on the selection of primary/secondary instructions.
void make_symbol_offsets(void) {
    int offset = 0;

    for (int i = 0; i < instruction_sets->length; i++) {
        InstructionsSet *is = instruction_sets->elements[i];

        List *symbols = is->symbols;
        if (symbols) {
            for (int j = 0; j < symbols->length; j++) {
                Symbol *symbol = symbols->elements[j];
                symbol->offset = offset;
                symbol->section_index = section_text.index;
            }
        }

        offset += is->using_primary ? is->primary->size : is->secondary->size;
    }
}

// Do one pass over all instructions and find branch instructions that
// can be truncated due to proximity to its target.
int reduce_branch_instructions(void) {
    int offset = 0;

    int changed = 0;

    for (int i = 0; i < instruction_sets->length; i++) {
        InstructionsSet *is = instruction_sets->elements[i];

        if (is->using_primary && is->secondary && is->primary->relocation_symbol && is->primary->relocation_symbol->section_index == section_text.index) {
            int relative_offset = is->primary->relocation_symbol->offset - (offset + is->secondary->relocation_offset + 1);

            if (relative_offset >= -128 && relative_offset <= 127) {
                is->using_primary = 0;
                changed = 1;
            }
        }

        offset += is->using_primary ? is->primary->size : is->secondary->size;
    }

    return changed;
}


void emit_code(void) {
    // Reduce code size by looking for near branch instructions
    // using the secondary instructions in the set
    make_symbol_offsets();

    while (1) {
        // This is a timebomb since it runs in O(n^2). To be revisited.
        if (!reduce_branch_instructions()) break;
        make_symbol_offsets();
    }

    // Add the code to the .text section
    for (int i = 0; i < instruction_sets->length; i++) {
        InstructionsSet *is = instruction_sets->elements[i];
        Instructions *instr = is->using_primary ? is->primary : is->secondary;

        // All branch instructions use 32 bit memory addresses for the time being,
        // so we're only taking the primary instructions into account.
        int base_offset = get_current_section_size();

        if (instr->relocation_symbol) {
            int relocation_type;
            if (instr->branch) // This is set for branch opcodes
                relocation_type = R_X86_64_PLT32;
            else
                relocation_type = R_X86_64_PC32;

            if (instr->relocation_symbol->section_index != section_text.index)
                add_relocation(instr->relocation_symbol, relocation_type, base_offset + instr->relocation_offset);
            else {
                if (is->using_primary) {
                    int relative_offset = instr->relocation_symbol->offset - (base_offset + instr->relocation_offset + 4);
                    memcpy(instr->data + instr->relocation_offset, &relative_offset, 4); // 32 bit address
                }
                else {
                    instr = is->secondary;
                    char relative_offset = instr->relocation_symbol->offset - (base_offset + instr->relocation_offset + 1);
                    memcpy(instr->data + instr->relocation_offset, &relative_offset, 1); // 8 bit address
                }
            }
        }

        add_to_section(&section_text, instr->data, instr->size);
    }
}

void init_parser(void) {
    instruction_sets = new_list(10240);
}
