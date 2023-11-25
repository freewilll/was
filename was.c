#include <stdio.h>

#include "was.h"

void assemble(char *filename) {
    init_lexer(filename);

    while (cur_token != TOK_EOF) {
        next();
    }

    free_lexer();
}
