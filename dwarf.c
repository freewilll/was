#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwarf.h"
#include "elf.h"
#include "list.h"
#include "utils.h"
#include "symbols.h"

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
    int address;
    int file;
    int line;
} State;

State state;

void make_dwarf_debug_line_section(void) {
    // Only add line number information if a .debug_info section exists
    if (!get_section(".debug_info")) return;

    Section *debug_line_section = get_section(".debug_line");
    if (!debug_line_section)
        debug_line_section = add_section(".debug_line", SHT_PROGBITS, 0, 0);

    LineNumberProgramHeader header    = {0};

    header.version                    = 3; // DWARF version 3
    header.minimum_instruction_length = 1;
    header.default_is_stmt            = 1;
    header.line_base                  = -5;
    header.line_range                 = 14;
    header.opcode_base                = 13;

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

    static char zero = 0;

    // Add directories
    for (int i = 0; i < dirs_list->length; i++) {
        char *filename = dirs_list->elements[i];
        add_to_section(debug_line_section, filename, strlen(filename) + 1);
    }
    add_to_section(debug_line_section, &zero, 1); // Terminator

    // Add files
    for (int i = 0; i < files->length; i++) {
        File *file = files->elements[i];
        if (!file) simple_error("Non consecutive .file numbers");
        add_to_section(debug_line_section, file->filename, strlen(file->filename) + 1);

        char uleb128_data[8];
        int size = encode_uleb128(file->dir_index, uleb128_data);
        add_to_section(debug_line_section, uleb128_data, size);
        add_to_section(debug_line_section, &zero, 1); // Time of last modification not implemented
        add_to_section(debug_line_section, &zero, 1); // Length in bytes not implemented
    }
    add_to_section(debug_line_section, &zero, 1); // Terminator

    LineNumberProgramHeader *header_in_section = (LineNumberProgramHeader *) (debug_line_section->data + header_in_section_pos);

    header_in_section->unit_length = debug_line_section->size - 4; // Size not including the unit_length field itself

    // The number of bytes following the header_length field to the beginning of the
    // first byte of the line number program itself.
    header_in_section->header_length = debug_line_section->size - ((char *) &header.minimum_instruction_length - (char *) &header.unit_length);
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

void init_dwarf(void) {
    next_dir_index = 1;
    dirs_list = new_list(16);
    dirs_map = new_strmap();
    files = new_list(0);

    state.address = 0;
    state.file = 1;
    state.line = 1;
}
