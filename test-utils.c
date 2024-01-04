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


void assert_section(ElfSection* section, va_list ap) {
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

