#ifndef _DWARF_H
#define _DWARF_H

// https://dwarfstd.org/doc/Dwarf3.pdf page 95
typedef struct __attribute__ ((__packed__)) line_number_program_header {
    int unit_length;                          // The size in bytes of the line number information for this compilation unit, not including the unit_length field itself
    short version;                            // A version number
    int header_length;                        // The number of bytes following the header_length field to the beginning of the first byte of the line number program itself
    unsigned char minimum_instruction_length; // The size in bytes of the smallest target machine instruction.
    unsigned char default_is_stmt;            // The initial value of the is_stmt register
    signed char line_base;
    unsigned char line_range;
    unsigned char opcode_base;
    unsigned char standard_opcode_lengths[12];
} LineNumberProgramHeader;

void make_dwarf_debug_line_section(void);
void add_dwarf_file(int number, char *name);
void init_dwarf(void);

#endif
