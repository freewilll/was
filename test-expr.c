#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr.h"
#include "instr.h"
#include "relocations.h"
#include "symbols.h"
#include "test-utils.h"
#include "utils.h"

static Node *run_expression_parser(char *input) {
    // Trick the lexer into thinking identifiers aren't instructions
    char *lexer_str = malloc(strlen(input) + 8);
    sprintf(lexer_str, ".size %s", input);
    init_lexer_from_string(lexer_str);
    next();

    return parse_expression();
}

static void test_expr(char *input, char *expected_symbol_name, long expected_number) {
    printf("%-60s", input);

    Node *root = run_expression_parser(input);

    if (!root->value) panic("Expression didn't evaluate to a value");

    if (root->value->number != expected_number)
        panic("Expected %d, got %d", expected_number, root->value->number);

    if (expected_symbol_name && !root->value->symbol)
        panic("Expected symbol %s, got none", expected_symbol_name);
    else if (expected_symbol_name && strcmp(expected_symbol_name, root->value->symbol->name))
        panic("Expected symbol %s, got %s", expected_symbol_name, root->value->symbol->name);
    else if (!expected_symbol_name && root->value->symbol)
        panic("Expected no symbol, got %s", root->value->symbol->name);

    printf("pass\n");
}

// Test expressions that can be evaluated directly
static void test_direct_expressions(void) {
    test_expr("+7",                     NULL,   7);
    test_expr("-7",                     NULL,  -7);
    test_expr("7",                      NULL,   7);
    test_expr("1 + 2",                  NULL,   3);
    test_expr("1 + 2 + 3",              NULL,   6);
    test_expr("1 - 2",                  NULL,  -1);
    test_expr("2 * 3",                  NULL,   6);
    test_expr("2 * -3",                 NULL,  -6);
    test_expr("-2 * 3",                 NULL,  -6);
    test_expr("6 / 2",                  NULL,   3);
    test_expr("1 + 2 * 3",              NULL,   7);
    test_expr("1 + 2 * 3 + 4",          NULL,  11);
    test_expr("2 * (1 + 2)",            NULL,   6);
    test_expr("2 * (1 + 2) * (3 + 4)",  NULL,  42);
    test_expr("foo",                    "foo",  0);
    test_expr("foo + 1",                "foo",  1);
    test_expr("foo - 1",                "foo", -1);
    test_expr("1 + foo",                "foo",  1);
    test_expr("-1 + foo",               "foo", -1);
    test_expr("1 + 2 + foo",            "foo",  3);
    test_expr("1 + foo + 2 * 3",        "foo",  7);
}

static void test_symbol_difference_expression(void) {
    printf("%-60s", "test_symbol_difference_expression");

    Node *root = run_expression_parser("foo - bar");

    if (root->value) panic("Unexpected value");
    if (root->operation != OP_SUBTRACT) panic("Expected a subtraction operation");

    if (!root->left || !root->left->value || !root->left->value->symbol || strcmp(root->left->value->symbol->name, "foo"))
        panic("Expected LHS foo");
    if (!root->right || !root->right->value || !root->right->value->symbol || strcmp(root->right->value->symbol->name, "bar"))
        panic("Expected RHS bar");

    root->right->value->symbol->section_index = 1;
    root->left->value->symbol->section_index = 1;

    // Check foo - bar
    root->left->value->symbol->value = 0x10;
    root->right->value->symbol->value = 0x02;
    Value value = evaluate_node(root, 0xff);
    if (value.number != 0xe) panic("Expected 0xe, got %#lx", value.number);

    // Check . - bar
    root->left->value->symbol->name = ".";
    root->right->value->symbol->value = 0x02;
    value = evaluate_node(root, 0x10);
    if (value.number != 0xe) panic("Expected 0xe, got %#lx", value.number);

    // Check foo - .
    root->left->value->symbol->name = "foo";
    root->right->value->symbol->name = ".";
    root->left->value->symbol->value = 0x10;
    value = evaluate_node(root, 0x02);
    if (value.number != 0xe) panic("Expected 0xe, got %#lx", value.number);

    printf("pass\n");
}

int main() {
    init_tests();
    test_direct_expressions();
    test_symbol_difference_expression();
}
