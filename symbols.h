#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include "strmap.h"

typedef struct symbol {
    char *name;         // Name
    int size;           // Size
    int binding;        // Binding, e.g. local or global
    int type;           // Type, e.g. function or object
    int symtab_index;   // Index in the ELF symbol table
    int section_index;  // Section the symbol was defined in. Zero if not in a section (e.g. an undefined symbol)
    int value;          // Offset or alignment
} Symbol;

extern StrMap *symbols;

void init_symbols(void);
Symbol *get_symbol(char *name);
Symbol *get_or_add_symbol(char *name);

#endif
