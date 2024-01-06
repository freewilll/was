#include <stdio.h>
#include <stdlib.h>

#include "elf.h"
#include "list.h"
#include "relocations.h"

static List *relocations;

void init_relocations(void) {
    relocations = new_list(128);
}

void add_relocation(Symbol *symbol, int type, long offset, int addend) {
    Relocation *r = malloc(sizeof(Relocation));
    r->type = type;
    r->offset = offset;
    r->symbol = symbol;
    r->addend = addend;

    append_to_list(relocations, r);
}

void add_elf_relocations(void) {
    for (int i = 0; i < relocations->length; i++) {
        Relocation *r = relocations->elements[i];
        if (r->symbol->section_index) {
            // By convention, the section indexes correspond with the symbol table indexes
            int addend = r->symbol->offset - 4;
            add_elf_relocation(r->type, r->symbol->section_index, r->offset, addend);
        }
        else {
            add_elf_relocation(r->type, r->symbol->symtab_index, r->offset, r->addend - 4);
        }
    }
}
