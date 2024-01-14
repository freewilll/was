#include <stdarg.h>

#include "elf.h"

#define END -1

int dump_section(ElfSection *section);
void vassert_section(ElfSection* section, va_list ap);
void assert_section(ElfSection* section, ...);
void assert_relocations(ElfSection* section, ...);
void dump_symbols(void);
void assert_symbols(int first, ...);
