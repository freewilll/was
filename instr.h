#include <stdint.h>

#include "opcodes.h"
#include "symbols.h"


#ifndef _INSTR_H
#define _INSTR_H

#define REX_B 1 // The high bit of modrm rm or SIB base
#define REX_X 2 // The high bit of modrm index
#define REX_R 4 // The high bit of modrm reg
#define REX_W 8 // Set to 1 when 64 bit operands are used

#define SIZE08 0x01
#define SIZE16 0x02
#define SIZE32 0x04
#define SIZE64 0x08
#define SIZEXM 0x10 // A special size that means "unknown". Used for XMM registers where the opcode determines the size
#define SIZEST 0x20 // A special size used for fp87 stack registers

#define REG      0x40
#define IMM      0x80
#define MEM      0x100
#define ALT_8BIT 0x200 // Used with REG for spl, bpl, sil, dil 8-bit registers

typedef enum operand_type {
    REG08 =  SIZE08 | REG,
    REG16 =  SIZE16 | REG,
    REG32 =  SIZE32 | REG,
    REG64 =  SIZE64 | REG,
    REGXM =  SIZEXM | REG, // xmm registers
    REGST =  SIZEST | REG, // x87 ST registers
    IMM08 =  SIZE08 | IMM,
    IMM16 =  SIZE16 | IMM,
    IMM32 =  SIZE32 | IMM,
    IMM64 =  SIZE64 | IMM,
    MEM08 =  SIZE08 | MEM,
    MEM16 =  SIZE16 | MEM,
    MEM32 =  SIZE32 | MEM,
    MEM64 =  SIZE64 | MEM,
} OperandType;

#define OP_TYPE_IS_REG(op) (((op)->type & REG) == REG)
#define OP_TYPE_IS_IMM(op) (((op)->type & IMM) == IMM)
#define OP_TYPE_IS_MEM(op) (((op)->type & MEM) == MEM)
#define OP_TO_SIZE(op) ((op)->type & ~(IMM | REG | MEM | ALT_8BIT))
#define OP_HAS_SIZE(op) (!((op)->type & (SIZEXM | MEM)))
#define OP_IS_XMM(op) ((op)->type & SIZEXM)
#define OP_IS_ST(op) ((op)->type & SIZEST)
#define OP_IS_ALT_8BIT(op) ((op->type & ALT_8BIT))

typedef struct operand {
    OperandType type;           // Operand type
    int reg;                    // Register number
    long imm_or_mem_value;      // Immediate or memory value
    int indirect;               // Is it an indirect?
    int displacement;           // Displacement value
    int displacement_size;      // Size of the displacement (or zero if none)
    int has_sib;                // Has Scale, Index, Base
    int scale;                  // SIB scale (1, 2, 4, 8)
    int index;                  // SIB index register
    int base;                   // SIB base register
    Symbol *relocation_symbol;
} Operand;

typedef struct instructions {
    uint8_t data[16];
    int size;
    int relocation_offset;
    int relocation_size;
    Symbol *relocation_symbol;
    char branch;                // Is it a branch instruction?
} Instructions;

typedef struct instructions_set {
    int using_primary;
    Instructions *primary;
    Instructions *secondary;
    List *symbols;              // Zero or more symbols associated with the address at this instruction
} InstructionsSet;

int dump_instructions(Instructions *instr);

Instructions make_instructions(char *mnemonic, Operand *op1, Operand *op2);

#endif
