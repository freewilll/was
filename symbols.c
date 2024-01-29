#include <stdlib.h>

#include "strmap.h"
#include "symbols.h"
#include "elf.h"

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
    symbol->section_index = 0;
    symbol->value = 0;

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
