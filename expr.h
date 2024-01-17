#ifndef _EXPR_H
#define _EXPR_H

#include "symbols.h"

typedef enum operation {
    OP_ADD       = 1,
    OP_SUBTRACT  = 2,
    OP_MULTIPLY  = 3,
    OP_DIVIDE    = 4,
} Operation;

typedef struct value {
    Symbol *symbol; // Optional symbol
    long number;    // Optional number. If symbol is set, it's an offset
} Value;

typedef struct node Node;

// A Node either has a value or an operation + left + right
typedef struct node {
    Value *value;        // Optional value
    Operation operation; // Optional operation
    Node *left;          // Optional expression
    Node *right;         // Optional expression
} Node;

Node *parse_expression(void);
Value evaluate_node(Node *node, long current_offset);

#endif
