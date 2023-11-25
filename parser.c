#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
            printf("TODO: .file\n");
            skip();
            break;

        case TOK_DIRECTIVE_GLOBL:
            printf("TODO: .globl\n");
            skip();
            break;

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
            printf("TODO: .section\n");
            skip();
            break;

        case TOK_DIRECTIVE_SIZE:
            printf("TODO: .size\n");
            skip();
            break;

        case TOK_DIRECTIVE_STRING:
            printf("TODO: .string\n");
            skip();
            break;

        case TOK_DIRECTIVE_RODATA:
            printf("TODO: .rodata\n");
            skip();
            break;

        case TOK_DIRECTIVE_TEXT:
            printf("TODO: .text\n");
            skip();
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
    if (cur_token != TOK_INSTRUCTION) {
        printf("TODO: label at end without instruction\n");
        skip();
        return;
    }

    printf("TODO: instruction %s\n", cur_identifier);
    next();

    skip();
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

        // Parse statement
        if (cur_token >= TOK_DIRECTIVE_ALIGN && cur_token <= TOK_DIRECTIVE_ZERO)
            parse_directive_statement();
        else
            parse_instruction_statement();

        // TODO use labels
        for (int i = 0; i < labels->length; i++) free(labels->elements[i]);
        free_list(labels);

        while (cur_token == TOK_EOL) next();
    }
}

