#include <stdarg.h>

#include "elf.h"
#include "lexer.h"
#include "parser.h"
#include "relocations.h"
#include "symbols.h"

#define END -1

void init_tests(void);
void test_full_assembly(char *summary, char *input, ...);
int dump_section(ElfSection *section);
void vassert_section(ElfSection* section, va_list ap);
void assert_section(ElfSection* section, ...);
void assert_relocations(ElfSection* section, ...);
void dump_symbols(void);
void assert_symbols(int first, ...);
