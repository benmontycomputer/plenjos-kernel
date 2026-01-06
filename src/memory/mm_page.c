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

static inline void flush_ipi_all() {
    if (!smp_loaded) return;

    uint32_t curr_core = get_curr_core();

    for (uint32_t i = 0; i < get_n_cores(); i++) {
        if (cpu_cores[i].online && i != curr_core) {
            send_ipi(cpu_cores[i].lapic_id, IPI_TLB_SHOOTDOWN_IRQ + 32); // IPI for tlb shootdown
        }
    }
}

// Function to flush the entire TLB (by writing to cr3)
static inline void flush_tlb_all() {
    uint64_t cr3;
    // Get the current value of CR3 (the base of the PML4 table)
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    // Write the value of CR3 back to itself, which will flush the TLB
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

    uint32_t curr_core = get_curr_core();

    flush_ipi_all();
}

// MAKE SURE TO SEND THE IPI AFTER CALLING THIS FUNCTION
static inline void __native_flush_tlb_single(uint64_t addr) {
    asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

// Function to allocate a new page table / page directory / page directory pointer table
uint64_t *alloc_paging_node() {
    uint64_t *pt = (uint64_t *)find_next_free_frame();
    if (!pt) {
        printf("Paging error; failed to allocate PT / PD / PDPT\n");
        return NULL; // Allocation failed
    }
    pt = (uint64_t *)phys_to_virt((uint64_t)pt);
    memset(pt, 0, PAGE_LEN); // Zero out the page table
    return pt;
}

page_t *find_page_using_alloc(uint64_t virt, bool autocreate, uint64_t *alloc_func(), int user, pml4_t *pml4) {
    uint32_t i_pml4, i_pdpt, i_pd, i_pt;
    uint64_t *pml4_table, *pdpt_table, *pd_table, *pt_table, *pg;

    pml4_table = (uint64_t *)pml4;

    if (!pml4) {
        printf("No page table? Can't find_page().\n");

        return NULL;
    }

    addr_split(virt, &i_pml4, &i_pdpt, &i_pd, &i_pt);

    // int user = is_userspace(virt);

    uint64_t flags = PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | (user && PAGE_FLAG_USER);
    // printf("%p %p\n", pml4_table, i_pml4);

    // Search for a page directory pointer table; create if not found
    if (!(pml4_table[i_pml4] & PAGE_FLAG_PRESENT)) {
        if (!autocreate) return NULL;
        // Allocate a new page directory pointer table and add it to the pml4 table
        pdpt_table         = alloc_func();
        pml4_table[i_pml4] = (virt_to_phys((uint64_t)pdpt_table) | flags);
    } else {
        // Retrieve the page directory pointer table, making sure to discard unnecessary bits.
        if (user) pml4_table[i_pml4] |= PAGE_FLAG_USER;
        pdpt_table = (uint64_t *)phys_to_virt((uint64_t)pml4_table[i_pml4] & PHYSADDR_MASK);
    }

    // Search for a page directory table; create if not found
    if (!(pdpt_table[i_pdpt] & PAGE_FLAG_PRESENT)) {
        if (!autocreate) return NULL;
        // Allocate a new page directory table and add it to the page directory pointer table
        pd_table           = alloc_func();
        pdpt_table[i_pdpt] = (virt_to_phys((uint64_t)pd_table) | flags);
    } else {
        // Retrieve the page directory table, making sure to discard unnecessary bits
        if (user) pdpt_table[i_pdpt] |= PAGE_FLAG_USER;
        pd_table = (uint64_t *)phys_to_virt((uint64_t)pdpt_table[i_pdpt] & PHYSADDR_MASK);
    }

    // Search for a page table; create if not found
    if (!(pd_table[i_pd] & PAGE_FLAG_PRESENT)) {
        if (!autocreate) return NULL;
        // Allocate a new page table and add it to the page directory table
        pt_table       = alloc_func();
        pd_table[i_pd] = (virt_to_phys((uint64_t)pt_table) | flags);
    } else {
        // Retrieve the page table, making sure to discard unnecessary bits
        if (user) pd_table[i_pd] |= PAGE_FLAG_USER;
        pt_table = (uint64_t *)phys_to_virt((uint64_t)pd_table[i_pd] & PHYSADDR_MASK);
    }

    pg = &pt_table[i_pt];

    if (!(*pg & PAGE_FLAG_PRESENT)) {
        if (!autocreate) return NULL;
        alloc_page_frame((page_t *)pg, user, 1);

        if (!(*pg >> 12)) return NULL;
    }

    // Flush the tlb to update the tables in the CPU
    __native_flush_tlb_single(virt);
    flush_ipi_all();

    return (page_t *)pg;
}

page_t *find_page(uint64_t virt, bool autocreate, pml4_t *pml4) {
    return find_page_using_alloc(virt, autocreate, alloc_paging_node, is_userspace(virt), pml4);
}

// The alloc func should return a kernel-mapped pointer
void map_virtual_memory_using_alloc(uint64_t phys_start, uint64_t virt_start, size_t len, uint64_t flags,
                                    uint64_t *alloc_func(), pml4_t *pml4) {
    uint32_t i_pml4, i_pdpt, i_pd, i_pt;

    uint64_t *pml4_table, *pdpt_table, *pd_table, *pt_table;

    pml4_table = (uint64_t *)pml4;

    int user = (flags & PAGE_FLAG_USER);

    // uint64_t virt_start = phys_to_virt(phys_start);

    // Map as many pages as needed to fill the range
    for (size_t i = 0; i < len; i += PAGE_LEN) {
        // Addresses for current page
        uint64_t phys_current = phys_start + i;
        uint64_t virt_current = virt_start + i;

        addr_split(virt_current, &i_pml4, &i_pdpt, &i_pd, &i_pt);

        // Search for a page directory pointer table; create if not found
        if (!(pml4_table[i_pml4] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page directory pointer table and add it to the pml4 table
            pdpt_table         = alloc_func();
            pml4_table[i_pml4] = (virt_to_phys((uint64_t)pdpt_table) | flags);
        } else {
            // Retrieve the page directory pointer table, making sure to discard unnecessary bits.
            if (user) pml4_table[i_pml4] |= PAGE_FLAG_USER;
            pdpt_table = (uint64_t *)phys_to_virt((uint64_t)pml4_table[i_pml4] & PHYSADDR_MASK);
        }

        // Search for a page directory table; create if not found
        if (!(pdpt_table[i_pdpt] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page directory table and add it to the page directory pointer table
            pd_table           = alloc_func();
            pdpt_table[i_pdpt] = (virt_to_phys((uint64_t)pd_table) | flags);
        } else {
            // Retrieve the page directory table, making sure to discard unnecessary bits
            if (user) pdpt_table[i_pdpt] |= PAGE_FLAG_USER;
            pd_table = (uint64_t *)phys_to_virt((uint64_t)pdpt_table[i_pdpt] & PHYSADDR_MASK);
        }

        // Search for a page table; create if not found
        if (!(pd_table[i_pd] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page table and add it to the page directory table
            pt_table       = alloc_func();
            pd_table[i_pd] = (virt_to_phys((uint64_t)pt_table) | flags);
        } else {
            // Retrieve the page table, making sure to discard unnecessary bits
            if (user) pd_table[i_pd] |= PAGE_FLAG_USER;
            pt_table = (uint64_t *)phys_to_virt((uint64_t)pd_table[i_pd] & PHYSADDR_MASK);
        }

        // Finally, set the entry in the page table
        // printf("mapping 2 %p\n", phys_current);
        pt_table[i_pt] = ((phys_current & PHYSADDR_MASK) | flags);

        // Flush the tlb to update the tables in the CPU
        __native_flush_tlb_single(virt_current);
    }

    flush_ipi_all();
}

void map_virtual_memory(uint64_t phys_start, size_t len, uint64_t flags, pml4_t *pml4) {
    map_virtual_memory_using_alloc(phys_start, phys_to_virt(phys_start), len, flags, alloc_paging_node, pml4);
}

uint64_t get_physaddr(uint64_t virt, pml4_t *pml4) {
    uint32_t pml4_index, pdpt_index, pd_index, pt_index;

    addr_split(virt, &pml4_index, &pdpt_index, &pd_index, &pt_index);

    dir_entry_t pml4_entry = pml4->entries[pml4_index];

    if (!pml4_entry.present) return 0;

    pdpt_t *pdpt_table     = (pdpt_t *)phys_to_virt(pml4_entry.base_addr << 12);
    dir_entry_t pdpt_entry = pdpt_table->entries[pdpt_index];

    if (!pdpt_entry.present) return 0;

    pd_t *pd_table       = (pd_t *)phys_to_virt(pdpt_entry.base_addr << 12);
    dir_entry_t pd_entry = pd_table->entries[pd_index];

    if (!pd_entry.present) return 0;

    pt_t *pt_table = (pt_t *)phys_to_virt(pd_entry.base_addr << 12);
    page_t page    = pt_table->pages[pt_index];

    if (!page.present) return 0;

    return (page.frame << 12) + (virt & 0xFFF);
}

uint32_t get_physaddr32(uint64_t virt, pml4_t *pml4) {
    uint64_t phys = get_physaddr(virt, pml4);

    if (phys >> 32) {
        printf("WARNING: get_physaddr32() called, but resulting physaddr is >32bit\n");
        return 0U;
    }

    return phys & 0xFFFFFFFF;
}

void init_paging() {
    kernel_pml4_phys = get_cr3_addr();
    kernel_pml4      = (pml4_t *)phys_to_virt(kernel_pml4_phys);

    if (!kernel_pml4_phys) {
        printf("\nNo page table! Halt.\n");
        hcf();
    }

    printf("Page table vaddr: %p, phys addr: %p\n", kernel_pml4, kernel_pml4_phys);

    // init_pmm();

    /* uint64_t addr;

    for (addr = KERNEL_START_ADDR; addr < (0x100000 + KERNEL_START_ADDR); addr += PAGE_LEN) {
        page_t *page = find_page(addr, true, kernel_pml4);

        if (!page) {
            // currently no way to count # of failed pages
            printf("Warning: failed to get an entry for page at virt addr %p.\n", addr);

            continue;
        }
    } */
}

#define PTE_ADDR(x) ((x) & 0x000FFFFFFFFFF000ULL)

void free_page_table(pml4_t *pml4_virt) {
    uint64_t *pml4 = (uint64_t *)pml4_virt;

    for (size_t pml4_i = 0; pml4_i < 512; pml4_i++) {
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

                uint64_t *pt = (uint64_t *)phys_to_virt(PTE_ADDR(pde));
                for (size_t pt_i = 0; pt_i < 512; pt_i++) {
                    uint64_t pte = pt[pt_i];
                    if (!(pte & PAGE_FLAG_PRESENT)) continue;

                    phys_mem_unref_frame((phys_mem_free_frame_t *)phys_addr_to_frame_addr(PTE_ADDR(pte)));
                    pt[pt_i] = 0;
                }
                phys_mem_unref_frame(
                    (phys_mem_free_frame_t *)phys_addr_to_frame_addr(PTE_ADDR(pde))); // free page table
            }
            phys_mem_unref_frame(
                (phys_mem_free_frame_t *)phys_addr_to_frame_addr(PTE_ADDR(pdpte))); // free page directory
        }
        phys_mem_unref_frame((phys_mem_free_frame_t *)phys_addr_to_frame_addr(PTE_ADDR(pml4e))); // free pdpt
    }
    phys_mem_unref_frame(
        (phys_mem_free_frame_t *)phys_addr_to_frame_addr(virt_to_phys((uint64_t)pml4_virt))); // free pml4
}