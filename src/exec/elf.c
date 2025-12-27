#include <stdint.h>

#include "exec/elf.h"

#include "kernel.h"

#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "memory/mm_common.h"

#include "lib/stdio.h"

void loadelf(void *elf_base, pml4_t *pml4, uint64_t *entry_out, uint64_t voffs_if_pic) {
    ELF_header_t *ehdr = (ELF_header_t *)elf_base;
    ELF_program_header_t *phdrs = (ELF_program_header_t *)((uint64_t)elf_base + ehdr->ph_offset);

    printf("Loading ELF executable at base %p with %d program headers (phoffs = %p)\n", elf_base, ehdr->ph_entries, (void *)ehdr->ph_offset);

    for (uint16_t i = 0; i < ehdr->ph_entries; i++) {
        ELF_program_header_t *phdr = &phdrs[i];

        if (phdr->type == PT_DYNAMIC) {
            // Allow us to map
            printf("Found PT_DYNAMIC segment at vaddr %p\n", (void *)(phdr->vaddr + voffs_if_pic));
        } else if (phdr->type != 1) continue;

        uint8_t flags = ((phdr->flags & ELF_PHDR_W) ? 0 : ALLOCATE_VM_RO) | ((phdr->flags & ELF_PHDR_EX) ? ALLOCATE_VM_EX : 0) | ALLOCATE_VM_USER;

        for (uint64_t j = 0; j < phdr->memsz; j += PAGE_LEN) {
            // uint64_t p = alloc_virtual_memory(j + phdr->vaddr, ALLOCATE_VM_EX | ALLOCATE_VM_RO | ALLOCATE_VM_USER, pml4);
            uint64_t p = alloc_virtual_memory(j + phdr->vaddr + voffs_if_pic, flags, pml4);

            if (!p) {
                printf("Couldn't allocate memory at vaddr %p for user process. Halt!\n", j + phdr->vaddr + voffs_if_pic);
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

            if (j + phdr->vaddr == 0) {
                printf("Mapping ehdr at vaddr %p\n", (void *)(phdr->vaddr + voffs_if_pic));
            }
        }

        // memcpy((void *)phdr->vaddr, elf_base + phdr->offset, phdr->filesz);
        // memset((void *)phdr->vaddr + phdr->filesz, 0, phdr->memsz - phdr->filesz);
    }

    if (get_physaddr(voffs_if_pic, pml4) != 0) {
        printf("Warning: ehdr already mapped!\n");
    } else {
        // Map the ELF header itself
        uint64_t p = alloc_virtual_memory(voffs_if_pic, ALLOCATE_VM_RO | ALLOCATE_VM_USER, pml4);
        if (!p) {
            printf("Couldn't allocate memory for ELF header mapping. Halt!\n");
            hcf();
        }

        uint64_t v = phys_to_virt(p);
        memcpy((void *)v, elf_base, sizeof(ELF_header_t));
    }

    uint64_t entry = ehdr->entry_offset + voffs_if_pic;
    *entry_out = entry;
}