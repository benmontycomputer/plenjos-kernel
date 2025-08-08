#ifndef _KERNEL_MEMORY_DETECTION_H
#define _KERNEL_MEMORY_DETECTION_H

#include <stdint.h>

#define LIMINE_API_REVISION 3

#include "limine.h"

// Virtual address ranges for user mode (lower-half)
#define USER_START_ADDR 0x1000
#define USER_END_ADDR   0x00007FFFFFFFFFFF

// Virtual address ranges for kernel mode (higher-half)
#define KERNEL_START_ADDR 0xFFFF800000001000
#define KERNEL_HEAP_START_ADDR 0xFFFF900000000000
#define KERNEL_END_ADDR   0xFFFFFFFFFFFFFFFF

extern uint64_t PAGING_MODE;
extern uint64_t HHDM_OFFSET;
extern uint64_t PHYS_MEM_HEAD;

extern uint64_t PHYS_MEM_USEABLE_LENGTH;
extern uint64_t PHYS_MEM_USEABLE_START;
extern uint64_t PHYS_MEM_USEABLE_END;

void detect_memory(struct limine_memmap_response* memmap_response);

#endif