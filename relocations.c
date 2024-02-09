#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "list.h"
#include "relocations.h"

static List *relocations;

void init_relocations(void) {
    relocations = new_list(128);
}

// Get .rela.x associated with section .x. Create one if not existent
Section *get_relocation_section(Section *section) {
    if (!section->rela_section) {
        char *name = malloc(strlen(section->name + 5));
        sprintf(name, "%s%s", ".rela", section->name);
        section->rela_section = add_section(name, SHT_RELA, SHF_INFO_LINK, 0x08);
    }

    return section->rela_section;
}

void add_relocation(Section *section, Symbol *symbol, int type, long offset, int addend) {
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
        if (r->symbol->section_index && !r->symbol->binding == STB_GLOBAL && r->type != R_X86_64_REX_GOTP)
            add_elf_relocation(r->section, r->type, r->symbol->section->symtab_index, r->offset, r->symbol->value + r->addend);
        else
            add_elf_relocation(r->section, r->type, r->symbol->symtab_index, r->offset, r->addend);
    }
}

// Make ELF relocations sections
void make_rela_sections(void) {
    add_elf_relocations();

    for (int i = 0; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];
        if (section->rela_section) {
            section->rela_section->link = section_symtab->index;
            section->rela_section->info = section->index;
            section->rela_section->entsize = sizeof(ElfRelocation);
        }
    }
}
