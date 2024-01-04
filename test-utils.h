#include <stdarg.h>

#include "elf.h"

int dump_section(ElfSection *section);
void assert_section(ElfSection* section, va_list ap);
