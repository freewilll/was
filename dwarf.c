#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwarf.h"
#include "elf.h"
#include "list.h"
#include "utils.h"
#include "symbols.h"

#define LINE_BASE  -5
#define LINE_RANGE 14
#define OPCODE_BASE 13
#define MIN_LINE_INCREMENT (LINE_BASE)
#define MAX_LINE_INCREMENT (LINE_BASE + LINE_RANGE - 1)
#define OP255_ADDRESS_INCREMENT ((255 - OPCODE_BASE) / LINE_RANGE) // Used by DW_LNS_const_add_pc

static List *dirs_list;
static StrMap *dirs_map;
static List *files;
static int next_dir_index;

typedef struct file {
    char *filename;
    int dir_index;
} File;

// State machine for the line nunmbers
typedef struct state {
    // DWARF State machine
    int address;
    int file;
    int line_number;

    // Internal state
    char *data;             // Buffer
    int allocated;          // Allocated memory in buffer
    int size;               // Used size
    int locs_present;       // 1 if any .locs are present (the normal case)
} State;

State state;

static int make_dwarf_debug_line_section_header(Section *debug_line_section) {
    LineNumberProgramHeader header    = {0};

    header.version                    = 3; // DWARF version 3
    header.minimum_instruction_length = 1;
    header.default_is_stmt            = 1;
    header.line_base                  = LINE_BASE;
    header.line_range                 = LINE_RANGE;
    header.opcode_base                = OPCODE_BASE;

    header.standard_opcode_lengths[ 0] = 0; // Opcode 1  has 0 args
    header.standard_opcode_lengths[ 1] = 1; // Opcode 2  has 1 arg
    header.standard_opcode_lengths[ 2] = 1; // Opcode 3  has 1 arg
    header.standard_opcode_lengths[ 3] = 1; // Opcode 4  has 1 arg
    header.standard_opcode_lengths[ 4] = 1; // Opcode 5  has 1 arg
    header.standard_opcode_lengths[ 5] = 0; // Opcode 6  has 0 args
    header.standard_opcode_lengths[ 6] = 0; // Opcode 7  has 0 args
    header.standard_opcode_lengths[ 7] = 0; // Opcode 8  has 0 args
    header.standard_opcode_lengths[ 8] = 1; // Opcode 9  has 1 arg
    header.standard_opcode_lengths[ 9] = 0; // Opcode 10 has 0 args
    header.standard_opcode_lengths[10] = 0; // Opcode 11 has 0 args
    header.standard_opcode_lengths[11] = 1; // Opcode 12 has 1 arg

    int header_in_section_pos = debug_line_section->size;

    add_to_section(debug_line_section, &header, sizeof(header));

    return header_in_section_pos;
}

static int make_dwarf_debug_line_section_dirs(Section *debug_line_section) {
    static char zero = 0;

    for (int i = 0; i < dirs_list->length; i++) {
        char *filename = dirs_list->elements[i];
        add_to_section(debug_line_section, filename, strlen(filename) + 1);
    }

    add_to_section(debug_line_section, &zero, 1); // Terminator
}

static int make_dwarf_debug_line_section_files(Section *debug_line_section) {
    static char zero = 0;

    for (int i = 0; i < files->length; i++) {
        File *file = files->elements[i];
        if (!file) simple_error("Non consecutive .file numbers");
        add_to_section(debug_line_section, file->filename, strlen(file->filename) + 1);

        char uleb128_data[9];
        int size = encode_uleb128(file->dir_index, uleb128_data);
        add_to_section(debug_line_section, uleb128_data, size);
        add_to_section(debug_line_section, &zero, 1); // Time of last modification not implemented
        add_to_section(debug_line_section, &zero, 1); // Length in bytes not implemented
    }

    add_to_section(debug_line_section, &zero, 1); // Terminator
}

// Allocate space at the end of a section and return a pointer to it.
// Dynamically allocate space size as needed.
static void *allocate_in_state_data(int size) {
    int new_size = state.size + size;
    if (new_size > state.allocated) {
        while (new_size > state.allocated) state.allocated *= 2;
        state.data = realloc(state.data, state.allocated);
    }

    void *result = state.data + state.size;
    state.size = new_size;

    return result;
}

// Copy src to the end of a section and return the offset
static int add_to_state_data(const void *src, int size) {
    char *data = allocate_in_state_data(size);
    memcpy(data, src, size);
    return data - state.data;
}

static void make_dwarf_debug_line_section_program(Section *debug_line_section) {
    if (!state.locs_present) return; // No debug line info

    // Add final loc for the end of the file
    add_dwarf_loc(state.file, state.line_number, section_text->size);

    // Extended opcode 1: End of Sequence
    const char epilogue[] = {0x00, 0x01, DW_LNE_end_sequence};
    add_to_state_data(epilogue, sizeof(epilogue));

    add_to_section(debug_line_section, state.data, state.size);
}

void make_dwarf_debug_line_section(void) {
    // Only add line number information if a .debug_info section exists
    if (!get_section(".debug_info")) return;

    Section *debug_line_section = get_section(".debug_line");
    if (!debug_line_section)
        debug_line_section = add_section(".debug_line", SHT_PROGBITS, 0, 0);

    int header_in_section_pos = make_dwarf_debug_line_section_header(debug_line_section);
    make_dwarf_debug_line_section_dirs(debug_line_section);
    make_dwarf_debug_line_section_files(debug_line_section);

    // Note: debug_line_section->data can be reallocated so its address must not
    // be stored  & reused with an add_to_section call in between.
    LineNumberProgramHeader *header_in_section = (LineNumberProgramHeader *) (debug_line_section->data + header_in_section_pos);

    // The number of bytes following the header_length field to the beginning of the
    // first byte of the line number program itself.
    header_in_section->header_length =
        debug_line_section->size -
        (offsetof(LineNumberProgramHeader, minimum_instruction_length) - offsetof(LineNumberProgramHeader, unit_length));

    make_dwarf_debug_line_section_program(debug_line_section);

    header_in_section = (LineNumberProgramHeader *) (debug_line_section->data + header_in_section_pos);
    header_in_section->unit_length = debug_line_section->size - 4; // Size not including the unit_length field itself
}

static int add_dir(char *dirname) {
    int dir_index = (int) (long) strmap_get(dirs_map, dirname);
    if (dir_index) return dir_index;

    dir_index = next_dir_index++;
    strmap_put(dirs_map, dirname, (void *) (long) dir_index);
    append_to_list(dirs_list, dirname);

    return dir_index;
}

static void add_file(int file_index, int dir_index, char *filename) {
    if (file_index > files->length) {
        int old_length = files->length;
        resize_list(files, file_index);
        for (int i = old_length; i < files->length; i++) files->elements[i] = NULL;
    }

    if (files->elements[file_index - 1])
        error("File with index %d already taken", file_index);

    File *file = calloc(0, sizeof(File));
    file->filename = filename;
    file->dir_index = dir_index;
    files->elements[file_index - 1] = file;
}

void add_dwarf_file(int file_index, char *name) {
    if (name[0] == 0) error("Empty filename");

    char *last_slash = strrchr(name, '/');

    if (!last_slash) {
        add_file(file_index, 0, name);
    }
    else {
        if (name == last_slash) { // /test.c
            add_file(file_index, 0, name);
        }
        else {

            *last_slash = 0;
            char *dirname = name;
            name = last_slash + 1;
            int dir_index = add_dir(dirname);
            add_file(file_index, dir_index, name);
        }
    }
}

static void add_dwarf_loc_advance_address(int address_advance) {
    if (!address_advance) return;

    char dw_lns_advance_pc = DW_LNS_advance_pc;
    add_to_state_data(&dw_lns_advance_pc, 1);

    char sleb128_data[9];
    int size = encode_sleb128(address_advance, sleb128_data);
    add_to_state_data(sleb128_data, size);
}

static void add_dwarf_loc_increment_line(int line_increment) {
    if (!line_increment) return;

    char dw_lns_advance_line = DW_LNS_advance_line;
    add_to_state_data(&dw_lns_advance_line, 1);

    char sleb128_data[9];
    int size = encode_sleb128(line_increment, sleb128_data);
    add_to_state_data(sleb128_data, size);
}

void add_dwarf_loc(int file_index, int line_number, int address) {
    if (!state.locs_present) {
        // Extended opcode 2: set Address to 0x0
        const char prologue[] = {0x00, 0x09, DW_LNE_set_address, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        add_to_state_data(prologue, sizeof(prologue));

        state.locs_present = 1;
    }

    if (file_index != state.file) {
        char dw_lns_set_file = DW_LNS_set_file;
        add_to_state_data(&dw_lns_set_file, 1);

        char sleb128_data[9];
        int size = encode_sleb128(file_index, sleb128_data);
        add_to_state_data(sleb128_data, size);

        state.file = file_index;
    }

    // https://dwarfstd.org/doc/Dwarf3.pdf page 99
    int address_advance = address - state.address;

    if (address_advance < 0) simple_error("DWARF line numbers going backwards in address");

    int line_increment = line_number - state.line_number;
    unsigned int opcode = (line_increment - LINE_BASE) + (LINE_RANGE * address_advance) + OPCODE_BASE;

    if (!line_increment && !address_advance) return;

    if (line_increment >= MIN_LINE_INCREMENT && line_increment <= MAX_LINE_INCREMENT) {
        if (opcode <= 255) {
            add_to_state_data(&opcode, 1);
        }
        else if (address_advance <= 2 * OP255_ADDRESS_INCREMENT) {
            // See if a DW_LNS_const_add_pc can be used, see page 101 of https://dwarfstd.org/doc/Dwarf3.pdf
            opcode = (line_increment - LINE_BASE) + (LINE_RANGE * (address_advance - OP255_ADDRESS_INCREMENT)) + OPCODE_BASE;
            if (opcode <= 255) {
                char dw_lns_const_add_pc = DW_LNS_const_add_pc;
                add_to_state_data(&dw_lns_const_add_pc, 1);
                add_to_state_data(&opcode, 1);
            }
            else {
                add_dwarf_loc_increment_line(line_increment);
                add_dwarf_loc_advance_address(address_advance);
            }

        }
        else {
            add_dwarf_loc_increment_line(line_increment);
            add_dwarf_loc_advance_address(address_advance);
        }
    }
    else {
        add_dwarf_loc_increment_line(line_increment);
        add_dwarf_loc_advance_address(address_advance);
    }

    state.address += address_advance;
    state.line_number += line_increment;
}

void init_dwarf(void) {
    next_dir_index = 1;
    dirs_list = new_list(16);
    dirs_map = new_strmap();
    files = new_list(0);

    state.address = 0;
    state.file = 1;
    state.line_number = 1;
    state.data = malloc(1024);
    state.allocated = 1024;
    state.size = 0;
    state.locs_present = 0;
}
