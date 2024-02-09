#include <stdio.h>

#include "elf.h"
#include "lexer.h"
#include "opcodes.h"
#include "parser.h"
#include "relocations.h"
#include "was.h"

void assemble(char *input_filename, char *output_filename) {
    init_lexer(input_filename);
    init_sections();
    init_symbols();
    init_default_sections();
    init_relocations();
    init_opcodes();
    init_parser();
    parse();
    emit_code();
    make_section_indexes();
    make_symbols_section();
    make_rela_sections();
    finish_elf(output_filename);
    free_lexer();
}
