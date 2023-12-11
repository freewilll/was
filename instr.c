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
    int rex_w;              // Need to set REX W bit
    int need_size16;        // Need to send 16-bit size override
    char prefix;            // Optional prefix. Zero if no prefix
    char primary_opcode;    // Primary opcode
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
int dump_instructions(Instructions *instr) {
    for (int i = 0; i < instr->size; i++) {
        if (i != 0) printf(", ");
        printf("%#02x", instr->data[i]);
    }
    printf("\n");
}

// Checks the amoung of argumenst matches the opcode.
// Returns the amount of arguments or -1 if there is no match
static int check_args(Opcode *opcode, Operand *op1, Operand *op2) {
    // Check the number of arguments match
    int opcode_arg_count = 0;
    if (opcode->op1.am) opcode_arg_count++;
    if (opcode->op2.am) opcode_arg_count++;
    if (opcode->acc) opcode_arg_count++;

    int arg_count = 0;
    if (op1) arg_count++;
    if (op2) arg_count++;

    if (opcode_arg_count != arg_count) return -1;

    return opcode_arg_count;
}

// Determine the size of the instruction from the opcode definition and operands
static int get_operation_size(Opcode *opcode, OpcodeAlias *opcode_alias, Operand *op1, Operand *op2, int opcode_arg_count) {
    int size = opcode_alias->size;

    // Determine size from register operands
    if (!size) {
        if (op2 && opcode->conver)
            size = OP_TO_SIZE(op2);
        else if (op1 && OP_TYPE_IS_REG(op1))
            size = OP_TO_SIZE(op1);
        else if (op2 && OP_TYPE_IS_REG(op2))
            size = OP_TO_SIZE(op2);

        if (!opcode->conver) {
            // Check operands are the same size
            if (op1 && op2 && OP_TYPE_IS_REG(op1) && OP_TYPE_IS_REG(op2) && OP_TO_SIZE(op1) != OP_TO_SIZE(op2))
                error("Size mismatch within oparands");
        }

        if (!size) size = SIZE32; // In long mode, the default instruction size is 32 bits;
    }

    // Check for size mismatches when the size is specified in the mnemonic
    if (opcode_alias->size) {
        if (op1 && OP_TYPE_IS_REG(op1) && !op1->indirect && OP_TO_SIZE(op1) != opcode_alias->size) error("Size mismatch with opcode");
        if (op2 && OP_TYPE_IS_REG(op2) && !op2->indirect && OP_TO_SIZE(op2) != opcode_alias->size) error("Size mismatch with opcode");
    }

    return size;
}

// Check if an opcode's operand matches an operand
static int op_matches(Opcode *opcode, OpcodeOp *opcode_op, Operand *op) {
    if (!op) panic("op unexpectedly null");

    int op_size = OP_TO_SIZE(op);

    // IMM08 operands are also treated as IMM16 since some opcodes don't
    // encode IMM08 values.

    int alt_op_size = op->type == IMM08 ? SIZE16 : 0;

    // Match sizes
    if (!op->indirect && opcode_op->sizes && !(opcode_op->sizes & op_size) && !(alt_op_size && opcode_op->sizes & alt_op_size)) return 0;

    // The instruction dst is an al, ax, eax, rax in it, aka an accumulator
    if (opcode->acc && op->reg && !op->indirect) return 0;

    // Addressing mode E means register or memory
    if (opcode_op->am == AM_E && !(OP_TYPE_IS_REG(op) || OP_TYPE_IS_MEM(op))) return 0;

    // Addressing mode G is register
    if (opcode_op->am == AM_G && (!OP_TYPE_IS_REG(op) || op->indirect)) return 0;

    // Addressing mode M is memory
    if (opcode_op->am == AM_M && (!OP_TYPE_IS_MEM(op) && !op->indirect)) return 0;

    // Addressing mode I is immediate
    if (opcode_op->am == AM_I && !OP_TYPE_IS_IMM(op)) return 0;

    // Addressing mode I is immediate
    if (opcode_op->am == AM_J && !OP_TYPE_IS_MEM(op)) return 0;

    // Check for illegal sign extensions
    if (opcode_op->am == AM_I && opcode_op->sign_extended && op->type == IMM32 && op->imm_or_mem_value >= 0x80000000) return 0;

    return 1;
}

// Encode non-indirect operand
static void encode_direct(Encoding *enc) {
    enc->mode = 0b11;
}

// Add displacement data to the encoding. Determine if it's 8 or 32 bytes
static int encode_displacement(Encoding *enc, Operand *op) {
    if (op->displacement_size) {
        enc->has_displacement = 1;
        enc->displacement = op->displacement;
        enc->displacement_size = op->displacement_size;
        enc->mode = enc->displacement_size == SIZE08 ? 0b01 : 0b10;
        return 1;
    }

    return 0;
}

// Encode an indirect operand
// See 1.4.1 of 24594.pdf:
// AMD64 Architecture Programmer’s Manual Volume 3: General-Purpose and System Instructions
// https://wiki.osdev.org/X86-64_Instruction_Encoding#32.2F64-bit_addressing
static void encode_indirect(Encoding *enc, Operand *op) {
    enc->rm = op->reg;

    int short_rm = enc->rm & 7;

    if (enc->rm == REG_RIP) {
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
static void make_imm_or_memory_size(Encoding *enc, Opcode *opcode, Operand *op1) {
    int size = enc->size;

    int value_size = opcode->op1.uses_op_size
        ? size == SIZE64 ? SIZE32: size // 16 or 32 bit
        : OP_TO_SIZE(op1); // Dubious. Should the instruction should determine the size of the immidate?

    if (size == SIZE64 && opcode->op1.can_be_imm64) value_size = SIZE64;

    enc->imm_or_mem = op1->imm_or_mem_value;
    enc->imm_or_mem_size = value_size;
}

// Using the opcode and operands, figure out all that is needed to be able to generate
// bytes for an instruction.
static Encoding make_encoding(Operand *op1, Operand *op2, Opcode *opcode, OpcodeAlias *opcode_alias, OpcodeOp *single_opcode, int opcode_arg_count) {
    Encoding enc = {0};

    // Some oddballs stored in enc.
    enc.size = get_operation_size(opcode, opcode_alias, op1, op2, opcode_arg_count);
    enc.has_mod_rm = (opcode->needs_mod_rm || opcode->opcd_ext != -1);
    enc.branch = opcode->branch;
    enc.prefix = opcode->prefix;
    enc.need_size16 = enc.size == SIZE16;

    int primary_opcode = opcode->primary_opcode;

    Operand *indirect_op = NULL;

    // Determing modrm reg and rm values
    if (opcode_arg_count == 0) {
        if (opcode->conver) enc.rex_w = enc.size == SIZE64;
    }

    else if (opcode_arg_count == 1) {
             if (single_opcode->am == AM_G) enc.reg = op1->reg;
        else if (single_opcode->am == AM_E) enc.reg = op1->reg;

        if (opcode->opcd_ext != -1) {
            enc.rm = enc.reg;
            enc.reg = opcode->opcd_ext;
        }

        if (single_opcode->am == AM_Z) {
            enc.rm = op1->reg;
            primary_opcode += (op1->reg & 7);
        }

        if (!single_opcode->word_or_double_word_operand) enc.rex_w = enc.size == SIZE64;
    }
    else if (opcode_arg_count == 2) {
             if (opcode->op1.am == AM_G) enc.reg = op1->reg;
        else if (opcode->op2.am == AM_G) enc.reg = op2->reg;
        else if (opcode->opcd_ext != -1) enc.reg = opcode->opcd_ext;

        if (opcode->op1.am == AM_E || opcode->op1.am == AM_M) {
            enc.rm = op1->reg;
            if (op1->indirect) indirect_op = op1;
        }
        else if (opcode->op2.am == AM_E || opcode->op2.am == AM_M) {
            enc.rm = op2->reg;
            if (op2->indirect) indirect_op = op2;
        }

        if (op2 && opcode->op2.am == AM_Z) {
            enc.rm = op2->reg;
            primary_opcode += (op2->reg & 7);
        }

        enc.rex_w = enc.size == SIZE64;
    }

    // Figure out mod/RM SIB and displacement
    if (opcode->needs_mod_rm || opcode->opcd_ext != -1) {
        if (!indirect_op)
            encode_direct(&enc);
        else
            encode_indirect(&enc, indirect_op);
    }

    enc.primary_opcode = primary_opcode;

    // Store immediate/memory details
    if (op1 && (OP_TYPE_IS_IMM(op1) || OP_TYPE_IS_MEM(op1)))
        make_imm_or_memory_size(&enc, opcode, op1);

    return enc;
}

// Returns 0/1 if a REX prefix needs to be emitted
static int needs_rex_prefix(Encoding *enc) {
    return (enc->rex_w || enc->reg >= 8 || enc->rm >= 8 || enc->index >= 8);
}

// Determine the amount of bytes for an encoded instruction
static int encoding_size(Encoding *enc) {
    return
        needs_rex_prefix(enc) +     // REX prefix
        enc->need_size16 +          // 16-bit size override
        (enc->prefix != 0) +        // Prefix
        1 +                         // Primary opcode
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
            emit_sib_byte(0, 0, enc->base);

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
    emit_value(instr, value_size, enc->imm_or_mem);
}

static void emit_instructions(Instructions *instr, Encoding *enc) {
    memset(instr, 0, sizeof(Instructions));

    if (enc->need_size16) emit_uint8(instr, OPCODE_SET_SIZE16);
    emit_REX_prefix(instr, enc);
    if (enc->prefix) emit_uint8(instr, enc->prefix);
    emit_uint8(instr, enc->primary_opcode);
    if (enc->has_mod_rm) emit_modrm(instr, enc);
    if (enc->has_sib) emit_sib(instr, enc);
    if (enc->has_displacement) emit_displacement(instr, enc);
    if (enc->imm_or_mem_size) emit_imm_or_memory(instr, enc);

    instr->branch = enc->branch;
}

Instructions make_instructions(char *mnemonic, Operand *op1, Operand *op2) {
    #ifdef DEBUG
    printf("Assembling %s %#x %#x\n", mnemonic, op1 ? op1->type : 0, op2 ? op2->type: 0);
    #endif

    // Look up the corresponding opcode. Translates e.g. movb to mov.
    OpcodeAlias *opcode_alias = strmap_get(opcode_alias_map, mnemonic);

    if (!opcode_alias) panic("Unknown instruction %s\n", mnemonic);

    // Loop over all possible encodings, picking the one that generates
    // the smallest number of bytes.
    Encoding best_enc;
    int best_enc_size = -1;

    for (int i = 0; i < opcode_alias->opcodes->length; i++) {
        Opcode *opcode = opcode_alias->opcodes->elements[i];

        #ifdef DEBUG
        printf("Checking: ");
        print_opcode(opcode);
        #endif

        int opcode_arg_count = check_args(opcode, op1, op2);
        if (opcode_arg_count == -1) continue; // The number of args mismatch

        // Check for match
        OpcodeOp *single_opcode = NULL;
        if (opcode_arg_count == 1) {
            single_opcode = opcode->op1.am ? &opcode->op1 : &opcode->op2;
            if (!op_matches(opcode, single_opcode, op1)) continue;
        }
        else if (opcode_arg_count == 2) {
            if (!op_matches(opcode, &opcode->op1, op1)) continue;
            if (!op_matches(opcode, &opcode->op2, op2)) continue;
        }

        // At this point, the opcode can be used to generate code
        #ifdef DEBUG
        printf("  Matched\n");
        #endif

        // Encode instruction
        Encoding enc = make_encoding(op1, op2, opcode, opcode_alias, single_opcode, opcode_arg_count);

        int enc_size = encoding_size(&enc);

        // Store the encoding with the least number of bytes in best_enc.
        if (enc_size < best_enc_size || best_enc_size == -1) {
            best_enc = enc;
            best_enc_size = enc_size;
        }
    }

    if (best_enc_size == -1) panic("Unable to find encoding for instruction %s\n", mnemonic);

    // Generate the instructions
    Instructions instr;
    emit_instructions(&instr, &best_enc);

    return instr;
}
