#pragma once

#include <stdint.h>

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