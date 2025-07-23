#include <stdint.h>
#include <stddef.h>

#define LIMINE_API_REVISION 3

#include "limine.h"

#ifndef _KERNEL_MEMORYMANAGER_H
#define _KERNEL_MEMORYMANAGER_H

#define PHYSICAL_MEMORY_RESERVED ((unsigned char)0)
#define PHYSICAL_MEMORY_USEABLE ((unsigned char)1)
#define PHYSICAL_MEMORY_ACPI_RECLAIMABLE ((unsigned char)2)
#define PHYSICAL_MEMORY_ACPI_NVS ((unsigned char)3)
#define PHYSICAL_MEMORY_BADMEMORY ((unsigned char)4)
#define PHYSICAL_MEMORY_UNKNOWN ((unsigned char)5)

#define PAGING_FLAG_PRESENT         0x001
#define PAGING_FLAG_WRITE           0x002
#define PAGING_FLAG_USER            0x004
#define PAGING_FLAG_WRITE_THROUGH   0x008
#define PAGING_FLAG_CACHE_DISABLE   0x010
#define PAGING_FLAG_ACCESSED        0x020
#define PAGING_FLAG_LARGER_PAGES    0x040
#define PAGING_FLAG_OS_AVAILABLE    0xE00
#define PAGING_FLAG_NO_EXECUTE      (1 << 63)

#define PAGING_FLAGS_KERNEL_PAGE    (PAGING_FLAG_PRESENT | PAGING_FLAG_WRITE)
#define PAGING_FLAGS_USER_PAGE      (PAGING_FLAG_PRESENT | PAGING_FLAG_WRITE | PAGING_FLAG_USER)

#define MEMMAP_START (void *)0xFFFF800000000000

// PMM
#define PAGE_LEN 4096
#define FRAME_LEN 4096
#define BITMAP_LEN 64

// Finding index and offset from the bit number
#define INDEX_FROM_BIT_NO(x)(x / BITMAP_LEN)
#define OFFSET_FROM_BIT_NO(x)(x % BITMAP_LEN)

// Making bit no from index and offset
#define CONVERT_BIT_NO(idx, off) (idx * BITMAP_LEN + off)

extern struct kernel_memory_map memmap;

typedef struct physical_memory_range {
    unsigned char type;
    void *start;
    void *length;
};

typedef struct kernel_memory_map {
    /* struct physical_memory_range* physical_memory_ranges;
    size_t physical_memory_ranges_len;
    void* physical_memory_ranges_next_addr; */

    size_t mem_entry_count;
    struct limine_memmap_entry **mem_entries;
};

void init_memory_manager();

uint64_t phys_to_virt(uint64_t phys);
uint64_t virt_to_phys(uint64_t virt);

#endif