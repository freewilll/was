#include <stdlib.h>

#include "strmap.h"
#include "symbols.h"
#include "elf.h"

StrMap *symbols;

void init_symbols(void) {
    symbols = new_strmap();;
}

// Get a symbol from the symbol table. Returns NULL if not present.
Symbol *get_symbol(char *name) {
    return (Symbol *) strmap_get(symbols, name);
}

// Retrieve a symbol. If it doesn't exist, create one.
Symbol *get_or_add_symbol(char *name) {
    Symbol *symbol = strmap_get(symbols, name);

    if (symbol) return symbol;

    symbol = calloc(1, sizeof(Symbol));

    symbol->name    = name;
    symbol->type    = STT_NOTYPE;
    symbol->binding = STB_LOCAL;
    symbol->section_index = 0;
    symbol->offset = 0;

    strmap_put(symbols, name, symbol);

    return symbol;
}
