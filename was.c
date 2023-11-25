#include <stdio.h>

#include "was.h"

void assemble(char *filename) {
    init_lexer(filename);
    parse();
    free_lexer();
}
