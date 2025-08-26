#include <stdint.h>

#include "exec/elf.h"

#include "kernel.h"

#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "memory/mm_common.h"

#include "lib/stdio.h"

void loadelf(void *elf_base, pml4_t *pml4, uint64_t *entry_out, uint64_t *stack_out) {
    ELF_header_t *ehdr = (ELF_header_t *)elf_base;
    ELF_program_header_t *phdrs = (ELF_program_header_t *)((uint64_t)elf_base + ehdr->ph_offset);

    for (uint16_t i = 0; i < ehdr->ph_entries; i++) {
        ELF_program_header_t *phdr = &phdrs[i];

        if (phdr->type != 1) continue;

        for (uint64_t j = 0; j < phdr->memsz; j += PAGE_LEN) {
            uint64_t p = alloc_virtual_memory(j + phdr->vaddr, ALLOCATE_VM_EX | ALLOCATE_VM_RO | ALLOCATE_VM_USER, pml4);

            if (!p) {
                printf("Couldn't allocate memory at vaddr %p for user process. Halt!\n", j + phdr->vaddr);
                hcf();
            }

            uint64_t v = phys_to_virt(p);

            if (j >= phdr->filesz) {
                memset((void *)v, 0, PAGE_LEN);
            } else {
                if (j + PAGE_LEN >= phdr->filesz) {
                    memset((void *)v, 0, PAGE_LEN);
                    memcpy((void *)v, elf_base + phdr->offset + j, phdr->filesz - j);
                } else {
                    memcpy((void *)v, elf_base + phdr->offset + j, PAGE_LEN);
                }
            }
        }

        // memcpy((void *)phdr->vaddr, elf_base + phdr->offset, phdr->filesz);
        // memset((void *)phdr->vaddr + phdr->filesz, 0, phdr->memsz - phdr->filesz);
    }

    uint64_t entry = ehdr->entry_offset;
    // uint64_t stack = kmalloc_heap(0x1000);
    uint64_t stack = USER_START_ADDR + 0x500000;
    for (uint64_t i = 0; i < 0x4000; i += PAGE_LEN) {
        if (!alloc_virtual_memory(stack + i, ALLOCATE_VM_USER, pml4)) {
            printf("Couldn't allocate stack for user process. Halt!\n");
            hcf();
        }
    }

    *entry_out = entry;
    *stack_out = stack;
}