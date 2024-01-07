#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "elf.h"
#include "utils.h"

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

        if (expected == -1) {
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

        if (expected_type == -1) {
            if (pos != section->size) {
                dump_relocations(section);
                panic("Unexpected data at position %d", pos);
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
