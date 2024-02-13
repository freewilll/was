#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwarf.h"
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

    char *lexer_str = malloc(strlen(input) + 8);
    sprintf(lexer_str, ".data; %s", input);
    init_lexer_from_string(lexer_str);
    init_parser();
    init_dwarf();

    Section *section = get_section(".data");
    section->size = 0;
    section->chunks = NULL;

    parse_directive_statement();
    next();

    while (cur_token != TOK_EOF) {
        parse_directive_statement();
        while (cur_token == TOK_EOL) next();
    }

    if (section->chunks) emit_section_code(section);
    vassert_section_data(section, ap);

    printf("pass\n");
}

int main() {
    init_tests();

    test_assembly(".byte 1",  0x01, END);
    test_assembly(".word 1",  0x01, 0x00, END);
    test_assembly(".value 1", 0x01, 0x00, END);
    test_assembly(".long 1",  0x01, 0x00, 0x00, 0x00, END);
    test_assembly(".quad 1",  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, END);

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

    test_assembly(".align 1; .byte 2", 0x02, END);
    test_assembly(".byte 1; .align 1; .byte 2", 0x01, 0x02, END);
    test_assembly(".byte 1; .byte 2; .align 2; .byte 3", 0x01, 0x02, 0x03, END);

    test_assembly(".byte 1; .align 1; .byte 2", 0x01, 0x02, END);
    test_assembly(".byte 1; .align 2; .byte 2", 0x01, 0x00, 0x02, END);
    test_assembly(".byte 1; .align 4; .byte 2", 0x01, 0x00, 0x00, 0x00, 0x02, END);
    test_assembly(".byte 1; .align 8; .byte 2", 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, END);

    test_assembly(".sleb128 0",        0x00, END);
    test_assembly(".sleb128 1",        0x01, END);
    test_assembly(".sleb128 63",       0x3f, END);
    test_assembly(".sleb128 64",       0xc0, 0x00, END);
    test_assembly(".sleb128 65",       0xc1, 0x00, END);
    test_assembly(".sleb128 127",      0xff, 0x00, END);
    test_assembly(".sleb128 128",      0x80, 0x01, END);
    test_assembly(".sleb128 1000",     0xe8, 0x07, END);
    test_assembly(".sleb128 10000",    0x90, 0xce, 0x00, END);
    test_assembly(".sleb128 100000",   0xa0, 0x8d, 0x06, END);
    test_assembly(".sleb128 1000000",  0xc0, 0x84, 0x3d, END);
    test_assembly(".sleb128 10000000", 0x80, 0xad, 0xe2, 0x04, END);

    test_assembly(".sleb128 -1",        0x7f, END);
    test_assembly(".sleb128 -63",       0x41, END);
    test_assembly(".sleb128 -64",       0x40, END);
    test_assembly(".sleb128 -65",       0xbf, 0x7f, END);
    test_assembly(".sleb128 -127",      0x81, 0x7f, END);
    test_assembly(".sleb128 -128",      0x80, 0x7f, END);
    test_assembly(".sleb128 -1000",     0x98, 0x78, END);
    test_assembly(".sleb128 -10000",    0xf0, 0xb1, 0x7f, END);
    test_assembly(".sleb128 -100000",   0xe0, 0xf2, 0x79, END);
    test_assembly(".sleb128 -1000000",  0xc0, 0xfb, 0x42, END);
    test_assembly(".sleb128 -10000000", 0x80, 0xd3, 0x9d, 0x7b, END);

    test_assembly(".uleb128 0",        0x00, END);
    test_assembly(".uleb128 1",        0x01, END);
    test_assembly(".uleb128 127",      0x7f, END);
    test_assembly(".uleb128 128",      0x80, 0x01, END);
    test_assembly(".uleb128 1000",     0xe8, 0x07, END);
    test_assembly(".uleb128 10000",    0x90, 0x4e, END);
    test_assembly(".uleb128 100000",   0xa0, 0x8d, 0x06, END);
    test_assembly(".uleb128 1000000",  0xc0, 0x84, 0x3d, END);
    test_assembly(".uleb128 10000000", 0x80, 0xad, 0xe2, 0x04, END);
}
