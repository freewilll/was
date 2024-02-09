#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include "elf.h"
#include "strmap.h"

// The naming is dubious: this covers both symbols and sections

extern Section *section_text;
extern Section *section_data;
extern Section *section_bss;
extern Section *section_rodata;
extern Section *section_symtab;
extern Section *section_strtab;
extern Section *section_shstrtab;

typedef struct symbol {
    char *name;         // Name
    int size;           // Size
    int binding;        // Binding, e.g. local or global
    int type;           // Type, e.g. function or object
    int symtab_index;   // Index in the ELF symbol table
    Section *section;   // Section the symbol was defined in. Zero if not in a section (e.g. an undefined symbol)
    int section_index;  // Section index the symbol was defined in. Set either in the final pass, or if section is unset, e.g. for the COMM section
    int value;          // Offset or alignment
} Symbol;

extern StrMap *symbols;

void init_symbols(void);
Symbol *get_symbol(char *name);
Symbol *add_symbol(char *name);
Symbol *get_or_add_symbol(char *name);
Section *add_section(char *name, int type, int flags, int align);
void make_symbols_section(void);
void init_default_sections(void);

#endif
