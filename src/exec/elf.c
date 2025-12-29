#include "exec/elf.h"

#include "kernel.h"
#include "lib/stdio.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "memory/mm_common.h"

#include <stdint.h>

int elf_copy(void *dest, void *src, size_t count, pml4_t *current_pml4) {
    size_t first_page_len = PAGE_LEN - ((uint64_t)dest % PAGE_LEN);
    size_t last_page_len  = ((uint64_t)dest + count) % PAGE_LEN;

    uint64_t offs = 0;

    while (offs < count) {
        uint64_t addr  = (uint64_t)dest + offs;
        uint64_t paddr = get_physaddr((uint64_t)dest + offs, current_pml4);

        if (offs == 0) {
            // First page (or we only need 1 page); might not have to write a full page
            size_t to_copy = (count < first_page_len) ? count : first_page_len;
            memcpy((void *)phys_to_virt(paddr), src + offs, to_copy);
            offs += to_copy;
        } else if (offs + PAGE_LEN > count) {
            // Last page; might not have to write a full page
            memcpy((void *)phys_to_virt(paddr), src + offs, last_page_len);
            offs += last_page_len;
        } else {
            // Full page
            memcpy((void *)phys_to_virt(paddr), src + offs, PAGE_LEN);
            offs += PAGE_LEN;
        }
    }

    return 0;
}

void loadelf(void *elf_base, pml4_t *pml4, uint64_t *entry_out, uint64_t voffs_if_pic) {
    ELF_header_t *ehdr          = (ELF_header_t *)elf_base;
    ELF_program_header_t *phdrs = (ELF_program_header_t *)((uint64_t)elf_base + ehdr->ph_offset);

    bool ehdr_mapped = false;

    printf("Loading ELF executable at base %p, voffs %p with %d program headers (phoffs = %p)\n", elf_base,
           (void *)voffs_if_pic, ehdr->ph_entries, (void *)ehdr->ph_offset);

    for (uint16_t i = 0; i < ehdr->ph_entries; i++) {
        ELF_program_header_t *phdr = &phdrs[i];

        if (phdr->type == PT_DYNAMIC) {
            // Allow us to map
            // TODO: should we map these? I don't know.
            printf("Found PT_DYNAMIC segment at vaddr %p\n", (void *)(phdr->vaddr + voffs_if_pic));
            // Count the entries for debugging
            ELF_dyn_t *dyn   = (ELF_dyn_t *)((uint64_t)elf_base + phdr->offset);
            size_t dyn_count = phdr->memsz / sizeof(ELF_dyn_t);
            for (size_t j = 0; j < dyn_count; j++) {
                ELF_dyn_t *d = &dyn[j];
                printf(" Dynamic Entry %p: d_tag=%p, d_un.d_val=%p, d_un.d_ptr=%p\n", (void *)j, (void *)d->d_tag,
                       (void *)d->d_un.d_val, (void *)d->d_un.d_ptr);
            }
        } else if (phdr->type != 1) continue;

        uint8_t flags = ((phdr->flags & ELF_PHDR_W) ? 0 : ALLOCATE_VM_RO)
                        | ((phdr->flags & ELF_PHDR_EX) ? ALLOCATE_VM_EX : 0) | ALLOCATE_VM_USER;

        uint64_t p_start = 0;
        for (uint64_t j = 0; j < phdr->memsz; j += PAGE_LEN) {
            // uint64_t p = alloc_virtual_memory(j + phdr->vaddr, ALLOCATE_VM_EX | ALLOCATE_VM_RO | ALLOCATE_VM_USER,
            // pml4);
            uint64_t p = alloc_virtual_memory(j + phdr->vaddr + voffs_if_pic, flags, pml4)
                         + ((j + phdr->vaddr + voffs_if_pic) % PAGE_LEN);
            if (p_start == 0) {
                p_start = p;
            }

            if (phdr->type == PT_DYNAMIC) {
                printf("Mapping PT_DYNAMIC segment page at vaddr %p (file offset %p)\n",
                       (void *)(j + phdr->vaddr + voffs_if_pic), (void *)(phdr->offset + j));
            }

            if (!p) {
                printf("Couldn't allocate memory at vaddr %p for user process. Halt!\n",
                       j + phdr->vaddr + voffs_if_pic);
                hcf();
            }

            uint64_t v = phys_to_virt(p);

            memset((void *)v, 0, PAGE_LEN);

            if (j + phdr->vaddr == 0) {
                printf("Mapping ehdr at vaddr %p\n", (void *)(phdr->vaddr + voffs_if_pic));
                ehdr_mapped = true;
            }
        }

        size_t len = phdr->filesz;
        if (len > phdr->memsz) {
            len = phdr->memsz;
        }

        elf_copy((void *)(phdr->vaddr + voffs_if_pic), (void *)((uint64_t)elf_base + phdr->offset), len, pml4);

        /* if (phdr->type == PT_DYNAMIC) {
            printf("Finished mapping PT_DYNAMIC segment\nElfbase addr %p, vaddr %p, starting paddr %p, user "
                   "calculated paddr %p, memsz %p\n",
                   elf_base, (void *)(phdr->vaddr + voffs_if_pic), p_start,
                   get_physaddr((uint64_t)(phdr->vaddr + voffs_if_pic), pml4), (void *)phdr->memsz);
        
            // Verify the mapping by reading back
            ELF_dyn_t *dyn = (ELF_dyn_t *)phys_to_virt(get_physaddr((phdr->vaddr + voffs_if_pic), pml4));
            size_t dyn_count = phdr->memsz / sizeof(ELF_dyn_t);
            for (size_t j = 0; j < dyn_count; j++) {
                ELF_dyn_t *d = &dyn[j];
                printf(" Verified Dynamic Entry %p: d_tag=%p, d_un.d_val=%p, d_un.d_ptr=%p\n", (void *)j,
                       (void *)d->d_tag, (void *)d->d_un.d_val, (void *)d->d_un.d_ptr);
            }
        } */

        // memcpy((void *)phdr->vaddr, elf_base + phdr->offset, phdr->filesz);
        // memset((void *)phdr->vaddr + phdr->filesz, 0, phdr->memsz - phdr->filesz);
    }

    if (!ehdr_mapped) {
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
    *entry_out     = entry;
}