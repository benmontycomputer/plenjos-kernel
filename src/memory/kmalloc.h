#ifndef _KERNEL_KMALLOC_H
#define _KERNEL_KMALLOC_H

#include <stdint.h>
#include <stdbool.h>

// void *kmalloc(uint64_t size);

// Aligned to page tables
void *kmalloc_heap_aligned(uint64_t size);

void *kmalloc_heap(uint64_t size);

void init_kernel_heap();

// Aligned to page tables
// void *kmalloc_a(uint64_t size, bool align);

void kfree_heap(void *ptr);

#endif