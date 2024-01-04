#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "lexer.h"
#include "opcodes.h"
#include "parser.h"
#include "relocations.h"
#include "symbols.h"
#include "utils.h"
#include "test-utils.h"

#define END -1

void test_assembly(char *input, ...) {
    va_list ap;
    va_start(ap, input);

    printf("%-60s", input);

    char *lexer_str = malloc(strlen(input) + 5);
    sprintf(lexer_str, ".data\n%s", input);
    init_lexer_from_string(lexer_str);

    parse_directive_statement();
    next();
    ElfSection *section = get_current_section();
    section->size = 0;

    parse_directive_statement();
    assert_section(section, ap);

    printf("pass\n");
}

int main() {
    init_opcodes();
    init_symbols();
    init_relocations();

    test_assembly(".byte 1", 0x01, END);
    test_assembly(".word 1", 0x01, 0x00, END);
    test_assembly(".long 1", 0x01, 0x00, 0x00, 0x00, END);
    test_assembly(".quad 1", 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly(".byte 1",    0x01, END);
    test_assembly(".byte 127",  0x7f, END);
    test_assembly(".byte 128",  0x80, END);
    test_assembly(".byte -128", 0x80, END);
    test_assembly(".byte 254",  0xfe, END);
    test_assembly(".byte -2",   0xfe, END);
    test_assembly(".byte -1",   0xff, END);
    test_assembly(".byte 255",  0xff, END);

    test_assembly(".word 1",      0x01, 0x00, END);
    test_assembly(".word 32767",  0xff, 0x7f, END);
    test_assembly(".word 32768",  0x00, 0x80, END);
    test_assembly(".word -32768", 0x00, 0x80, END);
    test_assembly(".word 65534",  0xfe, 0xff, END);
    test_assembly(".word -2",     0xfe, 0xff, END);
    test_assembly(".word -1",     0xff, 0xff, END);
    test_assembly(".word 65535",  0xff, 0xff, END);

    test_assembly(".long 1",           0x01, 0x00, 0x00, 0x00, END);
    test_assembly(".long 2147483647",  0xff, 0xff, 0xff, 0x7f, END);
    test_assembly(".long 2147483648",  0x00, 0x00, 0x00, 0x80, END);
    test_assembly(".long -2147483648", 0x00, 0x00, 0x00, 0x80, END);
    test_assembly(".long 4294967294",  0xfe, 0xff, 0xff, 0xff, END);
    test_assembly(".long -2",          0xfe, 0xff, 0xff, 0xff, END);
    test_assembly(".long -1",          0xff, 0xff, 0xff, 0xff, END);
    test_assembly(".long 4294967295",  0xff, 0xff, 0xff, 0xff, END);

    test_assembly(".quad 1",                    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly(".quad 9223372036854775807",  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, END);
    test_assembly(".quad 9223372036854775808",  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, END);
    test_assembly(".quad -9223372036854775808", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, END);
    test_assembly(".quad 18446744073709551614", 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, END);
    test_assembly(".quad -2",                   0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, END);
    test_assembly(".quad -1",                   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, END);
    test_assembly(".quad 18446744073709551615", 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, END);

    test_assembly(".zero 0", END);
    test_assembly(".zero 1", 0x00, END);
    test_assembly(".zero 8", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly(".zero 12", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly(".string \"\"",     0x00, END);
    test_assembly(".string \"abc\"",  0x61, 0x62, 0x63, 0x00, END);
    test_assembly(".string \"\\\"\"", 0x22, 0x00, END);
    test_assembly(".string \"'\"",    0x27, 0x00, END);
}
