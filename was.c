#include <stdio.h>

#include "was.h"
#include "elf.h"

void assemble(char *input_filename, char *output_filename) {
    init_lexer(input_filename);
    parse();
    write_elf(output_filename);
    free_lexer();
}
