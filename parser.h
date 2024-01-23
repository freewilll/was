#ifndef _PARSER_H
#define _PARSER_H

#include "instr.h"

typedef struct size_chunk {
    Node *size_expr;            // Expression to be evaluated in a .size statement
    Symbol *size_symbol;        // Symbol in a .size statement
} SizeChunk;

// Code (instructions) and data (.byte, .word, etc) chunks are treated in a similar
// way since they both can have relocations.
// It's a bit of a hack, a data chunk carries a lot of unnecessary baggage.
typedef struct code_or_data_chunk {
    int using_primary;
    Instructions *primary;
    Instructions *secondary;
} CodeOrDataChunk;

typedef struct zero_chunk {
    int size;
} ZeroChunk;

typedef enum chunk_type {
    CT_CODE       = 1, // Code
    CT_DATA       = 2, // Data
    CT_ZERO       = 3, // This is a bunch of zeroes. data[] isn't used
    CT_SIZE_EXPR  = 4, // A size expression to be evaluated in the second pass
} ChunkType;

typedef struct text_chunk {
    ChunkType type;
    union {
        CodeOrDataChunk   cdc; // Used by both code, data and zero
        ZeroChunk         zec; // Used by .zero sections
        SizeChunk         sic; // Used by .size sections
    };
    List *symbols;              // Zero or more symbols associated with the address at this instruction
} TextChunk;

#define TEXT_CHUNK_SIZE(text_chunk) \
    ((text_chunk)->type == CT_CODE || (text_chunk)->type == CT_DATA) \
        ? (text_chunk)->cdc.using_primary ? (text_chunk)->cdc.primary->size : (text_chunk)->cdc.secondary->size \
        : (text_chunk)->zec.size

TextChunk *parse_instruction_statement(void);

TextChunk *parse_directive_statement(void);
void parse(void);
void init_parser(void);
void emit_code(void);

#endif
