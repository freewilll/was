#ifndef _OPCODES_H
#define _OPCODES_H

#include <stdint.h>
#include <stddef.h>

#include "list.h"
#include "strmap.h"

// Addressing modes
#define AM_C      1      // The reg field of the ModR/M byte selects a control register
#define AM_D      2      // The reg field of the ModR/M byte selects a debug register
#define AM_E      3      // The operand is either a general-purpose register or a memory address.
#define AM_ES     4      // (Implies original E).  The operand is either a x87 FPU stack register or a memory address.
#define AM_EST    5      // (Implies original E). A ModR/M byte follows the opcode and specifies the x87 FPU stack register.
#define AM_G      6      // The reg field of the ModR/M byte selects a general register
#define AM_I      7      // Immediate
#define AM_J      8      // RIP relative
#define AM_H      9      // The r/m field of the ModR/M byte always selects a general register, regardless of the mod field
#define AM_M      10     // The ModR/M byte may refer only to memory
#define AM_O      11     // Offset
#define AM_R      12     // Not used
#define AM_S      13     // Not used
#define AM_ST     14     // x87 FPU stack register.
#define AM_T      15     // The reg field of the ModR/M byte selects a test register (only MOV (0F24, 0F26)).
#define AM_V      16     // The reg field of the ModR/M byte selects a 128-bit XMM register.
#define AM_W      17     // The operand is either a 128-bit XMM register or a memory address.
#define AM_Z      18     // The three least-significant bits of the opcode byte selects a general-purpose register

typedef struct opcode_op {
    int am;                     // Addressing mode
    int sizes;
    char uses_op_size;
    int can_be_imm64;
    int sign_extended;
    int word_or_double_word_operand;
    char *type;
} OpcodeOp;

typedef struct opcode {
    char *mnem;
    uint8_t prefix;             // Optional prefix
    uint8_t ohf_prefix;         // Optional 0xf prefix
    uint8_t primary_opcode;
    uint8_t sec_opcd;           // Secondary opcode
    int opcd_ext;               // -1 if not used
    int needs_mod_rm;
    OpcodeOp op1;
    OpcodeOp op2;
    char op_size;
    char direction;
    char acc;                   // Accumulator
    char branch;                // Is a branch instruction
    char conver;                // Is a type conversion
    char x87fpu;                // Is a X87 FPU long double instruction
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
