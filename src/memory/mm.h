#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LIMINE_API_REVISION 3

#include "limine.h"

#ifndef _KERNEL_MEMORYMANAGER_H
#define _KERNEL_MEMORYMANAGER_H

#include "memory/mm_common.h"

#include "memory/detect.h"

void init_memory_manager();

// Page memory manager
extern pml4_t *kernel_pml4;

uint64_t get_cr3_addr();
void init_paging();
void flush_tlb_all();

page_t *find_page_using_alloc(uint64_t virt, bool autocreate, uint64_t *alloc_func(), pml4_t *pml4);
page_t *find_page(uint64_t virt, bool autocreate, pml4_t *pml4);

uint64_t get_physaddr(uint64_t virt);

// Physical memory manager
extern struct kernel_memory_map memmap;

uint32_t phys_mem_ref_frame(phys_mem_free_frame_t *frame);
uint32_t phys_mem_unref_frame(phys_mem_free_frame_t *frame);

uint64_t find_next_free_frame();
void alloc_page_frame(page_t *page, int user, int writeable);

// Virtual memory manager

// NOTE: these don't actually search the page tables; they're just for addresses mapped w/ the fixed offset
uint64_t phys_to_virt(uint64_t phys);
uint64_t virt_to_phys(uint64_t virt);

uint64_t *alloc_paging_node();

void map_virtual_memory_using_alloc(uint64_t phys_start, uint64_t virt_start, size_t len, uint64_t flags, uint64_t *alloc_func(), pml4_t *pml4);
void map_virtual_memory(uint64_t phys_addr, size_t size, uint64_t flags, pml4_t *pml4);

int alloc_virtual_memory(uint64_t virt, uint8_t flags);

#endif