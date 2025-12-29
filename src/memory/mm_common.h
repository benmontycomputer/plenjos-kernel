#pragma once

#include <stdint.h>
#include <stddef.h>

// Paging memory manager
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

// Physical memory manager
// Kernel only runs in long mode; physical memory extension is required to
// enter long mode, so this will always be enabled
#define __PHYSICAL_MEMORY_EXTENSION__

#define PHYSICAL_MEMORY_RESERVED ((unsigned char)0)
#define PHYSICAL_MEMORY_USEABLE ((unsigned char)1)
#define PHYSICAL_MEMORY_ACPI_RECLAIMABLE ((unsigned char)2)
#define PHYSICAL_MEMORY_ACPI_NVS ((unsigned char)3)
#define PHYSICAL_MEMORY_BADMEMORY ((unsigned char)4)
#define PHYSICAL_MEMORY_UNKNOWN ((unsigned char)5)

// PMM
#define PAGE_LEN 4096
#define FRAME_LEN 4096
#define BITMAP_LEN 64

// Finding index and offset from the bit number
#define INDEX_FROM_BIT_NO(x)(x / BITMAP_LEN)
#define OFFSET_FROM_BIT_NO(x)(x % BITMAP_LEN)

// Making bit no from index and offset
#define CONVERT_BIT_NO(idx, off) (idx * BITMAP_LEN + off)

// Page flags
#define PAGE_FLAG_PRESENT 0x1
#define PAGE_FLAG_WRITE   0x2
#define PAGE_FLAG_USER    0x4
#define PAGE_FLAG_NX      (1ULL << 63)

#define PHYSADDR_MASK 0x000FFFFFFFFFF000ULL

// Virtual memory allocation types
#define ALLOCATE_VM_EX  0x1
#define ALLOCATE_VM_RO  0x2
#define ALLOCATE_VM_USER 0x4

// Paging memory manager
typedef struct dir_entry { // 64 bit; pml4, pdpr and pd entry
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
} __attribute__((packed)) dir_entry_t;

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

typedef struct pt {
    page_t pages[512];
} __attribute__((aligned(PAGE_LEN))) pt_t;

typedef struct paging_node {
    dir_entry_t entries[512];
} __attribute__((aligned(PAGE_LEN))) paging_node_t;

typedef paging_node_t pd_t; typedef paging_node_t pdpt_t; typedef paging_node_t pml4_t;

// Physical memory manager
typedef struct kernel_memory_map {
    size_t mem_entry_count;
    struct limine_memmap_entry **mem_entries;
} kernel_memory_map_t;