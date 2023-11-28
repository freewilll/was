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

// https://wiki.osdev.org/X86-64_Instruction_Encoding#Registers
const char *register_names[] = {
    "al",   "cl",   "dl",   "bl",   "ah",   "ch",   "dh",   "bh",   "r8b",  "r9b",  "r10b",  "r11b",  "r12b",  "r13b",  "r14b",  "r15b",
    "ax",   "cx",   "dx",   "bx",   "sp",   "bp",   "si",   "di",   "r8w",  "r9w",  "r10w",  "r11w",  "r12w",  "r13w",  "r14w",  "r15w",
    "eax",  "ecx",  "edx",  "ebx",  "esp",  "ebp",  "esi",  "edi",  "r8d",  "r9d",  "r10d",  "r11d",  "r12d",  "r13d",  "r14d",  "r15d",
    "rax",  "rcx",  "rdx",  "rbx",  "rsp",  "rbp",  "rsi",  "rdi",  "r8",   "r9",   "r10",   "r11",   "r12",   "r13",   "r14",   "r15",
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
    "rip",  "st",
};

// TODO remove skip
static void skip() {
    while (cur_token != TOK_EOL && cur_token != TOK_EOF) next();
}

static void parse_directive_statement(void) {
    int directive = cur_token;
    next();

    switch (directive) {
        case TOK_DIRECTIVE_ALIGN:
            printf("TODO: .align\n");
            skip();
            break;

        case TOK_DIRECTIVE_BYTE:
            printf("TODO: .byte\n");
            skip();
            break;

        case TOK_DIRECTIVE_COMM:
            printf("TODO: .comm\n");
            skip();
            break;

        case TOK_DIRECTIVE_DATA:
            printf("TODO: .data\n");
            skip();
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

        case TOK_DIRECTIVE_LONG:
            printf("TODO: .long\n");
            skip();
            break;

        case TOK_DIRECTIVE_QUAD:
            printf("TODO: .quad\n");
            skip();
            break;

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

        case TOK_DIRECTIVE_RODATA:
            printf("TODO: .rodata\n");
            skip();
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

        case TOK_DIRECTIVE_WORD:
            printf("TODO: .word\n");
            skip();
            break;

        case TOK_DIRECTIVE_ZERO:
            printf("TODO: .zero\n");
            skip();
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

    // Truncate unless it's RIP or SP
    op->reg = get_cur_register_reg();

    op->type =
          cur_register < REG_WORD ? REG08
        : cur_register < REG_LONG ? REG16
        : cur_register < REG_QUAD ? REG32
                                  : REG64;

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

// Add a relocation to the relocation section
static void add_instruction_relocation(Instructions *instr, Operand *op1, Operand *op2, int base_offset) {
    Operand *op;
    if (op1 && op1->relocation_symbol)
        op = op1;
    else if (op2 && op2->relocation_symbol)
        op = op2;
    else
        return;

    if (instr->branch)
        op->relocation_type = R_X86_64_PLT32; // This depends on the opcode
    else
        op->relocation_type = R_X86_64_PC32;

    add_relocation(op->relocation_symbol, op->relocation_type, base_offset + instr->relocation_offset);
}

// Determine integer size
static int get_integer_size(long value) {
    if ((unsigned long) value <= 0xff)
        return SIZE08;
    else if ((unsigned long) value <= 0xffff)
        return SIZE16;
    else if ((unsigned long) value <= 0xffffffff)
        return SIZE32;
    else
        return SIZE64;
}

// Parse an operand
static void parse_operand(Operand *op) {
    memset(op, 0, sizeof(Operand));

    if (cur_token == TOK_REGISTER) {
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

    else if (cur_token == TOK_INTEGER) {
        // Memory
        op->type = MEM32; // Default memory address size
        next();

        if (cur_token == TOK_LPAREN) {
            // Parse 5(...)
            long value = cur_long;
            parse_indirect_operand(op);
            op->displacement = value;
            op->displacement_size = get_integer_size(value);

            // Displacements are only possible with 8 and 32 bits
            if (op->displacement_size == SIZE16)
                op->displacement_size = SIZE32;
            else if (op->displacement_size == SIZE64)
                error("Invalid operand size");
        }
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
        panic("Unable to parse operand for token %d\n", cur_token);
}

Instructions parse_instruction_statement(void) {
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
    free(mnemonic);

    int base_offset = get_current_section_size();
    add_to_current_section(instr.data, instr.size);
    if (instr.relocation_size) add_instruction_relocation(&instr, op1, op2, base_offset);

    return instr;
}

int parse(void) {
    while (cur_token != TOK_EOF) {
        List *labels = new_list(4);

        // Collect labels
        while (cur_token == TOK_LABEL) {
            append_to_list(labels, strdup(cur_identifier));
            next();
            while (cur_token == TOK_EOL) next(); // More labels can follow
        }

        for (int i = 0; i < labels->length; i++) {
            char *name = labels->elements[i];
            Symbol *symbol = get_or_add_symbol(strdup(name));
            associate_symbol_with_current_section(symbol);
        }

        // Parse statement
        if (cur_token >= TOK_DIRECTIVE_ALIGN && cur_token <= TOK_DIRECTIVE_ZERO)
            parse_directive_statement();
        else if (cur_token == TOK_INSTRUCTION)
            parse_instruction_statement();
        else if (cur_token == TOK_EOF)
            break;
        else {
            skip();
            panic("Don't know what do do with token %d", cur_token);
        }

        for (int i = 0; i < labels->length; i++) free(labels->elements[i]);
        free_list(labels);

        while (cur_token == TOK_EOL) next();
    }
}
