#ifndef _KERNEL_MEMORY_DETECTION_H
#define _KERNEL_MEMORY_DETECTION_H

#include "memory/mm_common.h"

#include <limine.h>
#include <stdint.h>

// Virtual address ranges for user mode (lower-half)
#define USER_START_ADDR 0x1000
#define USER_END_ADDR   0x00007FFFFFFFFFFF

// Virtual address ranges for kernel mode (higher-half)
#define KERNEL_START_ADDR 0xFFFF800000001000
#define KERNEL_END_ADDR   0xFFFFFFFFFFFFFFFF

extern uint64_t TSS_STACK_ADDR;
extern uint64_t KERNEL_HEAP_START_ADDR;

extern uint64_t PAGING_MODE;
extern uint64_t HHDM_OFFSET;
extern uint64_t PHYS_MEM_HEAD;

extern uint64_t PHYS_MEM_USEABLE_LENGTH;
extern uint64_t PHYS_MEM_USEABLE_START;
extern uint64_t PHYS_MEM_USEABLE_END;

#define FRAME_FLAG_USABLE 0x800000

// prev_free and next_free are indices in the frame table
struct phys_mem_free_frame {
    frame_t prev_free : PHYS_FRAME_BITS;
    frame_t next_free : PHYS_FRAME_BITS;

    // most significant bit is useable; all others are refcnt
    uint32_t flags : 24;
} __attribute__((packed));
typedef struct phys_mem_free_frame phys_mem_free_frame_t;

extern phys_mem_free_frame_t *phys_mem_frame_map;
extern phys_mem_free_frame_t *phys_mem_frame_map_next_free;

static inline frame_t encode_struct_frame_ptr(phys_mem_free_frame_t *frame) {
    return ((ptr_t)frame - (ptr_t)phys_mem_frame_map) / sizeof(phys_mem_free_frame_t);
}

static inline phys_mem_free_frame_t *decode_struct_frame_ptr(frame_t frame) {
    return (phys_mem_free_frame_t *)((frame * sizeof(phys_mem_free_frame_t)) + (ptr_t)phys_mem_frame_map);
}

// Converts the virtual address of a phys_mem_free_frame_t to the physical address of its corresponding frame
static inline physptr_t frame_struct_to_phys_addr(phys_mem_free_frame_t *frame) {
    return encode_struct_frame_ptr(frame) << PAGE_ALIGN_BITS;
}

// Converts a physical address to the virtual address of its corresponding phys_mem_free_frame_t
static inline phys_mem_free_frame_t *phys_addr_to_frame_struct(physptr_t phys) {
    return decode_struct_frame_ptr(phys >> PAGE_ALIGN_BITS);
}

/* struct phys_mem_frame {
    uint32_t ref_cnt : 31;
    uint32_t useable : 1;
} __attribute__((packed));
typedef struct phys_mem_frame phys_mem_frame_t;

extern phys_mem_frame_t *phys_mem_frame_map; */

// This is the total amount of memory used for phys_mem_frame_map, NOT the number of entries.
extern uint64_t phys_mem_frame_map_size;

// extern uint64_t next_free_phys_frame;

void parse_memmap_limine(struct limine_memmap_response *memmap_response);
void detect_memory(struct limine_memmap_response *memmap_response);

#endif