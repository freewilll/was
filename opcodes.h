#ifndef _OPCODES_H
#define _OPCODES_H

#include <stdint.h>

#include "list.h"
#include "strmap.h"

// Addressing modes
#define AM_E 'E' // Operand is register or memory. Uses ModR/M byte
#define AM_G 'G' // Operand is a register.         Uses ModR/M byte
#define AM_I 'I' // Immediate
#define AM_J 'J' // RIP relative
#define AM_M 'M' // Operand is memory
#define AM_O 'O' // Offset
#define AM_S 'S' // Not used
#define AM_Z 'Z' // The three least-significant bits of the opcode byte selects a general-purpose register

typedef struct opcode_op {
    int am;
    int sizes;
    char uses_op_size;
    int can_be_imm64;
    int sign_extended;
    int word_or_double_word_operand;
    char *type;
} OpcodeOp;

typedef struct opcode {
    char *mnem;
    uint8_t value;
    int opcd_ext;       // -1 if not used
    int needs_mod_rm;
    OpcodeOp dst;
    OpcodeOp src;
    char op_size;
    char direction;
    char acc; // Accumulator
    char branch; // Is a branch instruction
} Opcode;

typedef struct opcode_alias {
    char *alias_mnem;
    char *mnem;
    int size;
    List *opcodes;
} OpcodeAlias;

extern Opcode opcodes[];
extern OpcodeAlias opcode_aliases[];

extern int opcode_count;
extern int opcode_aliases_count;

extern StrMap *opcode_alias_map;

void print_opcode(Opcode *opcode);
void init_opcodes(void);

#endif
