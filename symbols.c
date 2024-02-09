#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "strmap.h"
#include "symbols.h"

// The naming is dubious: this covers both symbols and sections

StrMap *symbols;

Symbol builtin_dot_symbol = { ".", 0, STB_LOCAL, STT_NOTYPE };

void init_symbols(void) {
    symbols = new_strmap();
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

// Add a section + associated symbol
Section *add_section(char *name, int type, int flags, int align) {
    Section *section = add_elf_section(name, type, flags, align);

    // Add a symbol unless it's the null section
    if (name[0]) {
        Symbol *symbol = strmap_get(symbols, name);

        // A symbol might already be defined before the section is created
        if (!symbol) {
            symbol = add_symbol(strdup(name));
        }

        symbol->binding = STB_LOCAL;
        symbol->type = STT_SECTION;
        symbol->section = section;
    }

    return section;
}

// Add non-global, then global symbols to the symtab section
void make_symbols_section(void) {
    // Add non-global symbols
    for (StrMapIterator it = strmap_iterator(symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(symbols, name);

        if (symbol->section) symbol->section_index = symbol->section->index;

        // All undefined symbols must be global.
        if (symbol->type != STT_SECTION && !symbol->section_index) symbol->binding = STB_GLOBAL;

        // Any local symbols starting with .L aren't included in the ELF.
        int dot_local = strlen(name) >= 2 && name[0] == '.' && name[1] == 'L';
        if (symbol->binding != STB_GLOBAL && !dot_local) {
            char *elf_name = symbol->type == STT_SECTION ? "" : name;
            symbol->symtab_index = add_elf_symbol(elf_name, symbol->value, symbol->size, symbol->binding, symbol->type, symbol->section_index);

            if (symbol->type == STT_SECTION) {
                Section *section = get_section(name);
                section->symtab_index = symbol->symtab_index;
            }
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

// Create default sections
void init_default_sections(void) {
    //                                name            type          flags                      alignment
                          add_section("" ,            0,            0,                         0   );
    section_text        = add_section(".text",        SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 0x10);
    section_data        = add_section(".data",        SHT_PROGBITS, SHF_ALLOC | SHF_WRITE,     0x04);
    section_bss         = add_section(".bss",         SHT_NOBITS,   SHF_ALLOC | SHF_WRITE,     0x04);
    section_rodata      = add_section(".rodata",      SHT_PROGBITS, SHF_ALLOC,                 0x04);
    section_symtab      = add_section(".symtab",      SHT_SYMTAB,   0,                         0x08);
    section_strtab      = add_section(".strtab",      SHT_STRTAB,   0,                         0x01);
    section_shstrtab    = add_section(".shstrtab",    SHT_STRTAB,   0,                         0x01);

    // Start string table entries at 1, so that the zero value goes to an empty string
    add_to_section(section_strtab, "", 1);

    add_elf_symbol("", 0, 0, STB_LOCAL, STT_NOTYPE, SHN_UNDEF); // Null symbol
}
