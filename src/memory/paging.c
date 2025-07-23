#include <stdint.h>
#include <stdbool.h>

#include "kernel.h"
#include "memory/kmalloc.h"

#include "memory/detection.h"
#include "memory/paging.h"

#include "devices/kconsole.h"

// Partially modified from KeblaOS

pml4_t *kernel_pml4;

extern void reload_segments();

uint64_t *frames; // start of bitset frames
uint64_t nframes; // Total numbers of frames

void init_pmm() {
    nframes = (uint64_t)PHYS_MEM_USEABLE_LENGTH / (uint64_t)FRAME_LEN;         // Total number of frames in the memory

    frames = (uint64_t*)kmalloc_a(sizeof(uint64_t) * (nframes + 1) / BITMAP_LEN, 1); // Allocate memory for the bitmap array
    if(frames == NULL){
        // printf("[Error] PMM: Failed to allocate memory for frames\n");
        kputs("Couldn't alloc memory for frames.", 0, 26);
        return;
    }
    // clear the memory of frames array
    memset(frames, 0, sizeof(uint64_t) * (nframes + 1) / BITMAP_LEN);

    // printf(" [-] Successfully initialized PMM!\n");
}

// set the value of frames array by using bit no
void set_frame(uint64_t bit_no) {
    if (bit_no < nframes) {
        return;
    } // check either bit_no is less than total nframes i.e. 0 to nframes-1

    uint64_t bitmap_idx = INDEX_FROM_BIT_NO(bit_no);
    uint64_t bitmap_off = OFFSET_FROM_BIT_NO(bit_no);

    frames[bitmap_idx] |= (0x1 << bitmap_off);       // Set the bit
}

// Static function to find the first free frame.
// The below function will return a valid bit number or invalid bit no -1
uint64_t next_free_frame_bit()
{
    uint64_t free_bit = (uint64_t)-1;
    bool found = false;

    for (uint64_t bitmap_idx = 0; (bitmap_idx < INDEX_FROM_BIT_NO(nframes)) && !found; bitmap_idx++)
    {
        if (frames[bitmap_idx] != 0xFFFFFFFFFFFFFFFF) // if all bits not set, i.e. there has at least one bit is clear
        {    
            for (uint64_t bitmap_off = 0; bitmap_off < BITMAP_LEN; bitmap_off++)
            {
                uint64_t toTest = (uint64_t) 0x1ULL << bitmap_off; // Ensure the shift is handled as a 64-bit value.ULL means Unsigned Long Long 

                if ( !(frames[bitmap_idx] & toTest) ) // if corresponding bit is zero
                {
                    free_bit = CONVERT_BIT_NO(bitmap_idx, bitmap_off); // return corresponding bit number i.e frame index
                    found = true;
                    break;
                }
                continue; // If the current bit is set, continue to the next bit in the bitmap.
            }
        }
        continue;   // If all bits in the current bitmap are set, continue to the next bitmap index.
   }
   return free_bit; // Return an invalid frame index to indicate failure.
}

void alloc_page_frame(page_t *page, int user, int writeable) {
    // idx is now the index of the first free frame.
    uint64_t bit = next_free_frame_bit(); 

    if (bit == (uint64_t) -1) {
        kputs("No free frames for paging. Halt!", 0, 19);
        hcf();
    }

    set_frame(bit); // Mark the frame as used by passing the frame index

    page->present = 1;                      // Mark it as present.
    page->rw = writeable;                // Should the page be writeable?
    page->user = user;                      // Should the page be user-mode?
    PHYS_MEM_HEAD &= 0xFFFFFFFFFFFFF000;    // Align the physical memory head to 4KB boundary
    page->frame = (uint64_t) (PHYS_MEM_HEAD + (bit * FRAME_LEN)) >> 12;     // Store physical base address

    PHYS_MEM_HEAD += FRAME_LEN;            // Move the physical memory head to the next frame
}

void *get_physaddr(void *virtualaddr) {
    unsigned long pdindex = (unsigned long)virtualaddr >> 22;
    unsigned long ptindex = (unsigned long)virtualaddr >> 12 & 0x03FF;

    unsigned long *pd = (unsigned long *)0xFFFFF000;
    // Here you need to check whether the PD entry is present.

    unsigned long *pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.

    return (void *)((pt[ptindex] & ~0xFFF) + ((unsigned long)virtualaddr & 0xFFF));
}

// return current cr3 address i.e. root pml4 pointer address
uint64_t get_cr3_addr() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3)); // Read the CR3 register

    return cr3;
}

void map_page(void *physaddr, void *virtualaddr, unsigned int flags) {
    // Make sure that both addresses are page-aligned.

    unsigned long pdindex = (unsigned long)virtualaddr >> 22;
    unsigned long ptindex = (unsigned long)virtualaddr >> 12 & 0x03FF;

    unsigned long *pd = (unsigned long *)0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    // When it is not present, you need to create a new empty PT and
    // adjust the PDE accordingly.

    unsigned long *pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.
    // When it is, then there is already a mapping present. What do you do now?

    pt[ptindex] = ((unsigned long)physaddr) | (flags & 0xFFF) | 0x01; // Present

    // Now you need to flush the entry in the TLB
    // or you might not notice the change.
}

void unmap_page(void *virtualaddr) {
    // Make sure that both addresses are page-aligned.

    unsigned long pdindex = (unsigned long)virtualaddr >> 22;
    unsigned long ptindex = (unsigned long)virtualaddr >> 12 & 0x03FF;

    unsigned long *pd = (unsigned long *)0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    // When it is not present, you need to create a new empty PT and
    // adjust the PDE accordingly.

    unsigned long *pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.
    // When it is, then there is already a mapping present. What do you do now?

    pt[ptindex] = (unsigned long)0x00000000;

    // Now you need to flush the entry in the TLB
    // or you might not notice the change.
}

// pml4 = pointer map level 4, pdpt = page directory pointer table, pd = page directory, pt = page table
static inline void virt_split(uint64_t virt, uint32_t *pml4, uint32_t *pdpt, uint32_t *pd, uint32_t *pt) {
    *pml4 = ((virt) >> 39) & 0x1FF; // 39-47
    *pdpt = ((virt) >> 30) & 0x1FF; // 30-38
    *pd = ((virt) >> 21) & 0x1FF;   // 21-29
    *pt = ((virt) >> 12) & 0x1FF;   // 12-20
}

inline int is_userspace(uint64_t virt) {
    return (virt >= KERNEL_START_ADDR) ? 0 : 1;
}

// Function to flush the entire TLB (by writing to cr3)
void flush_tlb_all() {
    uint64_t cr3;
    // Get the current value of CR3 (the base of the PML4 table)
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    // Write the value of CR3 back to itself, which will flush the TLB
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

// Function to allocate a new page table
static pt_t* alloc_pt() {
    pt_t* pt = (pt_t*)kmalloc_a(sizeof(pt_t), 1);
    if(!pt) {
        kputs("Paging error; failed to allocate PT", 0, 23);
        return NULL;            // Allocation failed
    }
    memset(pt, 0, sizeof(pt_t)); // Zero out the page table
    return pt;
}

// Function to allocate a new page directory
static pd_t* alloc_pd() {
    pd_t* pd = (pd_t*)kmalloc_a(sizeof(pd_t), 1);
    if (!pd){
        kputs("Paging error; failed to allocate PD", 0, 23);
        return NULL;            // Allocation failed
    }
    memset(pd, 0, sizeof(pd_t)); // Zero out the page directory
    return pd;
}

// Function to allocate a new page directory pointer table
static pdpt_t* alloc_pdpt() {
    pdpt_t* pdpt = (pdpt_t*)kmalloc_a(sizeof(pdpt_t), 1);

    if (!pdpt) {
        kputs("Paging error; failed to allocate PDPT", 0, 23);
        return NULL;                    // Allocation failed
    }
    memset(pdpt, 0, sizeof(pdpt_t));    // Zero out the PDPT
    return pdpt;
}

page_t *request_page(uint64_t virt, bool autocreate, pml4_t *pml4) {
    if (!pml4) {
        kputs("No page table? Can't request_page().", 0, 18);

        return NULL;
    }

    uint32_t pml4_index, pdpt_index, pd_index, pt_index;

    virt_split(virt, &pml4_index, &pdpt_index, &pd_index, &pt_index);

    int user = is_userspace(virt);

    // Start at the top

    dir_entry_t *pml4_entry = (dir_entry_t *)&pml4->entries[pml4_index];

    if (!pml4_entry->present) {
        if (!autocreate) return NULL;

        pdpt_t *new_pdpt = alloc_pdpt();

        if (!new_pdpt) return NULL;

        pml4_entry->present = 1;
        pml4_entry->base_addr = (uint64_t)new_pdpt >> 12;

        pml4_entry->rw = 1;
    }
    pml4_entry->user = user;

    pdpt_t *pdpt_table = (pdpt_t *)(pml4_entry->base_addr << 12);
    dir_entry_t *pdpt_entry = (dir_entry_t *)phys_to_virt((uint64_t)&pdpt_table->entries[pdpt_index]);

    if (!pdpt_entry->present) {
        if (!autocreate) return NULL;

        pd_t *new_pd = alloc_pd();

        if (!new_pd) return NULL;

        pdpt_entry->present = 1;
        pdpt_entry->base_addr = (uint64_t)new_pd >> 12;

        pdpt_entry->rw = 1;
    }
    pdpt_entry->user = user;

    pd_t *pd_table = (pd_t *)(pdpt_entry->base_addr << 12);
    dir_entry_t *pd_entry = (dir_entry_t *)phys_to_virt((uint64_t)&pd_table->entries[pd_index]);

    if (!pd_entry->present) {
        if (!autocreate) return NULL;

        pt_t *new_pt = alloc_pt();

        if (!new_pt) return NULL;

        pd_entry->present = 1;
        pd_entry->base_addr = (uint64_t)new_pt >> 12;

        pd_entry->rw = 1;
    }
    pd_entry->user = user;

    pt_t *pt_table = (pt_t *)(pd_entry->base_addr << 12);
    page_t *page = (page_t *)phys_to_virt((uint64_t)&pt_table->pages[pt_index]);

    if (!page->present) {
        alloc_page_frame(page, user, 1);
        if (!page->frame) return NULL;
        page->user = user;
    }

    flush_tlb_all();

    return page;
}

void init_paging() {
    kernel_pml4 = (pml4_t *)phys_to_virt(get_cr3_addr());

    if (!kernel_pml4) {
        kputs("No page table! Halt.", 0, 18);
        hcf();
    }

    init_pmm();

    // Map kernel's pages

    uint64_t addr;

    for (addr = KERNEL_START_ADDR; addr < (0x100000 + KERNEL_START_ADDR); addr += PAGE_LEN) {
        page_t *page = request_page(addr, true, kernel_pml4);

        if (!page) {
            // currently no way to count # of failed pages
            kputs("Warning: failed to get an entry for 1 or more pages.", 0, 18);

            continue;
        }

        if (page->present && page->frame) {
            // kputs("[Debug] Page at address %x already present with frame %x\n", 0, 23);
            continue; // Skip if the page is already present
        }

        // alloc_page_frame(page, 0, 1);
    }
}