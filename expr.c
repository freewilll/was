#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr.h"
#include "lexer.h"
#include "utils.h"

#define NODE_HAS_SYMBOL(node) ((node)->value && (node)->value->symbol)
#define NODE_IS_NUMERIC(node) ((node)->value && !(node)->value->symbol)

static Node *parse(int level);

static void free_node(Node *node) {
    if (!node) return;
    if (node->value) free(node->value);
    free(node);
}

static Node *make_integer_node(long value) {
    Node *node = calloc(1, sizeof(Node));
    node->value = calloc(1, sizeof(Value));
    node->value->number = value;
    return node;
}

static Node *make_zero_node(void) {
    return make_integer_node(0);
}

// Make a node with a value with a symbol in it
static Node *make_symbol_node(void) {
    Node *node = calloc(1, sizeof(Node));
    node->value = calloc(1, sizeof(Value));
    node->value->symbol = get_or_add_symbol(strdup(cur_identifier));
    return node;
}

// Returns a malloc'd node; left is freed.
static Node *parse_binary_expression(Node *left, Operation operation, int token) {
    Node *node;

    next();
    Node *right = parse(token);

    if (NODE_HAS_SYMBOL(left) && NODE_HAS_SYMBOL(right)) {
        // Evalulate symbol - symbol
        if (operation != OP_SUBTRACT) error("Invalid operation on two symbols");

        if (left->value->symbol->section != right->value->symbol->section)
            error("Cannot subtract two symbols in different sections");

        // Create an operation node
        node = calloc(1, sizeof(Node));
        node->operation = OP_SUBTRACT;
        node->left = left;
        node->right = right;
    }

    else if (NODE_IS_NUMERIC(left) || NODE_IS_NUMERIC(right) && (NODE_HAS_SYMBOL(left) || NODE_HAS_SYMBOL(right))) {
        // Evalulate
        // - symbol <op> number
        // - number <op> number
        // - number <op> symbol

        node = left;
        if (!node->value->symbol) node->value->symbol = right->value->symbol;

        if (operation == OP_DIVIDE && !right->value->number) error("Divide by zero");

        switch (operation)  {
            case OP_ADD:      node->value->number += right->value->number; break;
            case OP_SUBTRACT: node->value->number -= right->value->number; break;
            case OP_MULTIPLY: node->value->number *= right->value->number; break;
            case OP_DIVIDE:   node->value->number /= right->value->number; break;
            default:
                panic("Unknown operation %d", operation);
        }

        free_node(right);
    }

    else {
        // Create an operation node
        node = calloc(1, sizeof(Node));
        node->operation = operation;
        node->left = left;
        node->right = right;
    }

    return node;
}

// Returns a malloc'd tree of nodes & values
static Node *parse(int level) {
    Node *node;

    switch (cur_token) {
        case TOK_PLUS:
            next();
            node = parse(level);
            break;

        case TOK_MINUS: {
            next();
            Node *subnode = parse(TOK_DOLLAR); // TOK_DOLLAR has higher precedence than all operators

            if (NODE_IS_NUMERIC(subnode)) {
                // Evaluate if possible
                node = subnode;
                node->value->number = -node->value->number;
            }
            else {
                // Return an operation node
                node = calloc(1, sizeof(Node));
                node->operation = OP_SUBTRACT;
                node->left = make_zero_node();
                node->right = subnode;
            }

            break;
        }

        case TOK_INTEGER: {
            node = make_integer_node(cur_long);
            next();
            break;
        }

        case TOK_IDENTIFIER: {
            node = make_symbol_node();
            next();
            break;
        }

        case TOK_LPAREN:
            next();
            node = parse(TOK_PLUS);
            consume(TOK_RPAREN, ")");
            break;

        default:
            error("Unexpected token %d in expression", cur_token);
    }

    while (cur_token >= level) {
        switch (cur_token) {
            case TOK_PLUS:     node = parse_binary_expression(node, OP_ADD,      TOK_MULTIPLY); break;
            case TOK_MINUS:    node = parse_binary_expression(node, OP_SUBTRACT, TOK_MULTIPLY); break;
            case TOK_MULTIPLY: node = parse_binary_expression(node, OP_MULTIPLY, TOK_MULTIPLY); break;
            case TOK_DIVIDE:   node = parse_binary_expression(node, OP_DIVIDE,   TOK_MULTIPLY); break;
            default:
                return node; // Bail once we hit something unknown
        }
    }

    return node;
}

// Parse a simple arithmetic expression.
// It can have either one symbol or a subtraction of two symbols.
// Anything further is untested and likely not to work.
Node *parse_expression() {
    return parse(TOK_PLUS);
}

static Value evaluate(Node *node, long current_offset) {
    if (node->value) return *node->value;

    Value result = {0};
    Value left = evaluate(node->left, current_offset);
    Value right = evaluate(node->right, current_offset);

    if (node->operation != OP_SUBTRACT)
        panic("Interal error: unimplemented operation %d", node->operation);

    if (!left.symbol || !right.symbol)
        panic("Interal error: can only subtract two symbols");

    if (left.symbol->section && right.symbol->section && left.symbol->section != right.symbol->section)
        panic("Mismatch in section");

    int right_is_dot = right.symbol->name[0] == '.' && !right.symbol->name[1];
    int left_is_dot =  left. symbol->name[0] == '.' && !left. symbol->name[1];

    long right_offset = right_is_dot ? current_offset : right.symbol->value;
    long left_offset  = left_is_dot  ? current_offset : left. symbol->value;

    result.number = left_offset - right_offset;

    return result;
}

// Evaluate a tree of nodes. The only thing implemented right now is
// node-node subtraction.
Value evaluate_node(Node *node, long current_offset) {
    return evaluate(node, current_offset);
}
