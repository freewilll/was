#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "elf.h"
#include "relocations.h"
#include "symbols.h"
#include "utils.h"
#include "was.h"

// ELF sections
//                               id name          type          flags                      alignment
ElfSection section_null      = { 0, "" ,          0,            0,                         0    };
ElfSection section_text      = { 1, ".text",      SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 0x10 };
ElfSection section_rela_text = { 2, ".rela.text", SHT_RELA,     SHF_INFO_LINK,             0x08 };
ElfSection section_data      = { 3, ".data",      SHT_PROGBITS, SHF_ALLOC | SHF_WRITE,     0x04 };
ElfSection section_bss       = { 4, ".bss",       SHT_NOBITS,   SHF_ALLOC | SHF_WRITE,     0x04 };
ElfSection section_rodata    = { 5, ".rodata",    SHT_PROGBITS, SHF_ALLOC,                 0x04 };
ElfSection section_symtab    = { 6, ".symtab",    SHT_SYMTAB,   0,                         0x08 };
ElfSection section_strtab    = { 7, ".strtab",    SHT_STRTAB,   0,                         0x01 };
ElfSection section_shstrtab  = { 8, ".shstrtab",  SHT_STRTAB,   0,                         0x01 };

ElfSection *sections[] = {
    &section_null,
    &section_text,
    &section_rela_text,
    &section_data,
    &section_bss,
    &section_rodata,
    &section_symtab,
    &section_strtab,
    &section_shstrtab,
};
const int section_count = sizeof(sections) / sizeof(char *);

static ElfSection *current_section = &section_text; // Current section code/data is being added to
static int local_symbol_end = 0;                    // Index of last local symbol


#define ALIGN(address, alignment) ((address) + (alignment) - 1) & (~((alignment) - 1));

// Lookup the section by name and make it the current section code/data is being
// appended to.
void set_current_section(char *name) {
    for (int i = 0; i < section_count; i++) {
        ElfSection *s = sections[i];
        if (!strcmp(s->name, name)) {
            current_section = s;
            return;
        }
    }

    simple_error("Unknown section %s", name);
}

// Return the size of the current section
ElfSection *get_current_section(void) {
    return current_section;
}

// Return the size of the current section
int get_current_section_size(void) {
    return current_section->size;
}

// Allocate space at the end of a section and return a pointer to it.
// Dynamically allocate space size as needed.
static void *allocate_in_section(ElfSection *section, int size) {
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
static int add_to_section(ElfSection *section, void *src, int size) {
    char *data = allocate_in_section(section, size);
    memcpy(data, src, size);
    return data - section->data;
}

// Copy src to the end of the current section and return the offset
int add_to_current_section(void *src, int size) {
    return add_to_section(current_section, src, size);
}

// Copy src to the end of a section and return the offset
static int add_zeros_to_section(ElfSection *section, int size) {
    char *data = allocate_in_section(section, size);
    memset(data, 0, size);
    return data - section->data;
}

// Copy src to the end of the current section and return the offset
int add_zeros_to_current_section(int size) {
    return add_zeros_to_section(current_section, size);
}

// Add a symbol to the ELF symbol table symtab
static int add_elf_symbol(char *name, long value, int binding, int type, int section_index) {
    // Add a string to the strtab unless name is "".
    // Empty names are all mapped to the first entry in the string table.
    int strtab_offset = *name ? add_to_section(&section_strtab, name, strlen(name) + 1) : 0;

    ElfSymbol *symbol = allocate_in_section(&section_symtab, sizeof(ElfSymbol));
    memset(symbol, 0, sizeof(ElfSymbol));
    symbol->st_name = strtab_offset;
    symbol->st_value = value;
    symbol->st_info = (binding << 4) + type;
    symbol->st_shndx = section_index;

    int index = symbol - (ElfSymbol *) section_symtab.data;

    if (binding == STB_LOCAL) local_symbol_end = index;

    return index;
}

// Add a special symbol with the source filename
void add_file_symbol(char *filename) {
    add_elf_symbol(filename, 0, STB_LOCAL, STT_FILE, SHN_ABS);
}

// Point the symbol offset and section_index to the current section & offset in it
void associate_symbol_with_current_section(Symbol *symbol) {
    symbol->offset = current_section->size;
    symbol->section_index = current_section->index;
}

// Add a relocation to the ELF rela_text section.
void add_elf_relocation(int type, int symbol_index, long offset, long addend) {
    ElfRelocation *r = allocate_in_section(&section_rela_text, sizeof(ElfRelocation));

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
    elf_header->e_shnum     = section_count;                 // Number of section header entries
    elf_header->e_shstrndx  = section_shstrtab.index;        // The section header string table index
}

// Make ELF section header
static void make_section_header(ElfSectionHeader *sh, ElfSection *section) {
    sh->sh_name = add_to_section(&section_shstrtab, section->name, strlen(section->name) + 1);
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
    *psection_headers_size = sizeof(ElfSectionHeader) * section_count;
    *psection_headers = calloc(1, *psection_headers_size);

    for (int i = 0; i < section_count; i++)
        make_section_header(&(*psection_headers)[i], sections[i]);
}

// Add non-global, then global symbols to the symtab section
static void make_symbols_section(void) {
    // Add non-global symbols
    for (StrMapIterator it = strmap_iterator(symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(symbols, name);

        // All undefined symbols must be global.
        if (!symbol->section_index) symbol->binding = STB_GLOBAL;

        if (symbol->binding != STB_GLOBAL)
            symbol->symtab_index = add_elf_symbol(name, symbol->offset, symbol->binding, symbol->type, symbol->section_index);
    }

    // Add global symbols
    for (StrMapIterator it = strmap_iterator(symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(symbols, name);

        if (symbol->binding == STB_GLOBAL)
            symbol->symtab_index = add_elf_symbol(name, 0, symbol->binding, symbol->type, symbol->section_index);
    }

    section_symtab.link = section_strtab.index;
    section_symtab.info = local_symbol_end + 1; // Index of the first global symbol
    section_symtab.entsize = sizeof(ElfSymbol);
}

// Make ELF relocations section
void make_rela_text_section(void) {
    add_elf_relocations();

    section_rela_text.link = section_symtab.index;
    section_rela_text.info = section_text.index;
    section_rela_text.entsize = sizeof(ElfRelocation);
}

// Determine offsets for all the sections within the final ELF file.
static int layout_elf_sections(ElfSectionHeader *section_headers) {
    int shdr_size = sizeof(ElfSectionHeader);

    // Determine section offsets
    int offset = ALIGN(sizeof(ElfHeader) + shdr_size * section_count, 16);
    for (int i = 1; i < section_count; i++) {
        ElfSection *section = sections[i];
        section->start = offset;
        section_headers[i].sh_offset = offset;
        offset = ALIGN(offset + section->size, 16);
    }

    return offset;
}

// Copy all the section data to the final positions in the ELF file.
void copy_sections_to_elf(char *program) {
    for (int i = 0; i < section_count; i++) {
        ElfSection *section = sections[i];
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

    make_symbols_section();
    make_rela_text_section();
    make_section_headers(&section_headers_size, &section_headers);

    int size = layout_elf_sections(section_headers);
    char *program = calloc(1, size);

    make_elf_header((ElfHeader *) program);
    memcpy(program + sizeof(ElfHeader), section_headers, section_headers_size);
    copy_sections_to_elf(program);

    write_elf_file(filename, program, size);
}

void init_elf() {
    init_symbols();
    init_relocations();

    // Start string table entries at 1, so that the zero value goes to an empty string
    add_to_section(&section_strtab, "", 1);

    add_elf_symbol("", 0, STB_LOCAL, STT_NOTYPE,  SHN_UNDEF); // Null symbol

    // By convention, the section indexes correspond with the symbol table indexes
    for (int i = 1; i < section_count; i++)
        add_elf_symbol("", 0, STB_LOCAL, STT_SECTION, sections[i]->index);
}
