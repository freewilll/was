#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lexer.h"
#include "utils.h"
#include "was.h"

static char *input;             // Input file data
static char *input_end;         // Input file data
static char *ip;                // Input pointer to currently lexed char.
static int seen_instruction;    // Currently lexing labels or instructions
static int seen_directive;      // Currently lexing a directive

char *cur_filename;
int cur_line;                       // Current line

int cur_token;                      // Current token
char *cur_identifier;               // Current identifier
int cur_register;                   // Current register id
int cur_register_alt_8bit;          // Set to 1 for spl, bpl, sil, dil 8-bit registers
long cur_long;                      // Current integer
StringLiteral cur_string_literal;   // Current string literal

void free_lexer(void) {
    free(cur_identifier);
    free(cur_string_literal.data);
    free(input);
}

static void start_lexer(void) {
    ip = input;
    cur_line = 1;
    cur_identifier = malloc(MAX_IDENTIFIER_SIZE);
    cur_string_literal.data = malloc(MAX_STRING_LITERAL_SIZE * 4);
    seen_instruction = 0;
    seen_directive = 0;
    cur_register = 0;

    next();
}

void init_lexer(char *filename) {
    cur_filename = filename;

    FILE *f  = fopen(filename, "r");

    if (f == 0) {
        perror(filename);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    int input_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    input = malloc(input_size + 1);
    int read = fread(input, 1, input_size, f);
    if (read != input_size) {
        printf("Unable to read input file\n");
        exit(1);
    }

    input[input_size] = 0;
    input_end = input + input_size;
    fclose(f);

    start_lexer();
}

void init_lexer_from_string(char *string) {
    input = string;
    input_end = input + strlen(string);

    start_lexer();
}

static void skip_whitespace(void) {
    while (ip < input_end) {
        if (*ip == ' ' || *ip == '\t' || *ip == '\f' || *ip == '\v')
            ip++;
        else return;
    }
}

static void skip_comments(void) {
    if (*ip != '#') return;
    while (*ip != '\n' && ip < input_end) ip++;
}

static void lex_octal_literal(void) {
    cur_long = 0;
    int c = 0;
    while (*ip >= '0' && *ip <= '7') {
        cur_long = cur_long * 8 + *ip - '0';
        ip++;
        c++;
        if (c == 3) break;
    }
}

static void lex_integer(void) {
    cur_token = TOK_INTEGER;

    int is_octal = 0;
    int is_decimal = 0;
    int is_hex = 0;
    int base;

    if (*ip == '0') {
        if (ip[1] == 'x') {
            ip += 2;
            is_hex = 1;
            base = 16;
        }
        else {
            is_octal = 1;
            ip += 1;
            base = 8;
        }
    }
    else {
        is_decimal = 1;
        base = 10;
    }

    cur_long = 0;
    while (
        ip < input_end &&
            (is_octal && (*ip >= '0' && *ip <= '7')) ||
            (is_decimal && (*ip >= '0' && *ip <= '9')) ||
            (is_hex && ((*ip >= '0' && *ip <= '9') || (*ip >= 'a' && *ip <= 'f') || (*ip >= 'A' && *ip <= 'Z')))
        ) {

        int digit = is_hex
            ? (
                *ip >= 'a'
                ? *ip - 'a' + 10
                : *ip >= 'A'
                    ? *ip - 'A' + 10
                : *ip - '0'
              )
            : *ip - '0';

        cur_long = cur_long * base + digit;

        ip++;
    }
}

static void lex_string_literal(void) {
    int size = 0;

    ip += 1;
    char *data = cur_string_literal.data;
    while (input_end - ip >= 1 && *ip != '"') {
        if (*ip != '\\') data[(size)++] = *ip++;
        else if (input_end - ip >= 2 && *ip == '\\') {
                 if (ip[1] == '\'') { ip += 2; data[(size)++] = '\''; }
            else if (ip[1] == '"' ) { ip += 2; data[(size)++] = '\"'; }
            else if (ip[1] == '?' ) { ip += 2; data[(size)++] = '?'; }
            else if (ip[1] == '\\') { ip += 2; data[(size)++] = '\\'; }
            else if (ip[1] == 'a' ) { ip += 2; data[(size)++] = 7; }
            else if (ip[1] == 'b' ) { ip += 2; data[(size)++] = 8; }
            else if (ip[1] == 'f' ) { ip += 2; data[(size)++] = 12; }
            else if (ip[1] == 'n' ) { ip += 2; data[(size)++] = 10; }
            else if (ip[1] == 'r' ) { ip += 2; data[(size)++] = 13; }
            else if (ip[1] == 't' ) { ip += 2; data[(size)++] = 9; }
            else if (ip[1] == 'v' ) { ip += 2; data[(size)++] = 11; }
            else if (ip[1] == 'e' ) { ip += 2; data[(size)++] = 27; }
            else if (ip[1] >= '0' && ip[1] <= '7' ) {
                ip++;
                lex_octal_literal();
                data[(size)++] = cur_long & 0xff;
            }
            else error("Unknown \\ escape in string literal");
        }

        data[size] = 0;
    }

    if (*ip != '"') error("Expecting terminating \" in string literal");
    ip++;

    if (size >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);

    data[size] = 0;

    cur_token = TOK_STRING_LITERAL;

    cur_string_literal.size = size + 1;
}

#define FIND_REG(regs, outcome, alt) \
    for (int i = 0; i < 16; i++) { \
        if (!strcmp(name, regs[i])) { \
            cur_register = outcome + i; \
            cur_register_alt_8bit = alt; \
           return; \
        }  \
    }

static void parse_register(void) {
    char name[6];

    #define MAX_REGISTER_SIZE sizeof(name) - 1
    int j = 0;
    while (((*ip >= 'a' && *ip <= 'z') || (*ip >= '0' && *ip <= '9')) && ip < input_end) {
        if (j == MAX_REGISTER_SIZE) panic("Exceeded maximum register size %d", MAX_REGISTER_SIZE);
        name[j] = *ip;
        j++;
        ip++;
    }
    name[j] = 0;

    // https://wiki.osdev.org/X86-64_Instruction_Encoding#Registers
    char *regs0a[16] = {"al",   "cl",   "dl",   "bl",   "ah",   "ch",   "dh",   "bh",   "r8b",  "r9b",  "r10b",  "r11b",  "r12b",  "r13b",  "r14b",  "r15b"};
    char *regs0b[16] = {"",      "",    "",     "",     "spl",  "bpl",  "sil",  "dil",   "",    "",     "",      "",      "",      "",      "",      ""};
    char *regs1[16]  = {"ax",   "cx",   "dx",   "bx",   "sp",   "bp",   "si",   "di",   "r8w",  "r9w",  "r10w",  "r11w",  "r12w",  "r13w",  "r14w",  "r15w"};
    char *regs2[16]  = {"eax",  "ecx",  "edx",  "ebx",  "esp",  "ebp",  "esi",  "edi",  "r8d",  "r9d",  "r10d",  "r11d",  "r12d",  "r13d",  "r14d",  "r15d"};
    char *regs3[16]  = {"rax",  "rcx",  "rdx",  "rbx",  "rsp",  "rbp",  "rsi",  "rdi",  "r8",   "r9",   "r10",   "r11",   "r12",   "r13",   "r14",   "r15"  };
    char *regs4[16]  = {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"};

    FIND_REG(regs0a, REG_BYTE, 0);
    FIND_REG(regs0b, REG_BYTE, 1);
    FIND_REG(regs1,  REG_WORD, 0);
    FIND_REG(regs1,  REG_WORD, 0);
    FIND_REG(regs2,  REG_LONG, 0);
    FIND_REG(regs3,  REG_QUAD, 0);
    FIND_REG(regs4,  REG_XMM,  0);

    if (!strcmp(name, "rip")) {
        cur_register = REG_RIP;
        return;
    }

    if (!strcmp(name, "st"))  {
        if (ip < input_end - 2 && ip[0] == '(' && ip[2] == ')' && ip[1] >= '0' && ip[1] <= '7') {
            cur_register = REG_ST + ip[1] - '0';
            ip += 3;
        }
        else {
            cur_register = REG_ST; // %ST is a shortcut for %ST(0)
        }

        return;
    }

    error("Unknown register %%%s", name);
}

// Lexer. Lex a next token or TOK_EOF if the file is ended
void next(void) {
    // Increment the line number after consuming a newline
    if (cur_token == TOK_EOL) cur_line++;

    while (ip < input_end) {
        skip_whitespace();
        skip_comments();

        if (ip >= input_end) break;

        char c1 = ip[0];
        char c2 = ip[1];

        if (c1 == '/' && c2 == '/') {
            // Skip comments
            while (*ip != '\n' && ip < input_end) ip++;
            continue;
        }

             if (c1 == '('  )  { ip += 1;  cur_token = TOK_LPAREN;   }
        else if (c1 == ')'  )  { ip += 1;  cur_token = TOK_RPAREN;   }
        else if (c1 == ','  )  { ip += 1;  cur_token = TOK_COMMA;    }
        else if (c1 == '+'  )  { ip += 1;  cur_token = TOK_PLUS;     }
        else if (c1 == '-'  )  { ip += 1;  cur_token = TOK_MINUS;    }
        else if (c1 == '*'  )  { ip += 1;  cur_token = TOK_MULTIPLY; }
        else if (c1 == '/'  )  { ip += 1;  cur_token = TOK_DIVIDE;   }
        else if (c1 == '$'  )  { ip += 1;  cur_token = TOK_DOLLAR;   }

        // Instruction separator
        else if (c1 == ';') {
            ip += 1;
            cur_token = TOK_EOL;
            seen_instruction = 0;
            seen_directive = 0;
        }

        // Newline
        else if (c1 == '\n') {
            ip += 1;
            cur_token = TOK_EOL;
            seen_instruction = 0;
            seen_directive = 0;
        }

        // Decimal literal
        else if (c1 >= '0' && c1 <= '9') {
            lex_integer();
        }

        // String literal
        else if ((c1 == '"') || (input_end - ip >= 2 && c1 == 'L' && c2 == '"')) {
            lex_string_literal();
        }

        else if (c1 == '%') {
            // Register
            cur_token = TOK_REGISTER;
            ip++;
            parse_register();
        }

        // Label, instruction, directive or identifier
        else if (((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_' || c1 == '.') || c1 == '@') {

            int j = 0;
            while (
                    ((*ip >= 'a' && *ip <= 'z') ||
                        (*ip >= 'A' && *ip <= 'Z') ||
                        (*ip >= '0' && *ip <= '9') ||
                        (*ip == '_' ||
                        (*ip == '@') ||
                        (*ip == ':') ||
                        (*ip == '.'))) && ip < input_end) {

                if (j == MAX_IDENTIFIER_SIZE) panic("Exceeded maximum identifier size %d", MAX_IDENTIFIER_SIZE);
                cur_identifier[j] = *ip;
                j++;
                ip++;
            }

            if (!j) panic("cur_identifier is unexpectedly empty");

            int is_label = cur_identifier[j - 1] == ':';

            cur_identifier[j] = 0;

            if (!seen_directive && !is_label && cur_identifier[0] == '.') {
                // Parse directive or identifier starting with dot
                     if (!strcmp(cur_identifier, ".align"   )) { cur_token = TOK_DIRECTIVE_ALIGN;   seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".byte"    )) { cur_token = TOK_DIRECTIVE_BYTE;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".comm"    )) { cur_token = TOK_DIRECTIVE_COMM;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".data"    )) { cur_token = TOK_DIRECTIVE_DATA;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".file"    )) { cur_token = TOK_DIRECTIVE_FILE;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".globl"   )) { cur_token = TOK_DIRECTIVE_GLOBL;   seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".loc"     )) { cur_token = TOK_DIRECTIVE_LOC;     seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".local"   )) { cur_token = TOK_DIRECTIVE_LOCAL;   seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".long"    )) { cur_token = TOK_DIRECTIVE_LONG;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".quad"    )) { cur_token = TOK_DIRECTIVE_QUAD;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".section" )) { cur_token = TOK_DIRECTIVE_SECTION; seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".size"    )) { cur_token = TOK_DIRECTIVE_SIZE;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".sleb128" )) { cur_token = TOK_DIRECTIVE_SLEB128; seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".string"  )) { cur_token = TOK_DIRECTIVE_STRING;  seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".text"    )) { cur_token = TOK_DIRECTIVE_TEXT;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".type"    )) { cur_token = TOK_DIRECTIVE_TYPE;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".uleb128" )) { cur_token = TOK_DIRECTIVE_ULEB128; seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".word"    )) { cur_token = TOK_DIRECTIVE_WORD;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".value"   )) { cur_token = TOK_DIRECTIVE_VALUE;   seen_directive = 1; }
                else if (!strcmp(cur_identifier, ".zero"    )) { cur_token = TOK_DIRECTIVE_ZERO;    seen_directive = 1; }
                else if (!strcmp(cur_identifier, "."        )) { cur_token = TOK_DOT_SYMBOL;        seen_directive = 1; }
                else {
                    cur_token = TOK_IDENTIFIER;
                }
            }

            else if (is_label) {
                // Label
                cur_token = TOK_LABEL;
                cur_identifier[j - 1] = 0;
            }

            else {
                if (!seen_directive && !seen_instruction) {
                    // Instruction
                    cur_token = TOK_INSTRUCTION;
                    seen_instruction = 1;
                }
                else {
                    // Identifier
                    cur_token = TOK_IDENTIFIER;
                }
            }
        }

        else
            error("Unknown token %c (%d)", *ip, *ip);

        return;
    }

    cur_token = TOK_EOF;
}

void expect(int token, char *what) {
    if (cur_token != token) error("Expected %s", what);
}

void consume(int token, char *what) {
    expect(token, what);
    next();
}
