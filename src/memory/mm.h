#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef _KERNEL_MEMORYMANAGER_H
#define _KERNEL_MEMORYMANAGER_H

#include "memory/detect.h"
#include "memory/mm_common.h"

void init_memory_manager(void);

// Page memory manager
extern pml4_t *kernel_pml4;
extern uint64_t kernel_pml4_phys;

extern mm_t *kernel_mm;

uint64_t get_cr3_addr(void);
void set_cr3_addr(uint64_t cr3);
void init_paging(void);
void populate_kernel_half_pml4(void);

inline int is_userspace(uint64_t virt) {
    return (virt >= KERNEL_START_ADDR) ? 0 : 1;
}

physptr_t get_physaddr(uint64_t virt, mm_t *mm);
uint32_t get_physaddr32(uint64_t virt, mm_t *mm);

void free_user_page_table(pml4_t *pml4_virt);

// Physical memory manager
extern struct kernel_memory_map memmap;

uint32_t phys_mem_ref_frame(phys_mem_free_frame_t *frame);
uint32_t phys_mem_unref_frame(phys_mem_free_frame_t *frame);

uint64_t find_next_free_frame(void);
void alloc_page_frame(page_t *page, int user, int writeable);

// Virtual memory manager

// NOTE: these don't actually search the page tables; they're just for addresses mapped w/ the fixed offset
inline uint64_t phys_to_virt(uint64_t phys) {
    return phys + HHDM_OFFSET;
}

inline uint64_t virt_to_phys(uint64_t virt) {
    return virt - HHDM_OFFSET;
}

uint64_t *alloc_paging_node(void);

vm_area_t *vmlookup(void *virt, mm_t *mm);
int vmmap(physptr_t phys_start, void *virt_start, size_t len, vma_flags_t requested_flags, vm_area_t **out, mm_t *mm);
int vmalloc(void *virt_hint, size_t size, vma_flags_t requested_flags, vm_area_t **out, mm_t *mm);
void vmfree(vm_area_t *vma, mm_t *mm);

void flush_tlb_range(ptr_t start, ptr_t end, mm_t *mm);

// Returns the physical address of the allocated memory
// uint64_t alloc_virtual_memory(uint64_t virt, uint8_t flags, mm_t *mm);
// void free_virtual_memory(uint64_t virt, mm_t *mm);

mm_t *mm_new(void);

// These return the old val
int mm_ref(mm_t *mm);
int mm_unref(mm_t *mm);

#endif