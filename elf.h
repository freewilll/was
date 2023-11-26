#ifndef _ELF_H
#define _ELF_H

// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h

#define SHN_UNDEF         0        // This section table index means the symbol is undefined. When the link editor combines this object file with another that defines the indicated symbol, this file's references to the symbol will be linked to the actual definition.
#define SHN_ABS           65521    // The symbol has an absolute value that will not change because of relocation.#define
#define SHF_WRITE         1
#define SHF_ALLOC         2
#define SHF_EXECINSTR     4

#define SEC_NULL        0
#define SEC_TEXT        1
#define SEC_DATA        2
#define SEC_BSS         3
#define SEC_SYMTAB      4
#define SEC_STRTAB      5
#define SEC_SHSTRTAB    6

#define SHT_PROGBITS    0x1
#define SHT_SYMTAB      0x2
#define SHT_STRTAB      0x3
#define SHT_RELA        0x4
#define SHT_NOBITS      0x08
#define E_MACHINE_TYPE_X86_64   0x3e
#define ET_REL   1                         // relocatable


struct elf_header {
    char   ei_magic0;       // 0x7F followed by ELF(45 4c 46) in ASCII; these four bytes constitute the magic number.
    char   ei_magic1;
    char   ei_magic2;
    char   ei_magic3;
    char   ei_class;        // This byte is set to either 1 or 2 to signify 32- or 64-bit format, respectively.
    char   ei_data;         // This byte is set to either 1 or 2 to signify little or big endianness, respectively.
    char   ei_version;      // Set to 1 for the original version of ELF.
    char   ei_osabi;        // Identifies the target operating system ABI.
    char   ei_osabiversion; // Further specifies the ABI version.
    char   pad0;            // Unused
    char   pad1;            // Unused
    char   pad2;            // Unused
    char   pad3;            // Unused
    char   pad4;            // Unused
    char   pad5;            // Unused
    char   pad6;            // Unused
    short  e_type;          // File type.
    short  e_machine;       // Machine architecture.
    int    e_version;       // ELF format version.
    long   e_entry;         // Entry point.
    long   e_phoff;         // Program header file offset.
    long   e_shoff;         // Section header file offset.
    int    e_flags;         // Architecture-specific flags.
    short  e_ehsize;        // Size of ELF header in bytes.
    short  e_phentsize;     // Size of program header entry.
    short  e_phnum;         // Number of program header entries.
    short  e_shentsize;     // Size of section header entry.
    short  e_shnum;         // Number of section header entries.
    short  e_shstrndx;      // Section name strings section.
};

struct section_header {
    int  sh_name;           // An offset to a string in the .shstrtab section that represents the name of this section
    int  sh_type;           // Identifies the type of this header.
    long sh_flags;          // Identifies the attributes of the section.
    long sh_addr;           // Virtual address of the section in memory, for sections that are loaded.
    long sh_offset;         // Offset of the section in the file image.
    long sh_size;           // Size in bytes of the section in the file image. May be 0.
    int  sh_link;           // Contains the section index of an associated section.
    int  sh_info;           // Contains extra information about the section.
    long sh_addralign;      // Contains the required alignment of the section. This field must be a power of two.
    long sh_entsize;        // Contains the size, in bytes, of each entry, for sections that contain fixed-size entries. Otherwise, this field contains zero.
};

struct symbol {
    int   st_name;          // This member holds an index into the object file's symbol string table
    char  st_info;          // This member specifies the symbol's type (low 4 bits) and binding (high 4 bits) attributes
    char  st_other;         // This member currently specifies a symbol's visibility.
    short st_shndx;         // Every symbol table entry is defined in relation to some section. This member holds the relevant section header table index.
    long  st_value;         // This member gives the value of the associated symbol. Depending on the context, this may be an absolute value, an address, and so on; details appear
    long  st_size;          // Many symbols have associated sizes. For example, a data object's size is the number of bytes contained in the object. This member holds 0 if the symbol has no size or an unknown size.
};

struct relocation {
    long r_offset;          // Location to be relocated
    long r_info;            // Relocation type (low 32 bits) and symbol index (high 32 bits).
    long r_addend;          // Addend
};

void write_elf(char *filename);

#endif
