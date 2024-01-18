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

// Types
#define AT_B    1    // # Byte
#define AT_BS   2    // # Byte, sign-extended to the size of the destination operand.
#define AT_BSS  3    // # Byte, sign-extended to the size of the stack pointer (for example, PUSH (6A)).
#define AT_D    4    // #  Doubleword
#define AT_DI   5    // #  Doubleword Integer (x87 FPU only)
#define AT_DR   6    // #  Double-real. Only x87 FPU instructions
#define AT_DQP  7    // # Doubleword, or quadword, promoted by REX.W in 64-bit mode
#define AT_ER   8    // # Extended-real. Only x87 FPU instructions).
#define AT_Q    9    // # Quad
#define AT_QI   10   // # Quad Integer (x87 FPU only)
#define AT_SR   11   // # Single-real (x87 FPU only)
#define AT_SS   12   // #  Scalar element of a 128-bit packed single-precision floating data.
#define AT_SD   13   // #  Scalar element of a 128-bit packed double-precision floating data.
#define AT_V    14   // #   Word or doubleword, depending on operand-size attribute (for example, INC (40), PUSH (50)).
#define AT_VDS  15   // # Word or doubleword, depending on operand-size attribute, or doubleword, sign-extended to 64 bits for 64-bit operand size.
#define AT_VQ   16   // # Quadword (default) or word if operand-size prefix is used (for example, PUSH (50)).
#define AT_VQP  17   // # Word or doubleword, depending on operand-size attribute, or quadword, promoted by REX.W in 64-bit mode.
#define AT_VS   18   // # Word or doubleword sign extended to the size of the stack pointer (for example, PUSH (68)).
#define AT_W    19   // #  Word
#define AT_WI   20   // #  Word Integer (x87 FPU only)

typedef struct opcode_op {
    int am;                     // Addressing mode
    int type;                   // Type
    int sizes;
    char uses_op_size;
    int can_be_imm64;
    int word_or_double_word_operand;
    char is_gen_reg;
    char gen_reg_nr;
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
    OpcodeOp op3;
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
    char op1_size;
    char op2_size;
    char op3_size;
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
