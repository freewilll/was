#include <stdio.h>

#include "elf.h"
#include "lexer.h"
#include "parser.h"
#include "was.h"

void assemble(char *input_filename, char *output_filename) {
    init_lexer(input_filename);
    init_elf();
    parse();
    finish_elf(output_filename);
    free_lexer();
}
