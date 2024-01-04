#include <stdio.h>

#include "elf.h"
#include "lexer.h"
#include "opcodes.h"
#include "parser.h"
#include "was.h"

void assemble(char *input_filename, char *output_filename) {
    init_lexer(input_filename);
    init_elf();
    init_opcodes();
    init_parser();
    parse();
    emit_code();
    finish_elf(output_filename);
    free_lexer();
}
