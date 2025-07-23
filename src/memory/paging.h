#ifndef _KERNEL_PAGING_H
#define _KERNEL_PAGING_H

#include <stdint.h>
#include "memory/memorymanager.h"

// pml4, pdpr and pd entry
struct dir_entry { // 64 bit
    uint64_t present      : 1;  // always 1
    uint64_t rw           : 1;  // 0 for read-only, 1 for read-write
    uint64_t user         : 1;  // 0 for kernel, 1 for user
    uint64_t pwt          : 1;  
    uint64_t pcd          : 1;
    uint64_t accessed     : 1;
    uint64_t reserved_1   : 3;  // all zeros
    uint64_t available_1  : 3;  // zero
    uint64_t base_addr    : 40; // Table base address
    uint64_t available_2  : 11; // zero
    uint64_t xd           : 1;
} __attribute__((packed));
typedef struct dir_entry dir_entry_t;

typedef struct page { // 64 bit
    uint64_t present   : 1;
    uint64_t rw        : 1;
    uint64_t user      : 1;
    uint64_t pwt       : 1;
    uint64_t pcd       : 1;
    uint64_t accessed  : 1;
    uint64_t dirty     : 1;
    uint64_t pat       : 1;
    uint64_t global    : 1;
    uint64_t ignored   : 3;
    uint64_t frame     : 40;
    uint64_t reserved  : 11;
    uint64_t nx        : 1;
} __attribute__((packed)) page_t;

// page table structure is containing 512 page entries
typedef struct pt { 
    page_t pages[512];
} __attribute__((aligned(PAGE_LEN))) pt_t;

// page directory structure is containg 512 page table entries
typedef struct pd { 
    dir_entry_t entries[512]; // Each entry have Physical addresses of PTs
} __attribute__((aligned(PAGE_LEN))) pd_t;

// pdpt structure is containing 512 page directory entries
typedef struct pdpt { 
    dir_entry_t entries[512]; // Each entry have Physical addresses of PDs
} __attribute__((aligned(PAGE_LEN))) pdpt_t;

// pml4 structure is containing 512 pdpt directory entries
typedef struct pml4 { 
    dir_entry_t entries[512]; // Each entry have Physical addresses of PDPTs
} __attribute__((aligned(PAGE_LEN))) pml4_t;

extern pml4_t *kernel_pml4;

void alloc_page_frame(page_t *page, int user, int writeable);
void map_page(void *physaddr, void *virtualaddr, unsigned int flags);
uint64_t get_cr3_addr();
void init_paging();
void flush_tlb_all();

#endif