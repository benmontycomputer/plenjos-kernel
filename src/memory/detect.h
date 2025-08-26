#ifndef _KERNEL_MEMORY_DETECTION_H
#define _KERNEL_MEMORY_DETECTION_H

#include <stdint.h>

#define LIMINE_API_REVISION 3

#include "limine.h"
#include "memory/mm_common.h"

// Virtual address ranges for user mode (lower-half)
#define USER_START_ADDR 0x1000
#define USER_END_ADDR 0x00007FFFFFFFFFFF

// Virtual address ranges for kernel mode (higher-half)
#define KERNEL_START_ADDR 0xFFFF800000001000
#define KERNEL_END_ADDR 0xFFFFFFFFFFFFFFFF

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
    uint64_t prev_free : 36;
    uint64_t next_free : 36;

    // most significant bit is useable; all others are refcnt
    uint32_t flags : 24;
} __attribute__((packed));
typedef struct phys_mem_free_frame phys_mem_free_frame_t;

extern phys_mem_free_frame_t *phys_mem_frame_map;
extern phys_mem_free_frame_t *phys_mem_frame_map_next_free;

static inline uint64_t encode_struct_frame_ptr(uint64_t ptr) {
    return (ptr - (uint64_t)phys_mem_frame_map) / sizeof(phys_mem_free_frame_t);
}

static inline uint64_t decode_struct_frame_ptr(uint64_t ptr) {
    return (ptr * sizeof(phys_mem_free_frame_t)) + (uint64_t)phys_mem_frame_map;
}

// Converts the address of a phys_mem_free_frame_t to the physical address of its corresponding frame
static inline uint64_t frame_addr_to_phys_addr(uint64_t frame_addr) {
    return encode_struct_frame_ptr(frame_addr) << 12;
}

// Converts the address of a phys_mem_free_frame_t to the physical address of its corresponding frame
static inline uint64_t phys_addr_to_frame_addr(uint64_t phys) {
    return decode_struct_frame_ptr(phys >> 12);
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