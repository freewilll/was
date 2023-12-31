#ifndef _LEXER_H
#define _LEXER_H

#define MAX_IDENTIFIER_SIZE           1024
#define MAX_STRING_LITERAL_SIZE       4095

typedef struct string_literal {
    char *data;
    int size;
} StringLiteral;

enum {
    TOK_EOF = 1,
    TOK_EOL,
    TOK_INTEGER,
    TOK_FLOATING_POINT_NUMBER,
    TOK_STRING_LITERAL,
    TOK_LABEL,
    TOK_IDENTIFIER,
    TOK_DIRECTIVE_ALIGN,
    TOK_DIRECTIVE_BYTE,
    TOK_DIRECTIVE_COMM,             // 10
    TOK_DIRECTIVE_DATA,
    TOK_DIRECTIVE_FILE,
    TOK_DIRECTIVE_GLOBL,
    TOK_DIRECTIVE_LOCAL,
    TOK_DIRECTIVE_LONG,
    TOK_DIRECTIVE_QUAD,
    TOK_DIRECTIVE_SECTION,
    TOK_DIRECTIVE_SIZE,
    TOK_DIRECTIVE_STRING,
    TOK_DIRECTIVE_RODATA,           // 20
    TOK_DIRECTIVE_TEXT,
    TOK_DIRECTIVE_TYPE,
    TOK_DIRECTIVE_ULEB128,
    TOK_DIRECTIVE_WORD,
    TOK_DIRECTIVE_ZERO,
    TOK_DOT_SYMBOL,
    TOK_INSTRUCTION,
    TOK_REGISTER,
    TOK_RPAREN,
    TOK_LPAREN,                     // 30
    TOK_COMMA,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DOLLAR,
};

// The order is used for size determination
#define REG_BYTE 0x00
#define REG_WORD 0x10
#define REG_LONG 0x20
#define REG_QUAD 0x30
#define REG_XMM  0x40
#define REG_ST   0x50
#define REG_RIP  0x60

extern char *cur_filename;      // Current filename being lexed
extern int cur_line;            // Current line number being lexed

extern int cur_token;                       // Current token
extern char *cur_identifier;                // Current identifier
extern int cur_register;                    // Current register id
extern long cur_long;                       // Current integer
extern StringLiteral cur_string_literal;    // Current string literal

void free_lexer(void);
void init_lexer(char *filename);
void init_lexer_from_string(char *string);
void next(void);
void expect(int token, char *what);
void consume(int token, char *what);

#endif
