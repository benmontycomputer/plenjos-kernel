#pragma once

#include <stddef.h>
#include <stdint.h>

#define PT_LOAD    1
#define PT_DYNAMIC 2

#define DT_NULL     0 /* End of dynamic section */
#define DT_NEEDED   1 /* Name of needed library */
#define DT_PLTRELSZ 2 /* Size in bytes of PLT relocs */
#define DT_PLTGOT   3 /* Processor defined value */
#define DT_HASH     4 /* Address of symbol hash table */
#define DT_STRTAB   5 /* Address of string table */
#define DT_SYMTAB   6 /* Address of symbol table */

/**
 * Address of a non-PLT relocation table (there can only be one of these). When relocation is mandatory, either DT_RELA
 * or DT_REL should be present (but NOT both). If DT_RELA is present, DT_RELASZ and DT_RELAENT must also be present.
 */
#define DT_RELA    7
#define DT_RELASZ  8
#define DT_RELAENT 9

#define DT_STRSZ    10 /* Size of string table */
#define DT_SYMENT   11 /* Size of each symbol table entry */
#define DT_INIT     12 /* Address of init function */
#define DT_FINI     13 /* Address of termination function */
#define DT_SONAME   14 /* Name of shared object */
#define DT_RPATH    15 /* Library search path (deprecated) */
#define DT_SYMBOLIC 16 /* Start symbol search here */

/**
 * Address of a non-PLT relocation table (there can only be one of these). When relocation is mandatory, either DT_RELA
 * or DT_REL should be present (but NOT both). If DT_REL is present, DT_RELSZ and DT_RELENT must also be present.
 *
 * DT_REL is similar to DT_RELA, but does not have an addend field; the addend is stored in the location to be modified
 */
#define DT_REL    17
#define DT_RELSZ  18
#define DT_RELENT 19

/**
 * PLT relocation table
 */
#define DT_PLTREL 20 /* Type of PLT relocation entry */
#define DT_JMPREL 23 /* Address of PLT relocation table */

#define DT_INIT_ARRAY   25 /* Address of the array of init functions */
#define DT_FINI_ARRAY   26 /* Address of the array of fini functions */
#define DT_INIT_ARRAYSZ 27 /* Size in bytes of DT_INIT_ARRAY */
#define DT_FINI_ARRAYSZ 28 /* Size in bytes of DT_FINI_ARRAY */

// Upper and lower 32 bits, respectively
#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)

// Relocation types that we'd need to handle
#define R_X86_64_NONE      0
#define R_X86_64_64        1
#define R_X86_64_GLOB_DAT  6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE  8

typedef uint64_t ELF_addr_t;

typedef struct ELF_dyn {
    int64_t d_tag; /* 8 bytes: type of entry */
    union {
        uint64_t d_val; /* 8 bytes: integer value */
        uint64_t d_ptr; /* 8 bytes: address value */
    } d_un;
} __attribute__((packed)) ELF_dyn_t;

typedef struct ELF_rel {
    uint64_t r_offset;
    uint64_t r_info;
} __attribute__((packed)) ELF_rel_t;

typedef struct ELF_rela {
    uint64_t r_offset; /* Location to apply the relocation (virtual address) */
    uint64_t r_info;   /* Symbol index + type of relocation */
    int64_t r_addend;  /* Constant addend used in relocation */
} __attribute__((packed)) ELF_rela_t;

typedef struct ELF_sym {
    uint32_t st_name;  /* Symbol name (string table index) */
    uint8_t st_info;   /* Symbol type and binding */
    uint8_t st_other;  /* Symbol visibility */
    uint16_t st_shndx; /* Section index */
    uint64_t st_value; /* Symbol value */
    uint64_t st_size;  /* Symbol size */
} __attribute__((packed)) ELF_sym_t;

struct ELF_header {
    /* Identification */
    uint32_t magic;
    uint8_t bits;   // 1 = 32 bit; 2 = 64 bit
    uint8_t endian; // 1 = little endian (x86, default ARM); 2 = big endian
    uint8_t header_ver;
    uint8_t os_abi; // 0 for System V
    uint64_t padding;

    /* Details */
    uint16_t type;
    uint16_t isa;
    uint32_t version;
    uint64_t entry_offset;
    uint64_t ph_offset;
    uint64_t sh_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t ph_entry_size;
    uint16_t ph_entries;
    uint16_t sh_entry_size;
    uint16_t sh_entries;
    uint16_t sh_string_table_index;
} __attribute__((packed));

#define ELF_PHDR_EX 0x1
#define ELF_PHDR_W  0x2
#define ELF_PHDR_R  0x4

struct ELF_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset; // Data offset
    uint64_t vaddr;  // Virtual address
    uint64_t paddr;  // Physical address
    uint64_t filesz; // Size in file
    uint64_t memsz;  // Size in memory
    uint64_t align;  // Alignment
} __attribute__((packed));

typedef struct ELF_header ELF_header_t;
typedef struct ELF_program_header ELF_program_header_t;

typedef struct elf_object {
    ELF_header_t *ehdr;          // Usable only during loading
    ELF_program_header_t *phdrs; // Usable only during loading

    ELF_dyn_t *dynamic;
    size_t dynamic_count;

    // These point to loaded memory locations; they are valid after loading
    ELF_addr_t base;
    ELF_rela_t *rela;
    size_t rela_sz;
    size_t relaent; // Size of each rela entry; should be sizeof(ELF_rela_t) but not always

    ELF_rela_t *rela_plt;
    size_t rela_plt_sz;

    uint64_t pltgot;

    ELF_sym_t *symtab;
    const char *strtab;
};

void init_dso_base();

// _apply_relocations is defined in lsdo.c so it can be used to self-relocate the linker itself; apply_relocations is a
// wrapper for _apply_relocations and must also be defined in ldso.c.
int apply_relocations(struct elf_object *obj, ELF_rela_t *rela, size_t rela_sz, int is_jmprel);

int parse_dynamic_section(struct elf_object *obj);
ELF_addr_t resolve_symbol(const char *name);