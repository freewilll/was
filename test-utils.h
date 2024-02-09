#include <stdarg.h>

#include "elf.h"
#include "lexer.h"
#include "parser.h"
#include "relocations.h"
#include "symbols.h"

#define END -1

void init_tests(void);
void test_full_assembly(char *summary, char *input, ...);
int dump_section(Section *section);
void vassert_section_data(Section* section, va_list ap);
void assert_section_data(Section* section, ...);
void assert_relocations(char *section_name, ...);
void dump_symbols(void);
void assert_symbols(int first, ...);
void assert_section(char *name, int type, int flags);
int get_symbol_symtab_index(char *name);