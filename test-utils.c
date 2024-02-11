#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dwarf.h"
#include "elf.h"
#include "utils.h"
#include "test-utils.h"
#include "was.h"

const char *symbol_type_names[] = {
    "NOTYPE", "OBJECT", "FUNC", "SECTION", "FILE", "COMMON", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?"
};

const char *symbol_binding_names[] = {
    "LOCAL", "GLOBAL", "WEAK", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
};

void init_tests(void) {
    init_sections();
    init_opcodes();
    init_symbols();
    init_default_sections();
    init_relocations();
}

void test_full_assembly(char *summary, char *input, ...) {
    va_list ap;
    va_start(ap, input);

    if (!input) input = summary;

    printf("%-60s", summary);

    section_text->size = 0;
    section_data->size = 0;
    section_rodata->size = 0;
    section_symtab->size = 0;

    init_lexer_from_string(input);
    init_sections();
    init_symbols();
    init_default_sections();
    init_relocations();
    init_parser();
    init_dwarf();
    parse();
    emit_code();
    make_dwarf_debug_line_section();
    make_section_indexes();
    make_symbols_section();
    make_rela_sections();

    vassert_section_data(section_text, ap);

    printf("pass\n");
}

// Print hex bytes for the encoded instructions
int dump_section(Section *section) {
    for (int i = 0; i < section->size; i++) {
        if (i != 0) printf(", ");
        printf("%#04x", (unsigned char) section->data[i]);
    }
    printf("\n");
}


void vassert_section_data(Section* section, va_list ap) {
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

void assert_section_data(Section* section, ...) {
    va_list ap;
    va_start(ap, section);

    vassert_section_data(section, ap);

    va_end(ap);
}

void dump_relocations(Section* section) {
    printf("Relocations:\n");
    printf("info          offset   addend\n");
    ElfRelocation *relocations = (ElfRelocation *) section->data;

    int count = section->size / sizeof(ElfRelocation);
    for (int i = 0; i < count; i++) {
        ElfRelocation *r = &relocations[i];
        printf("%#8lx %#8lx %8ld\n", r->r_info, r->r_offset, r->r_addend);
    }
}

void assert_relocations(char *section_name, ...) {
    Section *section = get_section(section_name);
    if (!section) panic("No section %s\n", section_name);

    va_list ap;
    va_start(ap, section_name);

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

        int got_type         = r->r_info & -1;
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
    ElfSymbol *symbols = (ElfSymbol *) section_symtab->data;

    int count = section_symtab->size / sizeof(ElfSymbol);
    for (int i = 0; i < count; i++) {
        ElfSymbol *symbol = &symbols[i];
        char binding = (symbol->st_info >> 4) & 0xf;
        char type = symbol->st_info & 0xf;
        const char *type_name = symbol_type_names[type];
        const char *binding_name = symbol_binding_names[binding];

        printf("%6d: %016ld  %4ld %-8s%-7sDEFAULT  ", i, symbol->st_value, symbol->st_size, type_name, binding_name);
        if ((unsigned short) symbol->st_shndx == SHN_UNDEF)
            printf("UND");
        else if ((unsigned short) symbol->st_shndx == SHN_ABS)
            printf("ABS");
        else if ((unsigned short) symbol->st_shndx == SHN_COMMON)
            printf("COM");
        else
            printf("%3d", symbol->st_shndx);

        int strtab_offset = symbol->st_name;
        if (strtab_offset)
            printf(" %s\n", &section_strtab->data[strtab_offset]);
        else
            printf("\n");
    }
}

void assert_symbols(int first, ...) {
    Section *section = section_symtab;

    va_list ap;
    va_start(ap, first);

    int processed_first = 0;
    int pos = 1 * sizeof(ElfSymbol); // Skip null symbol

    while (1) {
        int expected_value;
        if (processed_first) {
            expected_value = va_arg(ap, int);
        }
        else {
            expected_value = first;
            processed_first = 1;
        }

        int expected_size = va_arg(ap, int);
        int expected_type = va_arg(ap, int);
        int expected_binding = va_arg(ap, int);
        unsigned int expected_index = va_arg(ap, unsigned int);
        char *expected_name = va_arg(ap, char *);

        ElfSymbol *symbol = NULL;
        while (pos < section->size) {
            symbol = (ElfSymbol *) &section->data[pos];
            int type = symbol->st_info & 0xf;
            if (type != STT_SECTION && type != STT_FILE) break;

            pos += sizeof(ElfSymbol);
        }

        if (expected_value == END) {
            if (pos != section->size) {
                dump_symbols();
                panic("Unexpected data at position %d", (pos - 1) / sizeof(ElfSymbol));
            }

            return; // Success
        }

        if (pos == section->size) {
            dump_symbols();
            panic("Expected extra data");
        }

        int got_value = symbol->st_value;
        int got_size = symbol->st_size;
        int got_type = symbol->st_info & 0xf;
        char got_binding = (symbol->st_info >> 4) & 0xf;
        unsigned short got_index = symbol->st_shndx;
        char *got_name = symbol->st_name ? &section_strtab->data[symbol->st_name] : 0;

        int name_matches = ((!got_name && !expected_name) || (expected_name && !strcmp(expected_name, got_name)));

        if (
                expected_value != got_value ||
                expected_size != got_size ||
                expected_type != got_type ||
                expected_binding != got_binding ||
                expected_index != got_index ||
                !name_matches) {
            dump_symbols();
            panic("Symbols mismatch at position %d: expected %ld, %ld, %d, %d, %d, %s, got %ld, %ld, %d, %d, %d, %s",
                (pos - 1) / sizeof(ElfSymbol),
                expected_value,
                expected_size,
                expected_type,
                expected_binding,
                expected_index,
                expected_name ? expected_name : "null",
                got_value,
                got_size,
                got_type,
                got_binding,
                got_index,
                got_name ? got_name : "null"
            );
        }

        pos += sizeof(ElfSymbol);
    }
}

void assert_section(char *name, int type, int flags) {
    Section *section = get_section(name);
    if (!section) panic("No section %s", name);

    if (section->type != type) panic("Mismatched type, expected %d, got %d", type, section->type);
    if (section->flags != flags) panic("Mismatched flags, expected %d, got %d", flags, section->flags);
}

// Get index in the symbol table for a symbol. Returns zero if none.
int get_symbol_symtab_index(char *name) {
    Symbol *symbol = get_symbol(name);
    return symbol ? symbol->symtab_index : 0;
}

static void dump_dwarf_dirs(void) {
    Section *section = get_section(".debug_line");
    if (!section) panic("No .debug_line section");

    printf("DWARF directory table:\n");
    char *dirs = section->data + sizeof(LineNumberProgramHeader);
    int pos = 1;
    while (*dirs) {
        printf("%4d %s\n", pos++, dirs);
        dirs += strlen(dirs) + 1;
    }
}

static void dump_dwarf_files(void) {
    Section *section = get_section(".debug_line");
    if (!section) panic("No .debug_line section");

    char *files = section->data + sizeof(LineNumberProgramHeader);
    while (*files) while (*files++); // Move past dirs
    files++; // Move past terminating zero

    printf("DWARF file table:\n");
    printf("Entry  Dir  Name:\n");
    int pos = 1;
    while (*files) {
        char *filename = files;
        files += strlen(files) + 1;
        int dir_index = *files++; // Decoding uleb128 isn't implemented, so it's just one byte.
        files += 2; // time and length bytes
        printf("%5d %4d  %s\n", pos++, dir_index, filename);
    }
}

void assert_dwarf_dirs(char *first, ...) {
    Section *section = get_section(".debug_line");
    if (!section) panic("No .debug_line section");

    char *dirs = section->data + sizeof(LineNumberProgramHeader);

    va_list ap;
    va_start(ap, first);

    int processed_first = 0;
    int pos = 1;

    while (1) {
        char *expected_value;
        if (processed_first) {
            expected_value = va_arg(ap, char *);
        }
        else {
            expected_value = first;
            processed_first = 1;
        }

        if ((long) expected_value == ENDL) {
            if (dirs[0]) {
                dump_dwarf_dirs();
                panic("Unexpected data at position %d", pos);
            }

            return; // Success
        }

        if (!dirs[0]) {
            dump_dwarf_dirs();
            panic("Expected extra data");
        }

        if (strcmp(dirs, expected_value)) {
            dump_dwarf_dirs();
            panic("Mismatch at position %d: expected %s, got %s", pos, expected_value, dirs);
        }

        dirs += strlen(dirs) + 1;
        pos++;
    }
}

void assert_dwarf_files(int first, ...) {
    Section *section = get_section(".debug_line");
    if (!section) panic("No .debug_line section");

    char *files = section->data + sizeof(LineNumberProgramHeader);
    while (*files) while (*files++); // Move past dirs
    files++; // Move past terminating zero

    va_list ap;
    va_start(ap, first);

    int processed_first = 0;
    int pos = 1;

    while (1) {
        char *expected_filename;
        int expected_dir_index;
        if (processed_first) {
            expected_dir_index = va_arg(ap, int);
        }
        else {
            expected_dir_index = first;
            processed_first = 1;
        }

        expected_filename = va_arg(ap, char *);

        if (expected_dir_index == END) {
            if (files[0]) {
                dump_dwarf_files();
                panic("Unexpected data at position %d", pos);
            }

            return; // Success
        }

        if (!files[0]) {
            dump_dwarf_files();
            panic("Expected extra data");
        }

        char *got_filename = files;
        files += strlen(files) + 1;
        int got_dir_index = *files++; // Decoding uleb128 isn't implemented, so it's just one byte.
        files += 2; // time and length bytes

        if (strcmp(got_filename, expected_filename) || got_dir_index != expected_dir_index) {
            dump_dwarf_files();
            panic("Mismatch at position %d: expected %d/%s, got %d/%s", pos, expected_dir_index, expected_filename, got_dir_index, got_filename);
        }

        pos++;
    }
}
