#define MAX_IDENTIFIER_SIZE           1024
#define MAX_STRING_LITERAL_SIZE       4095

typedef struct string_literal {
    char *data;
    int size;
} StringLiteral;

// list.c
typedef struct list {
    int length;
    int allocated;
    void **elements;
} List;

List *new_list(int length);
void free_list(List *l);
void append_to_list(List *l, void *element);

// utils.c
void panic(char *format, ...);
void error(char *format, ...);

// was.c
void assemble(char *filename);

// lexer.c
enum {
    TOK_EOF = 1,
    TOK_EOL,
    TOK_INTEGER,
    TOK_FLOATING_POINT_NUMBER,
    TOK_STRING_LITERAL,
    TOK_LABEL,
    TOK_INSTRUCTION,
    TOK_IDENTIFIER,
    TOK_RPAREN,
    TOK_LPAREN,
    TOK_COMMA,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_DIRECTIVE_ALIGN,
    TOK_DIRECTIVE_BYTE,
    TOK_DIRECTIVE_COMM,
    TOK_DIRECTIVE_DATA,
    TOK_DIRECTIVE_FILE,
    TOK_DIRECTIVE_GLOBL,
    TOK_DIRECTIVE_LOCAL,
    TOK_DIRECTIVE_LONG,
    TOK_DIRECTIVE_QUAD,
    TOK_DIRECTIVE_SECTION,
    TOK_DIRECTIVE_SIZE,
    TOK_DIRECTIVE_STRING,
    TOK_DIRECTIVE_RODATA,
    TOK_DIRECTIVE_TEXT,
    TOK_DIRECTIVE_TYPE,
    TOK_DIRECTIVE_ULEB128,
    TOK_DIRECTIVE_WORD,
    TOK_DIRECTIVE_ZERO,
    TOK_DOT_SYMBOL,
    TOK_REGISTER,
    REG_RIP,
    REG_ST,
};

#define REG_BYTE 0x00
#define REG_WORD 0x10
#define REG_LONG 0x20
#define REG_QUAD 0x30
#define REG_XMM  0x40

extern char *cur_filename;             // Current filename being lexed
extern int cur_line;                   // Current line number being lexed
extern int cur_token;                  // Current token

void init_lexer(char *filename);
void next(void);
void free_lexer(void);
