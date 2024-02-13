#ifndef _PARSER_H
#define _PARSER_H

#include "instr.h"

// Code (instructions) and data (.byte, .word, etc) chunks are treated in a similar
// way since they both can have relocations.
// It's a bit of a hack, a data chunk carries a lot of unnecessary baggage.
typedef struct code_chunk {
    int using_primary;
    Instructions *primary;
    Instructions *secondary;
} CodeChunk;

typedef struct data_chunk {
    char *data;         // Either data or expr has a value
    Node *expr;
    int size;
} DataChunk;

typedef struct zero_chunk {
    int size;
} ZeroChunk;

typedef struct align_chunk {
    int alignment;
} AlignChunk;

typedef struct size_chunk {
    Node *size_expr;            // Expression to be evaluated in a .size statement
    Symbol *size_symbol;        // Symbol in a .size statement
} SizeChunk;

typedef struct loc_chunk {
    int file_index;
    int line_number;
} LocChunk;

typedef struct label_chunk {
    Symbol *symbol;
} LabelChunk;

typedef enum chunk_type {
    CT_CODE       = 1, // Code, i.e. instructions
    CT_DATA       = 2, // Data, coming from .byte, .word, .long, .quad or .string, evaluated in the second pass
    CT_ZERO       = 3, // This is a bunch of zeroes.
    CT_ALIGN      = 4, // This is either a bunch of zeroes or NOPs, dependent on alignment and if it's in .text.
    CT_SIZE_EXPR  = 5, // A size expression to be evaluated in the second pass; doesn't have a payload
    CT_LOC        = 6, // A loc doesn't have a payload
    CT_LABEL      = 7, // A label; doesn't have a payload
} ChunkType;

typedef struct chunk {
    ChunkType type;
    int offset;
    union {
        CodeChunk   coc;
        DataChunk   dac;
        ZeroChunk   zec;
        AlignChunk  aic;
        SizeChunk   sic;
        LocChunk    loc;
        LabelChunk  lac;
    };
} Chunk;

#define CODE_CHUNK_SIZE(chunk) ((chunk)->coc.using_primary ? (chunk)->coc.primary->size : (chunk)->coc.secondary->size)
#define ZERO_CHUNK_SIZE(chunk) ((chunk)->zec.size)
#define DATA_CHUNK_SIZE(chunk) ((chunk)->dac.size)

#define CHUNK_SIZE(chunk) ( \
      ((chunk)->type == CT_CODE) ? CODE_CHUNK_SIZE(chunk) \
    : ((chunk)->type == CT_DATA) ? DATA_CHUNK_SIZE(chunk) \
    : ((chunk)->type == CT_ZERO) ? ZERO_CHUNK_SIZE(chunk) \
    : 0 \
)

Chunk *parse_instruction_statement(void);
Chunk *parse_directive_statement(void);
void parse(void);
void init_parser(void);
void emit_section_code(Section *section);

#endif
