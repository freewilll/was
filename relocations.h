#ifndef _RELOCATIONS_H
#define _RELOCATIONS_H

#include "elf.h"
#include "symbols.h"

typedef struct relocation {
    Symbol *symbol;         // Symbol the relocation gets its offset from
    int type;               // Type of relocation
    int offset;             // Offset in the data section the relocated address ends up in
    int addend;             // Number to add to the symbol
    ElfSection *section;    // Section the relocation applies to
} Relocation;

void init_relocations(void);
void add_relocation(ElfSection *section, Symbol *symbol, int type, long offset, int addend);
void add_elf_relocations(void);

#endif
