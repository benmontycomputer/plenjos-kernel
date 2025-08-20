#include <stdint.h>

#include "exec/elf.h"

#include "kernel.h"

void loadelf(void *elf_base) {
    ELF_header_t *ehdr = (ELF_header_t *)elf_base;
    ELF_program_header_t *phdrs = (ELF_program_header_t *)((uint64_t)elf_base + ehdr->ph_offset);

    for (uint16_t i = 0; i < ehdr->ph_entries; i++) {
        ELF_program_header_t *phdr = &phdrs[i];

        if (phdr->type != 1) continue;

        memcpy((void *)phdr->vaddr, elf_base + phdr->offset, phdr->filesz);
        memset((void *)phdr->vaddr + phdr->filesz, 0, phdr->memsz - phdr->filesz);
    }

    uint64_t entry = ehdr->entry_offset;
    //uint64_t stack = malloc_uheap()
}