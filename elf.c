#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "was.h"
#include "elf.h"

typedef struct section {
    int number;     // Index in the section header table
    char *name;     // Name of the section
    int type;       // Type of the section
    int flags;      // Section attributes
    int align;      // Contains the required alignment of the section. This field must be a power of two.
    int link;       // Contains the section index of an associated section.
    int info;       // Contains extra information about the section.
    char *data;     // Contents of the section
    int size;       // Size of the section
    int start;      // Start address of the section in the ELF
    long entsize;   // Contains the size, in bytes, of each entry, for sections that contain fixed-size entries. Otherwise, this field contains zero.
} Section;

// Sections
//                           num name        type          flags                      align
Section section_null     = { 0, "" ,         0,            0,                         0    };
Section section_text     = { 1, ".text",     SHT_PROGBITS, SHF_ALLOC + SHF_EXECINSTR, 0x10 };
Section section_data     = { 2, ".data",     SHT_PROGBITS, SHF_WRITE + SHF_ALLOC,     0x04 };
Section section_bss      = { 3, ".bss",      SHT_NOBITS,   SHF_WRITE + SHF_ALLOC,     0x04 };
Section section_symtab   = { 4, ".symtab",   SHT_SYMTAB,   0,                         0x08 };
Section section_strtab   = { 5, ".strtab",   SHT_STRTAB,   0,                         0x01 };
Section section_shstrtab = { 6, ".shstrtab", SHT_STRTAB,   0,                         0x01 };

const int section_count = 7;
Section *sections[] = {
    &section_null,
    &section_text,
    &section_data,
    &section_bss,
    &section_symtab,
    &section_strtab,
    &section_shstrtab,
};

static int symbol_count;        // Amount of symbols
static char **shstrtab;         // Section header string table
static int *shstrtab_indexes;   // Section header string table indexes
static int shdr_start;          // Start address of the section header
static int total_size;          // Total size of the ELF


#define ALIGN(address, alignment) ((address) + (alignment) - 1) & (~((alignment) - 1));

static void make_string_list(char** strings, int len, char **string_list, int *indexes, int *size) {
    *size = 0;
    for (int i = 0; i < len; i++) *size += strlen(strings[i]) + 1;

    char *result = malloc(*size);

    char *dst = result;
    for (int i = 0; i < len; i++) {
        char *src = strings[i];
        indexes[i] = dst - result;
        while (*dst++ = *src++);
    }

    *string_list = result;
}

static void layout_elf_sections(void) {
    int shdr_size;
    shdr_size = sizeof(struct section_header);

    // Make section headers string table data
    shstrtab = malloc(sizeof(char *) * section_count);
    shstrtab_indexes = malloc(sizeof(int) * section_count);
    for (int i = 0; i < section_count; i++)
        shstrtab[i] = sections[i]->name;
    make_string_list(shstrtab, section_count, &section_shstrtab.data, shstrtab_indexes, &section_shstrtab.size);

    section_symtab.size = symbol_count * sizeof(struct symbol);

    // Determine section offsets
    shdr_start = sizeof(struct elf_header);

    int offset = ALIGN(shdr_start + shdr_size * section_count, 16);
    for (int i = 1; i < section_count; i++) {
        Section *section = sections[i];
        section->start = offset;
        offset = ALIGN(offset + section->size, 16);
    }

    total_size = offset;
}

void add_section_header(char *headers, Section *section) {
    struct section_header *sh;
    sh = (struct section_header *) (headers + sizeof(struct section_header) * section->number);

    sh->sh_name      = shstrtab_indexes[section->number];
    sh->sh_type      = section->type;
    sh->sh_flags     = section->flags;
    sh->sh_offset    = section->start;
    sh->sh_size      = section->size;
    sh->sh_link      = section->link;
    sh->sh_info      = section->info;
    sh->sh_addralign = section->align;
    sh->sh_entsize   = section->entsize;
}

void write_elf(char *filename) {
    layout_elf_sections();

    char *program = malloc(total_size);
    memset(program, 0, total_size);

    struct elf_header *elf_header = (struct elf_header *) program;

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
    elf_header->e_shoff     = shdr_start;                    // Offset to section header table
    elf_header->e_ehsize    = sizeof(struct elf_header);     // The size of this header, 0x40 for 64-bit
    elf_header->e_phentsize = 0;                             // The size of the program header
    elf_header->e_phnum     = 0;                             // Number of program header entries
    elf_header->e_shentsize = sizeof(struct section_header); // The size of the section header
    elf_header->e_shnum     = section_count;                 // Number of section header entries
    elf_header->e_shstrndx  = SEC_SHSTRTAB;                  // The string table index is the fourth section

    // Set some oddballs in the section header
    section_symtab.link = SEC_STRTAB;
    section_symtab.info = symbol_count;
    section_symtab.entsize = sizeof(struct symbol);

    // Set section headers + copy data
    for (int i = 1; i < section_count; i++) {
        Section *section = sections[i];
        add_section_header(program + shdr_start, section);
        memcpy(&program[section->start], section->data, section->size);
    }

    // Write output file
    FILE *f;
    if (!strcmp(filename, "-")) {
        f = stdout;
    }
    else {
        f = fopen(filename, "wb");
        if (!f) { perror("Unable to open write output file"); exit(1); }
    }

    int written = fwrite(program, 1, total_size, f);
    if (written < 0) { perror("Unable to write to output file"); exit(1); }
    fclose(f);
}
