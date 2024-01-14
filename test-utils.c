#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "elf.h"
#include "utils.h"
#include "test-utils.h"

const char *symbol_type_names[] = {
    "NOTYPE", "OBJECT", "FUNC", "SECTION", "FILE", "COMMON", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?"
};

const char *symbol_binding_names[] = {
    "LOCAL", "GLOBAL", "WEAK", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
};


// Print hex bytes for the encoded instructions
int dump_section(ElfSection *section) {
    for (int i = 0; i < section->size; i++) {
        if (i != 0) printf(", ");
        printf("%#04x", (unsigned char) section->data[i]);
    }
    printf("\n");
}


void vassert_section(ElfSection* section, va_list ap) {
    if (!section) panic("Assert section on a NULL");

    int pos = 0;

    while (1) {
        unsigned int expected = va_arg(ap, unsigned int);

        if (expected == END) {
            if (pos != section->size) {
                dump_section(section);
                panic("Unexpected data at position %d", pos);
            }

            return; // Success
        }

        if (pos == section->size) {
            dump_section(section);
            panic("Expected extra data at position %d: %#02x", pos, expected & 0xff);
        }

        if ((expected & 0xff) != (section->data[pos] & 0xff)) {
            dump_section(section);
            panic("Mismatch at position %d: expected %#02x, got %#02x", pos, (uint8_t) expected & 0xff, section->data[pos] & 0xff);
        }

        pos++;
    }
}

void assert_section(ElfSection* section, ...) {
    va_list ap;
    va_start(ap, section);

    vassert_section(section, ap);

    va_end(ap);
}

void dump_relocations(ElfSection* section) {
    printf("Relocations:\n");
    printf("info          offset   addend\n");
    ElfRelocation *relocations = (ElfRelocation *) section->data;

    int count = section->size / sizeof(ElfRelocation);
    for (int i = 0; i < count; i++) {
        ElfRelocation *r = &relocations[i];
        printf("%#8lx %#8lx %8ld\n", r->r_info, r->r_offset, r->r_addend);
    }
}

void assert_relocations(ElfSection* section, ...) {
    va_list ap;
    va_start(ap, section);

    if (!section) panic("Assert section on a NULL");

    int pos = 0;

    while (1) {
        int expected_type           = va_arg(ap, int);
        int expected_symbol_index   = va_arg(ap, int);
        int expected_offset         = va_arg(ap, int);
        int expected_addend         = va_arg(ap, int);

        if (expected_type == END) {
            if (pos != section->size) {
                dump_relocations(section);
                panic("Unexpected data at position %d", pos / sizeof(ElfRelocation));
            }

            return; // Success
        }

        ElfRelocation *r = (ElfRelocation *) &section->data[pos];

        int got_type         = r->r_info & 31;
        int got_symbol_index = r->r_info >> 32;
        int got_offset       = r->r_offset;
        int got_addend       = r->r_addend;

        if (pos == section->size) {
            dump_relocations(section);
            panic("Expected extra data at position %d", pos / sizeof(ElfRelocation));
        }

        if (
                expected_type != got_type ||
                expected_symbol_index != got_symbol_index ||
                expected_offset != got_offset ||
                expected_addend != got_addend) {

            dump_relocations(section);
            panic("Relocations mismatch at position %d: expected %#x, %d, %#x, %d, got %#x, %d, %#x, %ld",
                pos / sizeof(ElfRelocation),
                expected_type,
                expected_symbol_index,
                expected_offset,
                expected_addend,
                got_type,
                got_symbol_index,
                got_offset,
                got_addend
            );
        }

        pos += sizeof(ElfRelocation);
    }
}

// Readelf compatible symbol table output
void dump_symbols(void) {
    printf("Symbol Table:\n");
    printf("   Num:     Value         Size Type    Bind   Vis      Ndx Name\n");
    ElfSymbol *symbols = (ElfSymbol *) section_symtab.data;

    int count = section_symtab.size / sizeof(ElfSymbol);
    for (int i = 0; i < count; i++) {
        ElfSymbol *symbol = &symbols[i];
        char binding = (symbol->st_info >> 4) & 0xf;
        char type = symbol->st_info & 0xf;
        const char *type_name = symbol_type_names[type];
        const char *binding_name = symbol_binding_names[binding];

        printf("%6d: %016ld  %4ld %-8s%-7sDEFAULT  ", i, symbol->st_value, symbol->st_size, type_name, binding_name);
        if (symbol->st_shndx)
            printf("%3d", symbol->st_shndx);
        else
            printf("UND");

        int strtab_offset = symbol->st_name;
        if (strtab_offset)
            printf(" %s\n",  &section_strtab.data[strtab_offset]);
        else
            printf("\n");
    }
}

void assert_symbols(int first, ...) {
    ElfSection *section = &section_symtab;

    va_list ap;
    va_start(ap, first);

    int processed_first = 0;
    int pos = first_symbol_index * sizeof(ElfSymbol); // Skip section symbols

    while (1) {
        int expected_value;
        if (processed_first) {
            expected_value = va_arg(ap, int);
        }
        else {
            expected_value = first;
            processed_first = 1;
        }

        int expected_type = va_arg(ap, int);
        int expected_binding = va_arg(ap, int);
        char *expected_name = va_arg(ap, char *);

        if (expected_value == END) {
            if (pos != section->size) {
                dump_symbols();
                panic("Unexpected data at position %d", (pos - 1) / sizeof(ElfSymbol));
            }

            return; // Success
        }

        ElfSymbol *symbol = (ElfSymbol *) &section->data[pos];

        int got_value = symbol->st_value;
        int got_type = symbol->st_info & 0xf;
        char got_binding = (symbol->st_info >> 4) & 0xf;
        char *got_name = symbol->st_name ? &section_strtab.data[symbol->st_name] : 0;

        if (pos == section->size) {
            dump_symbols();
            panic("Expected extra data at position %d", (pos - 1) / sizeof(ElfSymbol));
        }

        int name_matches = ((!got_name && !expected_name) || (expected_name && !strcmp(expected_name, got_name)));

        if (expected_value != got_value || expected_type != got_type || expected_binding != got_binding || !name_matches) {
            dump_symbols();
            panic("Symbols mismatch at position %d: expected %ld, %d, %d, %s, got %ld, %d, %d, %s",
                (pos - 1) / sizeof(ElfSymbol),
                expected_value,
                expected_type,
                expected_binding,
                expected_name ? expected_name : "null",
                got_value,
                got_type,
                got_binding,
                got_name ? got_name : "null"
            );
        }

        pos += sizeof(ElfSymbol);
    }
}
