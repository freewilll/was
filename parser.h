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

typedef struct align_chunk {
    int alignment;
} AlignChunk;

typedef enum chunk_type {
    CT_CODE       = 1, // Code
    CT_DATA       = 2, // Data
    CT_ZERO       = 3, // This is a bunch of zeroes. data[] isn't used
    CT_ALIGN      = 4, // This is either a bunch of zeroes or NOPs, dependent on alignment and if it's in .text.. data[] isn't used
    CT_SIZE_EXPR  = 5, // A size expression to be evaluated in the second pass
} ChunkType;

typedef struct chunk {
    ChunkType type;
    int offset;
    union {
        CodeOrDataChunk   cdc;
        ZeroChunk         zec;
        AlignChunk        aic;
        SizeChunk         sic;
    };
    List *symbols; // Zero or more symbols associated with the address at this instruction
} Chunk;

#define CHUNK_SIZE(chunk) ( \
    ((chunk)->type == CT_CODE || (chunk)->type == CT_DATA) \
        ? (chunk)->cdc.using_primary ? (chunk)->cdc.primary->size : (chunk)->cdc.secondary->size \
        : (chunk)->zec.size \
    )

Chunk *parse_instruction_statement(void);
Chunk *parse_directive_statement(void);
void parse(void);
void init_parser(void);
void emit_code(void);

#endif
