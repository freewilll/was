#ifndef _PARSER_H
#define _PARSER_H

#include "instr.h"

Instructions *parse_instruction_statement(void);

void parse_directive_statement(void);
int parse(void);
void init_parser(void);
void emit_code(void);

#endif
