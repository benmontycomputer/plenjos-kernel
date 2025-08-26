#include <stdbool.h>
#include <stdint.h>

#include "kernel.h"

#include "memory/mm.h"

#include "memory/detect.h"
#include "memory/kmalloc.h"

#include "arch/x86_64/apic/apic.h"

#include "lib/stdio.h"

#include "arch/x86_64/cpuid/cpuid.h"

// Partially modified from KeblaOS

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
    *pd = ((virt) >> 21) & 0x1FF;   // 21-29
    *pt = ((virt) >> 12) & 0x1FF;   // 12-20
}

// Function to flush the entire TLB (by writing to cr3)
void flush_tlb_all() {
    uint64_t cr3;
    // Get the current value of CR3 (the base of the PML4 table)
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    // Write the value of CR3 back to itself, which will flush the TLB
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

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
        pdpt_table = alloc_func();
        pml4_table[i_pml4] = (virt_to_phys((uint64_t)pdpt_table) | flags);
    } else {
        // Retrieve the page directory pointer table, making sure to discard unnecessary bits.
        // the tilde inverts the 0xFFF (so we can write ~0xFFF instead of 0xFFFFFFFFFFFFF000)
        if (user) pml4_table[i_pml4] |= PAGE_FLAG_USER;
        pdpt_table = (uint64_t *)phys_to_virt((uint64_t)pml4_table[i_pml4] & ~(uint64_t)0xFFF);
    }

    // Search for a page directory table; create if not found
    if (!(pdpt_table[i_pdpt] & PAGE_FLAG_PRESENT)) {
        if (!autocreate) return NULL;
        // Allocate a new page directory table and add it to the page directory pointer table
        pd_table = alloc_func();
        pdpt_table[i_pdpt] = (virt_to_phys((uint64_t)pd_table) | flags);
    } else {
        // Retrieve the page directory table, making sure to discard unnecessary bits
        if (user) pdpt_table[i_pdpt] |= PAGE_FLAG_USER;
        pd_table = (uint64_t *)phys_to_virt((uint64_t)pdpt_table[i_pdpt] & ~(uint64_t)0xFFF);
    }

    // Search for a page table; create if not found
    if (!(pd_table[i_pd] & PAGE_FLAG_PRESENT)) {
        if (!autocreate) return NULL;
        // Allocate a new page table and add it to the page directory table
        pt_table = alloc_func();
        pd_table[i_pd] = (virt_to_phys((uint64_t)pt_table) | flags);
    } else {
        // Retrieve the page table, making sure to discard unnecessary bits
        if (user) pd_table[i_pd] |= PAGE_FLAG_USER;
        pt_table = (uint64_t *)phys_to_virt((uint64_t)pd_table[i_pd] & ~(uint64_t)0xFFF);
    }

    pg = &pt_table[i_pt];

    if (!(*pg & PAGE_FLAG_PRESENT)) {
        if (!autocreate) return NULL;
        alloc_page_frame((page_t *)pg, user, 1);

        if (!(*pg >> 12)) return NULL;
    }

    // Flush the tlb to update the tables in the CPU
    __native_flush_tlb_single(virt);

    return (page_t *)pg;
}

page_t *find_page(uint64_t virt, bool autocreate, pml4_t *pml4) {
    return find_page_using_alloc(virt, autocreate, alloc_paging_node, is_userspace(virt), pml4);
}

void map_virtual_memory_using_alloc(uint64_t phys_start, uint64_t virt_start, size_t len, uint64_t flags, uint64_t *alloc_func(), pml4_t *pml4) {
    uint32_t i_pml4, i_pdpt, i_pd, i_pt;

    uint64_t *pml4_table, *pdpt_table, *pd_table, *pt_table;

    pml4_table = (uint64_t *)pml4;

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
            pdpt_table = alloc_func();
            pml4_table[i_pml4] = (virt_to_phys((uint64_t)pdpt_table) | flags);
        } else {
            // Retrieve the page directory pointer table, making sure to discard unnecessary bits.
            // the tilde inverts the 0xFFF (so we can write ~0xFFF instead of 0xFFFFFFFFFFFFF000)
            pdpt_table = (uint64_t *)phys_to_virt((uint64_t)pml4_table[i_pml4] & ~(uint64_t)0xFFF);
        }

        // Search for a page directory table; create if not found
        if (!(pdpt_table[i_pdpt] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page directory table and add it to the page directory pointer table
            pd_table = alloc_func();
            pdpt_table[i_pdpt] = (virt_to_phys((uint64_t)pd_table) | flags);
        } else {
            // Retrieve the page directory table, making sure to discard unnecessary bits
            pd_table = (uint64_t *)phys_to_virt((uint64_t)pdpt_table[i_pdpt] & ~(uint64_t)0xFFF);
        }

        // Search for a page table; create if not found
        if (!(pd_table[i_pd] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page table and add it to the page directory table
            pt_table = alloc_func();
            pd_table[i_pd] = (virt_to_phys((uint64_t)pt_table) | flags);
        } else {
            // Retrieve the page table, making sure to discard unnecessary bits
            pt_table = (uint64_t *)phys_to_virt((uint64_t)pd_table[i_pd] & ~(uint64_t)0xFFF);
        }

        // Finally, set the entry in the page table
        // printf("mapping 2 %p\n", phys_current);
        pt_table[i_pt] = ((phys_current & ~(uint64_t)0xFFF) | flags);

        // Flush the tlb to update the tables in the CPU
        __native_flush_tlb_single(virt_current);
    }
}

void map_virtual_memory(uint64_t phys_start, size_t len, uint64_t flags, pml4_t *pml4) {
    map_virtual_memory_using_alloc(phys_start, phys_to_virt(phys_start), len, flags, alloc_paging_node, pml4);
}

uint64_t get_physaddr(uint64_t virt, pml4_t *pml4) {
    uint32_t pml4_index, pdpt_index, pd_index, pt_index;

    addr_split(virt, &pml4_index, &pdpt_index, &pd_index, &pt_index);

    dir_entry_t *pml4_entry = (dir_entry_t *)&pml4->entries[pml4_index];

    if (!pml4_entry->present) return 0;

    pdpt_t *pdpt_table = (pdpt_t *)(pml4_entry->base_addr << 12);
    dir_entry_t *pdpt_entry = (dir_entry_t *)phys_to_virt((uint64_t)&pdpt_table->entries[pdpt_index]);

    if (!pdpt_entry->present) return 0;

    pd_t *pd_table = (pd_t *)(pdpt_entry->base_addr << 12);
    dir_entry_t *pd_entry = (dir_entry_t *)phys_to_virt((uint64_t)&pd_table->entries[pd_index]);

    if (!pd_entry->present) return 0;

    pt_t *pt_table = (pt_t *)(pd_entry->base_addr << 12);
    page_t *page = (page_t *)phys_to_virt((uint64_t)&pt_table->pages[pt_index]);

    if (!page->present) return 0;

    return (page->frame << 12) + (virt & 0xFFF);
}

void init_paging() {
    kernel_pml4_phys = get_cr3_addr();
    kernel_pml4 = (pml4_t *)phys_to_virt(kernel_pml4_phys);

    if (!kernel_pml4_phys) {
        printf("\nNo page table! Halt.\n");
        hcf();
    }

    printf("Page table vaddr: %p, phys addr: %p\n", kernel_pml4, kernel_pml4_phys);

    // init_pmm();

    uint64_t addr;

    /* for (addr = KERNEL_START_ADDR; addr < (0x100000 + KERNEL_START_ADDR); addr += PAGE_LEN) {
        page_t *page = find_page(addr, true, kernel_pml4);

        if (!page) {
            // currently no way to count # of failed pages
            printf("Warning: failed to get an entry for page at virt addr %p.\n", addr);

            continue;
        }
    } */
}