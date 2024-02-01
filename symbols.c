#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "strmap.h"
#include "symbols.h"

StrMap *symbols;

Symbol builtin_dot_symbol = { ".", 0, STB_LOCAL, STT_NOTYPE };

void init_symbols(void) {
    symbols = new_strmap();;
}

// Get a symbol from the symbol table. Returns NULL if not present.
Symbol *get_symbol(char *name) {
    if (name[0] == '.' && !name[1]) return &builtin_dot_symbol;

    return (Symbol *) strmap_get(symbols, name);
}

Symbol *add_symbol(char *name) {
    Symbol *symbol = calloc(1, sizeof(Symbol));

    symbol->name    = name;
    symbol->type    = STT_NOTYPE;
    symbol->binding = STB_LOCAL;

    strmap_put(symbols, name, symbol);

    return symbol;
}

// Retrieve a symbol. If it doesn't exist, create one.
Symbol *get_or_add_symbol(char *name) {
    Symbol *symbol = get_symbol(name);

    if (symbol)
        return symbol;
    else
        return add_symbol(name);
}

void associate_symbol_with_current_section(Symbol *symbol) {
    Section *section = get_current_section();

    symbol->value = section->size;
    symbol->section = section;
}

// Add non-global, then global symbols to the symtab section
void make_symbols_section(void) {
    // Add non-global symbols
    for (StrMapIterator it = strmap_iterator(symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(symbols, name);

        if (symbol->section) symbol->section_index = symbol->section->index;

        // All undefined symbols must be global.
        if (!symbol->section_index) symbol->binding = STB_GLOBAL;

        // Any local symbols starting with .L aren't included in the ELF.
        int dot_local = strlen(name) >= 2 && name[0] == '.' && name[1] == 'L';
        if (symbol->binding != STB_GLOBAL && !dot_local) {
            symbol->symtab_index = add_elf_symbol(name, symbol->value, symbol->size, symbol->binding, symbol->type, symbol->section_index);
        }
    }

    // Add global symbols
    for (StrMapIterator it = strmap_iterator(symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(symbols, name);

        if (symbol->section) symbol->section_index = symbol->section->index;

        if (symbol->binding == STB_GLOBAL)
            symbol->symtab_index = add_elf_symbol(name, symbol->value, symbol->size, symbol->binding, symbol->type, symbol->section_index);
    }

    section_symtab->link = section_strtab->index;
    section_symtab->info = local_symbol_end + 1; // Index of the first global symbol
    section_symtab->entsize = sizeof(ElfSymbol);
}
