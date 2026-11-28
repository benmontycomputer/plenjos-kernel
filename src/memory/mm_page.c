#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/cpuid/cpuid.h"
#include "arch/x86_64/irq.h"
#include "cpu/cpu.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "memory/detect.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"

#include <stdbool.h>
#include <stdint.h>

// Partially modified from KeblaOS

// TODO: figure out on core-by-core basis if TLB shootdown is needed

pml4_t *kernel_pml4;
uint64_t kernel_pml4_phys;

// return current cr3 address i.e. root pml4 pointer address
uint64_t get_cr3_addr() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3)); // Read the CR3 register

    return cr3;
}

void set_cr3_addr(uint64_t cr3) {
    asm volatile("movq %0, %%cr3" : : "r"(cr3));
}

// pml4 = pointer map level 4, pdpt = page directory pointer table, pd = page directory, pt = page table
static inline void addr_split(uint64_t virt, uint32_t *pml4, uint32_t *pdpt, uint32_t *pd, uint32_t *pt) {
    *pml4 = ((virt) >> 39) & 0x1FF; // 39-47
    *pdpt = ((virt) >> 30) & 0x1FF; // 30-38
    *pd   = ((virt) >> 21) & 0x1FF; // 21-29
    *pt   = ((virt) >> 12) & 0x1FF; // 12-20
}

#define PTE_ADDR(x) ((x) & 0x000FFFFFFFFFF000ULL)

void init_paging() {
    kernel_pml4_phys = get_cr3_addr();
    kernel_pml4      = (pml4_t *)phys_to_virt(kernel_pml4_phys);

    if (!kernel_pml4_phys) {
        printf("\nNo page table! Halt.\n");
        hcf();
    }

    printf("Page table vaddr: %p, phys addr: %p\n", kernel_pml4, kernel_pml4_phys);
}

void populate_kernel_half_pml4() {
    for (int i = 256; i < 512; i++) {
        if (!kernel_pml4->entries[i].present) {
            uint64_t phys = find_next_free_frame();

            void *virt = (void *)phys_to_virt(phys);
            memset(virt, 0, PAGE_LEN);

            kernel_pml4->entries[i].present   = 1;
            kernel_pml4->entries[i].rw        = 1;
            kernel_pml4->entries[i].base_addr = phys >> 12;
        }
    }
}

void free_user_page_table(pml4_t *pml4_virt) {
    uint64_t *pml4 = (uint64_t *)pml4_virt;

    for (size_t pml4_i = 0; pml4_i < 256; pml4_i++) {
        uint64_t pml4e = pml4[pml4_i];
        if (!(pml4e & PAGE_FLAG_PRESENT)) continue;

        uint64_t *pdpt = (uint64_t *)phys_to_virt(PTE_ADDR(pml4e));
        for (size_t pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
            uint64_t pdpte = pdpt[pdpt_i];
            if (!(pdpte & PAGE_FLAG_PRESENT)) continue;

            // 1 GiB huge page?
            /* if (pdpte & PTE_PS) {
                free_page((void *)PTE_ADDR(pdpte));
                continue;
            } */

            uint64_t *pd = (uint64_t *)phys_to_virt(PTE_ADDR(pdpte));
            for (size_t pd_i = 0; pd_i < 512; pd_i++) {
                uint64_t pde = pd[pd_i];
                if (!(pde & PAGE_FLAG_PRESENT)) continue;

                // 2 MiB huge page?
                /* if (pde & PTE_PS) {
                    free_page((void *)PTE_ADDR(pde));
                    continue;
                } */

                phys_mem_unref_frame(phys_addr_to_frame_struct(PTE_ADDR(pde))); // free page table
            }
            phys_mem_unref_frame(phys_addr_to_frame_struct(PTE_ADDR(pdpte))); // free page directory
        }
        phys_mem_unref_frame(phys_addr_to_frame_struct(PTE_ADDR(pml4e))); // free pdpt
    }
    phys_mem_unref_frame(phys_addr_to_frame_struct(virt_to_phys((uint64_t)pml4_virt))); // free pml4
}