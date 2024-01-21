#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "instr.h"
#include "lexer.h"
#include "opcodes.h"
#include "utils.h"

#define OPCODE_SET_SIZE16 0x66

// Uncomment to enable debug output
// #define DEBUG

// Encoding contains all the details necessary to generate the bytes for an instruction
typedef struct encoding {
    int size;               // Size of operation
    int need_rex;           // Set to 1 to always emit a rex. A rex can be emitted even if this is set to zero.
    int rex_w;              // Need to set REX W bit
    int need_size16;        // Need to send 16-bit size override
    char prefix;            // Optional prefix
    char ohf_prefix;        // Optional 0xf prefix. Zero if no prefix
    char primary_opcode;    // Primary opcode
    char sec_opcd;          // Secondary opcode
    int has_mod_rm;         // Has Mod RM byte
    int mode;               // Mod RM mode
    int reg;                // Mod RM reg
    int rm;                 // Mod RM rm
    int has_sib;            // Has SIB byte
    int scale;              // SIB scale number
    int index;              // SIB index register
    int base;               // SIB base register
    int has_displacement;   // Has displacement 8-bit byte or 32-bit bytes
    int displacement;       // Displacement offset
    int displacement_size;  // Displacement size
    long imm_or_mem;        // Immediate or memory bytes
    int imm_or_mem_size;    // Size of immediate or memory bytes
    int branch;             // Is branch instruction
} Encoding;

// Print hex bytes for the encoded instructions
void dump_instructions(Instructions *instr) {
    for (int i = 0; i < instr->size; i++) {
        if (i != 0) printf(", ");
        printf("%#02x", instr->data[i]);
    }
    printf("\n");
}

// Checks the amoung of argumenst matches the opcode.
// Returns the amount of arguments or -1 if there is no match
static int check_args(Opcode *opcode, Operand *op1, Operand *op2, Operand *op3) {
    // Check the number of arguments match
    int opcode_arg_count = 0;
    if (opcode->op1.am || opcode->op1.is_gen_reg) opcode_arg_count++;
    if (opcode->op2.am || opcode->op2.is_gen_reg) opcode_arg_count++;
    if (opcode->op3.am || opcode->op3.is_gen_reg) opcode_arg_count++;

    int arg_count = 0;
    if (op1) arg_count++;
    if (op2) arg_count++;
    if (op3) arg_count++;

    if (opcode_arg_count != arg_count) return -1;

    return opcode_arg_count;
}

// Determine the size of the instruction from the opcode definition and operands
static int get_operation_size(Opcode *opcode, OpcodeAlias *opcode_alias, Operand *op1, Operand *op2, Operand *op3) {
    int size = opcode_alias->op1_size; // Except for conversions, op1_size == op2_size

    if (opcode->conver) {
        if (op1 && OP_HAS_SIZE(op1)) size = OP_TO_SIZE(op1);
        if (op2 && OP_HAS_SIZE(op2)) size = OP_TO_SIZE(op2);
    }

    // Determine size from register operands
    if (!size) {
        if (opcode->branch && op1 && OP_TYPE_IS_MEM(op1))
            size = OP_TO_SIZE(op1);
        else if (op1 && OP_TYPE_IS_REG(op1) && !op1->indirect)
            size = OP_TO_SIZE(op1);
        else if (op2 && OP_TYPE_IS_REG(op2) && !op2->indirect)
            size = OP_TO_SIZE(op2);
        else if (op3 && OP_TYPE_IS_REG(op3) && !op3->indirect)
            size = OP_TO_SIZE(op3);

        // Check operands are the same size unless they are conversions or one of them is an indirect
        if (!opcode->conver && op1 && op2 && !op1->indirect && !op2->indirect) {
            if (op1 && op2 && OP_TYPE_IS_REG(op1) && OP_TYPE_IS_REG(op2) && OP_TO_SIZE(op1) != OP_TO_SIZE(op2))
                error("Size mismatch within oparands");
            if (op1 && op3 && OP_TYPE_IS_REG(op1) && OP_TYPE_IS_REG(op3) && OP_TO_SIZE(op1) != OP_TO_SIZE(op3))
                error("Size mismatch within oparands");
        }

        if (!size) size = SIZE32; // In long mode, the default instruction size is 32 bits;
    }

    return size;
}

// Check if an immediate operand matches the opcode operand
static int imm_op_matches(Opcode *opcode, OpcodeOp *opcode_op, Operand *op, int size) {
    // If the imm value is negative then there is no need to check for operands that
    // would sign extend.
    if (op->imm_or_mem_value < 0) return 1;

    // Check for illegal sign extensions in immediates
    // bs and bss is a byte sign extended
    // vds is an int32 sign extended to int64
    int imm08_would_byte_sign_extend = op->imm_or_mem_value <= -0x80 || op->imm_or_mem_value >= 0x80;
    int imm32_would_byte_sign_extend = op->imm_or_mem_value <= -0x80000000L || op->imm_or_mem_value >= 0x80000000L;

    // Will the opcode sign extend?
    int ext08 = opcode_op->type == AT_BS || opcode_op->type == AT_BSS;
    int ext32 = opcode_op->type == AT_VDS;

    if (ext08 && op->type == IMM08 && size == SIZE16 && imm08_would_byte_sign_extend) return 0;
    if (ext08 && op->type == IMM08 && size == SIZE32 && imm08_would_byte_sign_extend) return 0;
    if (ext08 && op->type == IMM08 && size == SIZE64 && imm08_would_byte_sign_extend) return 0;
    if (ext32 && op->type == IMM32 && size == SIZE64 && imm32_would_byte_sign_extend) return 0;

    return 1;
}

// Check if an opcode's operand matches an operand
static int op_matches(Opcode *opcode, int opcode_alias_size, OpcodeOp *opcode_op, Operand *op, int size) {
    if (!op) panic("op unexpectedly null");

    // If the operation size can be determined by the operand, use that, otherwise fall back too the
    // passed-in size, which is determined by the opcode or opcode alias.
    int op_size = OP_HAS_SIZE(op) ? OP_TO_SIZE(op): size;

    // If the opcode alias has a size & the addressing mode is memory, then the  operand doesn't have
    // a size and is matched.
    if (!opcode_alias_size && opcode_op->am == AM_M && !OP_HAS_SIZE(op)) return 1;

    // Match sizes
    if (opcode_op->sizes
        && !(OP_TO_SIZE(op) == SIZEXM)
        && !(opcode_op->sizes & op_size)
        && !OP_TYPE_IS_IMM(op)              // Immediate sizes are handled in AM_I code
    ) return 0;

    // Operand is a general register. Check it matches.
    if (opcode_op->is_gen_reg && OP_TYPE_IS_REG(op) && opcode_op->gen_reg_nr != op->reg) return 0;

    // The instruction dst is an al, ax, eax, rax in it, aka an accumulator
    if (opcode->acc && (op->reg || op->indirect)) return 0;

    // Match addressing mode
    switch (opcode_op->am) {
        case 0:
            // No addressing mode is restricted in another way, e.g. with acc or is_gen_reg.
            return 1;

        case AM_E:
            // Register or memory
            return OP_TYPE_IS_REG(op) || OP_TYPE_IS_MEM(op);

         case AM_ES:
            // fp87 stack register or memory
            return OP_TYPE_IS_ST(op) || OP_TYPE_IS_MEM(op) || op->indirect;

        case AM_EST:
            // fp87 stack register
            return OP_TYPE_IS_ST(op);

        case AM_G:
            // Register
            return OP_TYPE_IS_REG(op) && !op->indirect;

        case AM_I: {
            // Immediate

            if (!OP_TYPE_IS_IMM(op)) return 0;

            int org_size = OP_TO_SIZE(op);

            if (org_size & (IMM08)                         && (opcode_op->sizes & SIZE08) && imm_op_matches(opcode, opcode_op, op, size)) return 1;
            if (org_size & (IMM08 | IMM16)                 && (opcode_op->sizes & SIZE16) && imm_op_matches(opcode, opcode_op, op, size)) return 1;
            if (org_size & (IMM08 | IMM16 | IMM32)         && (opcode_op->sizes & SIZE32) && imm_op_matches(opcode, opcode_op, op, size)) return 1;
            if (org_size & (IMM08 | IMM16 | IMM32 | IMM64) && (opcode_op->sizes & SIZE64) && imm_op_matches(opcode, opcode_op, op, size)) return 1;

            return 0;

        }

        case AM_J:
            // RIP relative
            return OP_TYPE_IS_MEM(op);

        case AM_M:
            // Memory
            return OP_TYPE_IS_MEM(op) || (OP_TYPE_IS_REG(op) && op->indirect);

        case AM_S:
            // Not implemented
            return 0;

        case AM_ST:
            // x87 FPU stack register
            return OP_TYPE_IS_ST(op);

        case AM_V:
            // xmm register
            return OP_TYPE_IS_XMM(op);

        case AM_W:
            // xmm register or memory
            return OP_TYPE_IS_XMM(op) || OP_TYPE_IS_MEM(op) || (OP_TYPE_IS_REG(op) && op->indirect);

        case AM_Z:
            // Register
            return OP_TYPE_IS_REG(op) && !op->indirect;

        default:
            panic("Internal error: unhandled addressing mode %d", opcode_op->am);
    }

    panic("Should not get here");
}

// Encode non-indirect operand
static void encode_mod_rm_register(Encoding *enc) {
    enc->mode = 0b11;
}

// Add displacement data to the encoding. Determine if it's 8 or 32 bytes
static int encode_displacement(Encoding *enc, Operand *op) {
    // Displacement is signed. If there is an overflow on 8-bit displacement,
    // convert it to 32 bit.
    if (op->displacement_size == 1 && op->displacement >= 0x80)
        op->displacement_size = SIZE32;

    if (op->displacement_size) {
        enc->has_displacement = 1;
        enc->displacement = op->displacement;
        enc->displacement_size = op->displacement_size;
        enc->mode = enc->displacement_size == SIZE08 ? 0b01 : 0b10;
        return 1;
    }

    return 0;
}

// Encode an indirect operand or memory without register
// See 1.4.1 of 24594.pdf:
// AMD64 Architecture Programmer’s Manual Volume 3: General-Purpose and System Instructions
// https://wiki.osdev.org/X86-64_Instruction_Encoding#32.2F64-bit_addressing
static void encode_mod_rm_memory(Encoding *enc, Operand *op) {
    enc->rm = op->reg;

    int short_rm = enc->rm & 7;

    if (OP_TYPE_IS_MEM(op)) {
        // 32 bit displacement without a register, converted from a memory operand
        // This is pretty ugly. The memory/immediate emitting code and this code
        // should play together a bit more nicely. Modifying the op here is also
        // pretty nasty.

        enc->rm = 4;
        enc->has_sib = 1;
        enc->scale = 0;
        enc->index = 4;
        enc->base = 5;

        // Disable emitting of memory value
        enc->imm_or_mem_size = 0;
        op->displacement = op->imm_or_mem_value;
        op->displacement_size = SIZE32;
        encode_displacement(enc, op);
        enc->mode = 0;
        op->type &= ~MEM; // Ugly!
    }

    else if (enc->rm == REG_RIP) {
        // Must have 32 bit displacement
        enc->rm = 5;
        encode_displacement(enc, op);
        enc->mode = 0b00;
        enc->has_displacement = 1;
        enc->displacement_size = SIZE32;
    }

    else if (short_rm == 4) {
        // RBP & R12
        // Must have SIB

        enc->has_sib = 1;
        enc->base = 4;
        enc->index = 4;

        if (op->has_sib) {
            enc->scale = op->scale;
            enc->rm = 4;
            enc->index = op->index;
            enc->base = op->base;
        }

        if (encode_displacement(enc, op)) enc->index = 4;
    }

    else if (short_rm == 5) {
        // RSP and R13

        if (op->has_sib) {
            enc->rm = 4;
            enc->has_sib = 1;
            enc->scale = op->scale;
            enc->index = op->index;
            enc->base = op->base;
        }

        // Must have 8 bit displacement
        if (!encode_displacement(enc, op)) {
            enc->has_displacement = 1;
            enc->mode = 0b01;
            enc->displacement_size = SIZE08;
        }
    }

    else {
        // Regular registers

        if (op->has_sib) {
            enc->has_sib = 1;
            enc->base = 4;
            enc->rm = 4;
            enc->scale = op->scale;
            enc->index = op->index;
            enc->base = op->base;
        }

        encode_displacement(enc, op);
    }
}

// Determine the size of an immediate or memory operand.
static void make_imm_or_memory_size(Encoding *enc, OpcodeOp *opcode_op, Operand *op) {
    int size = enc->size;

    int value_size = opcode_op->uses_op_size
        ? size == SIZE64 ? SIZE32: size // 16 or 32 bit
        : OP_TO_SIZE(op); // Dubious. Should the instruction should determine the size of the immediate?

    if (size == SIZE64 && opcode_op->can_be_imm64) value_size = SIZE64;

    enc->imm_or_mem = op->imm_or_mem_value;
    enc->imm_or_mem_size = value_size;
}

// Encode mod/rm byte from operand and addressing mode
static void encode_mod_rm(Operand *op, int am, Encoding *enc, int *pprimary_opcode, Operand **pmemory_op) {
    switch (am) {
        case AM_G:
        case AM_V:
            enc->reg = op->reg;
            break;

        case AM_E:
        case AM_ES:
        case AM_EST:
        case AM_M:
        case AM_W:
            enc->rm = op->reg;
            if (OP_TYPE_IS_MEM(op) || op->indirect) *pmemory_op = op;
            break;

        case AM_Z:
            enc->rm = op->reg;
            *pprimary_opcode += (op->reg & 7);
            break;
    }
}

// Using the opcode and operands, figure out all that is needed to be able to generate
// bytes for an instruction.
static Encoding make_encoding(Operand *op1, Operand *op2, Operand *op3, Opcode *opcode, OpcodeAlias *opcode_alias, int opcode_arg_count, int size) {
    Encoding enc = {0};

    // Some oddballs stored in enc.
    enc.size = size;
    enc.has_mod_rm = (opcode->needs_mod_rm || opcode->opcd_ext != -1);
    enc.branch = opcode->branch;
    enc.prefix = opcode->prefix;
    enc.ohf_prefix = opcode->ohf_prefix;
    int is_xmm = ((op1 && OP_TYPE_IS_XMM(op1)) || (op2 && OP_TYPE_IS_XMM(op2)) || (op3 && OP_TYPE_IS_XMM(op3)));
    enc.need_size16 = (enc.size == SIZE16 && !is_xmm && !opcode->x87fpu);

    int primary_opcode = opcode->primary_opcode;

    // Emit a rex byte when one of the alternate spl, bpl, sil, dil 8 bit registers are used
    if ((op1 && OP_TYPE_IS_ALT_8BIT(op1)) || (op2 && OP_TYPE_IS_ALT_8BIT(op2)) || (op3 && OP_TYPE_IS_ALT_8BIT(op3))) enc.need_rex = 1;

    Operand *memory_op = NULL;

    if (opcode->opcd_ext != -1) enc.reg = opcode->opcd_ext;

    if (op1) encode_mod_rm(op1, opcode->op1.am, &enc, &primary_opcode, &memory_op);
    if (op2) encode_mod_rm(op2, opcode->op2.am, &enc, &primary_opcode, &memory_op);
    if (op3) encode_mod_rm(op3, opcode->op3.am, &enc, &primary_opcode, &memory_op);

    if (!opcode->op1.word_or_double_word_operand && !opcode->op2.word_or_double_word_operand && !opcode->x87fpu && !opcode->branch)
        enc.rex_w = enc.size == SIZE64;

    // Don't emit a rex prefix for some exceptions that default to be 64 bit in long mode
    // like push, pushq.
    // https://wiki.osdev.org/X86-64_Instruction_Encoding#Usage
    if (!strcmp(opcode->mnem, "push")) {
        enc.need_rex = 0;
        enc.rex_w =0;
    }

    // Figure out mod/RM SIB and displacement
    if (opcode->needs_mod_rm || opcode->opcd_ext != -1) {
        if (memory_op)
            encode_mod_rm_memory(&enc, memory_op);
        else
            encode_mod_rm_register(&enc);
    }

    enc.primary_opcode = primary_opcode;
    enc.sec_opcd = opcode->sec_opcd;

    // Store immediate/memory details
    if (op1 && (OP_TYPE_IS_IMM(op1) || OP_TYPE_IS_MEM(op1))) make_imm_or_memory_size(&enc, &opcode->op1, op1);
    if (op2 && (OP_TYPE_IS_IMM(op2) || OP_TYPE_IS_MEM(op2))) make_imm_or_memory_size(&enc, &opcode->op2, op2);
    if (op3 && (OP_TYPE_IS_IMM(op3) || OP_TYPE_IS_MEM(op3))) make_imm_or_memory_size(&enc, &opcode->op3, op3);

    return enc;
}

// Returns 0/1 if a REX prefix needs to be emitted
static int needs_rex_prefix(Encoding *enc) {
    return (enc->need_rex || enc->rex_w || enc->reg >= 8 || enc->rm >= 8 || enc->index >= 8);
}

// Determine the amount of bytes for an encoded instruction
static int encoding_size(Encoding *enc) {
    return
        needs_rex_prefix(enc) +     // REX prefix
        enc->need_size16 +          // 16-bit size override
        (enc->prefix != 0) +        // Prefix
        (enc->ohf_prefix != 0) +    // 0x0f Prefix
        1 +                         // Primary opcode
        (enc->sec_opcd != 0) +      // Secondary opcode
        enc->has_mod_rm +           // Mod RM byte
        enc->has_sib +              // SIB byte
        enc->displacement_size +    // Displacement
        enc->imm_or_mem_size;       // Immediate/memory
}

// Utility functions to add bytes to instructions
#define EMIT(_type, _size) do { *((_type *) &instr->data[instr->size]) = data;  instr->size += (_size); } while (0);
static void emit_uint8 (Instructions *instr, uint8_t  data) { EMIT(uint8_t,  1); }
static void emit_uint16(Instructions *instr, uint16_t data) { EMIT(uint16_t, 2); }
static void emit_uint32(Instructions *instr, uint32_t data) { EMIT(uint32_t, 4); }
static void emit_uint64(Instructions *instr, uint64_t data) { EMIT(uint64_t, 8); }

// Add REX prefix if necessary
static void emit_REX_prefix(Instructions *instr, Encoding *enc) {
    if (needs_rex_prefix(enc)) {
        int rex_b = enc->has_sib && enc->base != 4 ? enc->base : enc->rm;
        emit_uint8(instr, (0b100 << 4) | ( REX_W * enc->rex_w) | (REX_B * (rex_b >> 3)) | (REX_R * (enc->reg >> 3)) | (REX_X * (enc->index >> 3)));
    }
}

// Add ModR/M byte
static void emit_modrm(Instructions *instr, Encoding *enc) {
    emit_uint8(instr, (enc->mode << 6) | ((enc->reg & 7) << 3) | (enc->rm & 7));
}

#define emit_sib_byte(s, i, b) emit_uint8(instr, (((s) & 3) << 6) | (((i) & 7) << 3) | ((b & 7)))

// See 1.4.2 of 24594.pdf:
// AMD64 Architecture Programmer’s Manual Volume 3: General-Purpose and System Instructions
// https://wiki.osdev.org/X86-64_Instruction_Encoding#32.2F64-bit_addressing_2
static void emit_sib(Instructions *instr, Encoding *enc) {
    int short_base = enc->base & 7;

    if (enc->mode == 0b00) {
        if (short_base == 5) {
            emit_sib_byte(enc->scale, enc->index, enc->base);

            if (enc->base == 5) {
                enc->has_displacement = 1;
                enc->displacement_size = SIZE32;
            }
        }
        else
            emit_sib_byte(enc->scale, enc->index, enc->base);
    }
    else if (enc->mode == 0b01) {
            emit_sib_byte(enc->scale, enc->index, enc->base);

        if (enc->index == 4) {
            enc->has_displacement = 1;
            enc->displacement_size = SIZE08;
        }
    }
    else if (enc->mode == 0b10) {
        emit_sib_byte(enc->scale, enc->index, enc->base);

        if (enc->index == 4) {
            enc->has_displacement = 1;
            enc->displacement_size = SIZE32;
        }
    }
}

// Add a value to the instructions
static void emit_value(Instructions *instr, int size, long value) {
         if (size == SIZE08) emit_uint8 (instr, value);
    else if (size == SIZE16) emit_uint16(instr, value);
    else if (size == SIZE32) emit_uint32(instr, value);
    else                     emit_uint64(instr, value);
}

static void emit_displacement(Instructions *instr, Encoding *enc) {
    instr->relocation_offset = instr->size;
    instr->relocation_size = enc->displacement_size;
    emit_value(instr, enc->displacement_size, enc->displacement);
}

static void emit_imm_or_memory(Instructions *instr, Encoding *enc) {
    int value_size = enc->imm_or_mem_size;
    instr->relocation_offset = instr->size;
    instr->relocation_size = value_size;
    long value = enc->branch ? 0 : enc->imm_or_mem;
    emit_value(instr, value_size, value);
}

static void emit_instructions(Instructions *instr, Encoding *enc) {
    memset(instr, 0, sizeof(Instructions));

    if (enc->need_size16) emit_uint8(instr, OPCODE_SET_SIZE16);
    if (enc->prefix) emit_uint8(instr, enc->prefix);
    emit_REX_prefix(instr, enc);
    if (enc->ohf_prefix) emit_uint8(instr, enc->ohf_prefix);
    emit_uint8(instr, enc->primary_opcode);
    if (enc->sec_opcd) emit_uint8(instr, enc->sec_opcd);
    if (enc->has_mod_rm) emit_modrm(instr, enc);
    if (enc->has_sib) emit_sib(instr, enc);
    if (enc->has_displacement) emit_displacement(instr, enc);
    if (enc->imm_or_mem_size) emit_imm_or_memory(instr, enc);

    instr->branch = enc->branch;
}

Instructions make_instructions(char *mnemonic, Operand *op1, Operand *op2, Operand *op3) {
    #ifdef DEBUG
    printf("Assembling %s %#x %#x %#x\n", mnemonic, op1 ? op1->type : 0, op2 ? op2->type: 0, op3 ? op3->type: 0);
    #endif

    // Deal with GAS special case for imul
    // https://ftp.gnu.org/old-gnu/Manuals/gas-2.9.1/html_node/as_204.html
    // We have added a two operand form of `imul' when the first operand is an immediate
    // mode expression and the second operand is a register. This is just a shorthand,
    // so that, multiplying `%eax' by 69, for example, can be done with `imul
    // $69, %eax' rather than `imul $69, %eax, %eax'.
    if (!strncmp("imul", mnemonic, 4) && OP_TYPE_IS_IMM(op1) && OP_TYPE_IS_REG(op2) && !op2->indirect && !op3)
        op3 = op2;

    List *opcode_alias_list = strmap_get(opcode_alias_map, mnemonic);

    if (!opcode_alias_list) error("Unknown instruction %", mnemonic);

    // Loop over all possible encodings, picking the one that generates
    // the smallest number of bytes.
    Encoding best_enc;
    int best_enc_size = -1;

    for (int alias_i = 0; alias_i < opcode_alias_list->length; alias_i++) {
        OpcodeAlias *opcode_alias = opcode_alias_list->elements[alias_i];

        for (int i = 0; i < opcode_alias->opcodes->length; i++) {
            Opcode *opcode = opcode_alias->opcodes->elements[i];

            #ifdef DEBUG
            printf("Checking: %s ", opcode_alias->alias_mnem);
            print_opcode(opcode);
            #endif

            int opcode_arg_count = check_args(opcode, op1, op2, op3);
            if (opcode_arg_count == -1) continue; // The number of args mismatch

            int size = get_operation_size(opcode, opcode_alias, op1, op2, op3);

            int op1_size = size;
            int op2_size = size;
            int op3_size = size;

            if (opcode->conver) {
                if (op1 && !OP_HAS_SIZE(op1)) op1_size = opcode_alias->op1_size;
                if (op2 && !OP_HAS_SIZE(op2)) op2_size = opcode_alias->op2_size;
            }

            // Check for match
            if (op1 && !op_matches(opcode, opcode_alias->op1_size, &opcode->op1, op1, op1_size)) continue;
            if (op2 && !op_matches(opcode, opcode_alias->op2_size, &opcode->op2, op2, op2_size)) continue;
            if (op3 && !op_matches(opcode, opcode_alias->op3_size, &opcode->op3, op3, op3_size)) continue;

            // At this point, the opcode can be used to generate code
            #ifdef DEBUG
            printf("  Matched\n");
            #endif

            // Encode instruction
            Encoding enc = make_encoding(op1, op2, op3, opcode, opcode_alias, opcode_arg_count, size);

            int enc_size = encoding_size(&enc);

            // Store the encoding with the least number of bytes in best_enc.
            if (enc_size < best_enc_size || best_enc_size == -1) {
                best_enc = enc;
                best_enc_size = enc_size;
            }
        }
    }

    if (best_enc_size == -1) error("Unable to find encoding for instruction %s", mnemonic);

    // Generate the instructions
    Instructions instr;
    emit_instructions(&instr, &best_enc);

    return instr;
}
