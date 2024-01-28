#include <stdio.h>
#include <stdlib.h>

#include "elf.h"
#include "list.h"
#include "relocations.h"

static List *relocations;

void init_relocations(void) {
    relocations = new_list(128);
}

void add_relocation(ElfSection *section, Symbol *symbol, int type, long offset, int addend) {
    Relocation *r = malloc(sizeof(Relocation));
    r->type = type;
    r->offset = offset;
    r->symbol = symbol;
    r->addend = addend;
    r->section = section;

    append_to_list(relocations, r);
}

void add_elf_relocations(void) {
    for (int i = 0; i < relocations->length; i++) {
        Relocation *r = relocations->elements[i];

        // Global symbols that have been declared don't get rewritten to a section offset.
        // Symbols that use the global offset table also don't get rewritten to a section offset.
        if (r->symbol->section_index && !r->symbol->binding == STB_GLOBAL && r->type != R_X86_64_REX_GOTP) {
            // By convention, the section indexes correspond with the symbol table indexes..
            add_elf_relocation(r->section, r->type, r->symbol->section_index, r->offset, r->symbol->value + r->addend);
        }
        else {
            add_elf_relocation(r->section, r->type, r->symbol->symtab_index, r->offset, r->addend);
        }
    }
}
