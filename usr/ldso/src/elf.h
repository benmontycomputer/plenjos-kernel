#pragma once

#include <stddef.h>
#include <stdint.h>

#define PT_DYNAMIC 2

#define DT_NULL   0
#define DT_RELA   7
#define DT_RELASZ 8

#define R_X86_64_RELATIVE 8

// Lower 32 bits
#define ELF64_R_TYPE(i) ((uint32_t)(i))

typedef struct ELF_dyn {
    int64_t  d_tag;     /* 8 bytes: type of entry */
    union {
        uint64_t d_val; /* 8 bytes: integer value */
        uint64_t d_ptr; /* 8 bytes: address value */
    } d_un;
} __attribute__((packed)) ELF_dyn_t;

typedef struct ELF_rela {
    uint64_t r_offset;  /* Location to apply the relocation (virtual address) */
    uint64_t r_info;    /* Symbol index + type of relocation */
    int64_t  r_addend;  /* Constant addend used in relocation */
} __attribute__((packed)) ELF_rela_t;

struct ELF_header {
    /* Identification */
    uint32_t magic;
    uint8_t bits; // 1 = 32 bit; 2 = 64 bit
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
#define ELF_PHDR_W 0x2
#define ELF_PHDR_R 0x4

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

};

// The executable is not checked for execute permissions here; the kernel is responsible for that.
int loadelf(const char *path, struct elf_object *out_obj);