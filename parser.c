#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "elf.h"
#include "lexer.h"
#include "list.h"
#include "relocations.h"
#include "strmap.h"
#include "symbols.h"
#include "utils.h"
#include "was.h"

// TODO remove skip
void skip() {
    while (cur_token != TOK_EOL && cur_token != TOK_EOF) next();
}

void parse_directive_statement(void) {
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

void parse_instruction_statement(void) {
    printf("TODO: instruction %s\n", cur_identifier);
    skip();
}

// Add code for a hello world test program.
static void add_test_program() {
    char hello_world[] = {
        0x55,                                       //  0:   55                      push   %rbp
        0x48, 0x89, 0xe5,                           //  1:   48 89 e5                mov    %rsp,%rbp

        0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00,   //  4:   48 8d 05 00 00 00 00    lea    0x0(%rip),%rax  // .SL0 relocation
        0x48, 0x89, 0xc7,                           //  b:   48 89 c7                mov    %rax,%rdi
        0xb0, 0x00,                                 //  e:   b0 00                   mov    $0x0,%al
        0xe8, 0x00, 0x00, 0x00, 0x00,               // 10:   e8 00 00 00 00          callq  15 <main+0x15>

        0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00,   // 15:   48 8d 05 00 00 00 00    lea    0x0(%rip),%rax  // .SL1 relocation
        0x48, 0x89, 0xc7,                           // 1c:   48 89 c7                mov    %rax,%rdi
        0xb0, 0x00,                                 // 1f:   b0 00                   mov    $0x0,%al
        0xe8, 0x00, 0x00, 0x00, 0x00,               // 21:   e8 00 00 00 00          callq  26 <main+0x26>  // printf relocation

        0x48, 0xc7, 0xc0, 0x00, 0x00, 0x00, 0x00,   // 26:   48 c7 c0 00 00 00 00    mov    $0x0,%rax
        0xc9,                                       // 2d:   c9                      leaveq
        0xc3,                                       // 2e:   c3                      retq
    };

    set_current_section(".text");
    add_to_current_section(hello_world, sizeof(hello_world));

    // printf relocations
    Symbol *printf_symbol = get_or_add_symbol("printf");
    printf_symbol->binding = STB_GLOBAL;
    add_relocation(printf_symbol, R_X86_64_PLT32, 0x11);
    add_relocation(printf_symbol, R_X86_64_PLT32, 0x22);

    // .SL0 relocation
    Symbol *sl0_symbol = get_symbol(".SL0");
    if (!sl0_symbol) panic(".SL0 undefined");
    add_relocation(sl0_symbol, R_X86_64_PC32, 0x7);

    // .SL1 relocation
    Symbol *sl1_symbol = get_symbol(".SL1");
    if (!sl1_symbol) panic(".SL1 undefined");
    add_relocation(sl1_symbol, R_X86_64_PC32, 0x18);
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
        else {
            skip();
            panic("Don't know what do do with token %d", cur_token);
        }

        for (int i = 0; i < labels->length; i++) free(labels->elements[i]);
        free_list(labels);

        while (cur_token == TOK_EOL) next();
    }

    add_test_program();
}
