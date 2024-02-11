#include <stdio.h>

#include "branches.h"
#include "dwarf.h"
#include "elf.h"
#include "lexer.h"
#include "opcodes.h"
#include "parser.h"
#include "relocations.h"
#include "was.h"

void emit_code(void) {
    for (int i = 0; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];
        if (section->chunks) layout_section(section);
    }

    for (int i = 0; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];
        if (section->chunks) emit_section_code(section);
    }
}

void assemble(char *input_filename, char *output_filename) {
    init_lexer(input_filename);
    init_sections();
    init_symbols();
    init_default_sections();
    init_relocations();
    init_opcodes();
    init_parser();
    init_dwarf();
    parse();
    emit_code();
    make_dwarf_debug_line_section();
    make_section_indexes();
    make_symbols_section();
    make_rela_sections();
    finish_elf(output_filename);
    free_lexer();
}
