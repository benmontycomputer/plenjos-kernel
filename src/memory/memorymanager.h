#include <stdint.h>

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
#define PAGE_LEN 4096

typedef struct physical_memory_range {
    unsigned char type;
    void *start;
    void *length;
};

typedef struct kernel_memory_map {
    struct physical_memory_range* physical_memory_ranges;
    size_t physical_memory_ranges_len;
    void* physical_memory_ranges_next_addr;
};

void init_memory_manager(struct limine_memmap_response* memmap_response);

#endif