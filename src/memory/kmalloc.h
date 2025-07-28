#ifndef _KERNEL_KMALLOC_H
#define _KERNEL_KMALLOC_H

#include <stdint.h>
#include <stdbool.h>

void *kmalloc(uint64_t size);

// Aligned to page tables
void *kmalloc_a(uint64_t size, bool align);

#endif