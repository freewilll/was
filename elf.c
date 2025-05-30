#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "elf.h"
#include "list.h"
#include "strmap.h"
#include "utils.h"
#include "was.h"

// ELF sections
Section *section_text;
Section *section_data;
Section *section_bss;
Section *section_rodata;
Section *section_symtab;
Section *section_strtab;
Section *section_shstrtab;

int local_symbol_end = 0;    // Index of last local symbol

List *sections_list;
static StrMap *sections_map;

Section *add_elf_section(char *name, int type, int flags, int align) {
    Section *section = calloc(1, sizeof(Section));
    section->index = sections_list->length;
    section->name = strdup(name);
    section->type = type;
    section->flags = flags;
    section->align = align;

    append_to_list(sections_list, section);
    strmap_put(sections_map, name, section);

    return section;
}

void init_sections(void) {
    sections_list = new_list(8);
    sections_map = new_strmap();
}

void make_section_indexes(void) {
    // Rearrange sections list so that .symtab, .strtab and .shstrtab are last
    List *new_sections_list = new_list(sections_list->length);
    for (int i = 0; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];
        if (section == section_symtab || section == section_strtab || section == section_shstrtab) continue;
        append_to_list(new_sections_list, section);
    }
    append_to_list(new_sections_list, section_symtab);
    append_to_list(new_sections_list, section_strtab);
    append_to_list(new_sections_list, section_shstrtab);

    free_list(sections_list);
    sections_list = new_sections_list;

    for (int i = 1; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];
        section->index = i;
    }
}

// May return NULL if not existent
Section *get_section(char *name) {
    return strmap_get(sections_map, name);
}

// Allocate space at the end of a section and return a pointer to it.
// Dynamically allocate space size as needed.
static void *allocate_in_section(Section *section, int size) {
    int new_section_size = section->size + size;
    if (new_section_size > section->allocated) {
        if (!section->allocated) section->allocated = 1;
        while (new_section_size > section->allocated) section->allocated *= 2;
        section->data = realloc(section->data, section->allocated);
    }

    void *result = section->data + section->size;
    section->size = new_section_size;

    return result;
}

// Copy src to the end of a section and return the offset
int add_to_section(Section *section, void *src, int size) {
    char *data = allocate_in_section(section, size);
    memcpy(data, src, size);
    return data - section->data;
}

// Add size repeated characters to the section and return the offset
int add_repeated_value_to_section(Section *section, char value, int size) {
    char *data = allocate_in_section(section, size);
    memset(data, value, size);
    return data - section->data;
}

// Add size zeros to the section and return the offset
int add_zeros_to_section(Section *section, int size) {
    return add_repeated_value_to_section(section, 0, size);
}

// Add a symbol to the ELF symbol table symtab
// This function must be called with all local symbols first, then all global symbols
int add_elf_symbol(char *name, long value, long size, int binding, int type, int section_index) {
    // Add a string to the strtab unless name is "".
    // Empty names are all mapped to the first entry in the string table.
    int strtab_offset = *name ? add_to_section(section_strtab, name, strlen(name) + 1) : 0;

    ElfSymbol *symbol = allocate_in_section(section_symtab, sizeof(ElfSymbol));
    memset(symbol, 0, sizeof(ElfSymbol));
    symbol->st_name = strtab_offset;
    symbol->st_value = value;
    symbol->st_size = size;
    symbol->st_info = (binding << 4) + type;
    symbol->st_shndx = section_index;

    int index = symbol - (ElfSymbol *) section_symtab->data;

    if (binding == STB_LOCAL) local_symbol_end = index;

    return index;
}

// Add a special symbol with the source filename
void add_file_symbol(char *filename) {
    add_elf_symbol(filename, 0, 0, STB_LOCAL, STT_FILE, SHN_ABS);
}

// Add a relocation to the ELF rela_text section.
void add_elf_relocation(Section *section, int type, int symbol_index, long offset, long addend) {
    ElfRelocation *r = allocate_in_section(section, sizeof(ElfRelocation));

    r->r_offset = offset;
    r->r_info = type + ((long) symbol_index << 32);
    r->r_addend = addend;
}

// Populate the ELF header
static void make_elf_header(ElfHeader *elf_header) {
    // ELF header
    elf_header->ei_magic0 = 0x7f;                           // Magic
    elf_header->ei_magic1 = 'E';
    elf_header->ei_magic2 = 'L';
    elf_header->ei_magic3 = 'F';
    elf_header->ei_class    = 2;                             // 64-bit
    elf_header->ei_data     = 1;                             // LSB
    elf_header->ei_version  = 1;                             // Original ELF version
    elf_header->ei_osabi    = 0;                             // Unix System V
    elf_header->e_type      = ET_REL;                        // ET_REL Relocatable
    elf_header->e_machine   = E_MACHINE_TYPE_X86_64;         // x86-64
    elf_header->e_version   = 1;                             // EV_CURRENT Current version of ELF
    elf_header->e_phoff     = 0;                             // Offset to program header table
    elf_header->e_shoff     = sizeof(ElfHeader);             // Offset to section header table
    elf_header->e_ehsize    = sizeof(ElfHeader);             // The size of this header, 0x40 for 64-bit
    elf_header->e_phentsize = 0;                             // The size of the program header
    elf_header->e_phnum     = 0;                             // Number of program header entries
    elf_header->e_shentsize = sizeof(ElfSectionHeader);      // The size of the section header
    elf_header->e_shnum     = sections_list->length;         // Number of section header entries
    elf_header->e_shstrndx  = section_shstrtab->index;       // The section header string table index
}

// Make ELF section header
static void make_section_header(ElfSectionHeader *sh, Section *section) {
    sh->sh_name      = add_to_section(section_shstrtab, section->name, strlen(section->name) + 1);
    sh->sh_type      = section->type;
    sh->sh_flags     = section->flags;
    sh->sh_offset    = section->start;
    sh->sh_size      = section->size;
    sh->sh_link      = section->link;
    sh->sh_info      = section->info;
    sh->sh_addralign = section->align;
    sh->sh_entsize   = section->entsize;
}

void make_section_headers(int *psection_headers_size, ElfSectionHeader **psection_headers) {
    *psection_headers_size = sizeof(ElfSectionHeader) * sections_list->length;
    *psection_headers = calloc(1, *psection_headers_size);

    for (int i = 0; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];
        make_section_header(&(*psection_headers)[i], section);
    }
}

// Determine offsets for all the sections within the final ELF file.
static int layout_elf_sections(ElfSectionHeader *section_headers) {
    int shdr_size = sizeof(ElfSectionHeader);

    // Determine section offsets
    int offset = ALIGN_UP(sizeof(ElfHeader) + shdr_size * sections_list->length, 16);
    for (int i = 1; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];
        section->start = offset;
        section_headers[i].sh_offset = offset;
        offset = ALIGN_UP(offset + section->size, 16);
    }

    return offset;
}

// Copy all the section data to the final positions in the ELF file.
void copy_sections_to_elf(char *program) {
    for (int i = 0; i < sections_list->length; i++) {
        Section *section = sections_list->elements[i];

        // All sections have data other than bss
        if (section->index != section_bss->index)
            memcpy(&program[section->start], section->data, section->size);
    }
}

// Write the ELF file
void write_elf_file(char *filename, void *program, int size) {
    // Write output file
    FILE *f;
    if (!strcmp(filename, "-")) {
        f = stdout;
    }
    else {
        f = fopen(filename, "wb");
        if (!f) { perror("Unable to open write output file"); exit(1); }
    }

    int written = fwrite(program, 1, size, f);
    if (written < 0) { perror("Unable to write to output file"); exit(1); }
    fclose(f);
}

// Final stage of the assembly
void finish_elf(char *filename) {
    int section_headers_size;
    ElfSectionHeader *section_headers;

    make_section_headers(&section_headers_size, &section_headers);

    int size = layout_elf_sections(section_headers);
    char *program = calloc(1, size);

    make_elf_header((ElfHeader *) program);
    memcpy(program + sizeof(ElfHeader), section_headers, section_headers_size);
    copy_sections_to_elf(program);

    write_elf_file(filename, program, size);
}
