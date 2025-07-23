#define LIMINE_API_REVISION 3

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <limits.h>

#include "memory/memorymanager.h"
#include "memory/paging.h"
#include "devices/kconsole.h"

#include "kernel.h"

struct kernel_memory_map memmap;

void parse_memmap_limine(struct limine_memmap_response* memmap_response) {
    memmap.physical_memory_ranges = NULL;
    memmap.physical_memory_ranges_len = 0;
    memmap.physical_memory_ranges_next_addr = 0;

    // First, we need to find an empty page to store the memory tables

    struct limine_memmap_entry* entry = NULL;
    for(uint64_t entry_n=0;entry_n<memmap_response->entry_count;entry_n++) {
        entry = memmap_response->entries[entry_n];
        // use the entry
        
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t i=0;
            for(;((i+1)*(uint64_t)PAGE_LEN)<entry->length;i++) {
                // TODO: map pages
                map_page((void *)(entry->base+(uint64_t)(i*PAGE_LEN)), (void *)((uint64_t)MEMMAP_START)+(uint64_t)(i*PAGE_LEN), PAGING_FLAGS_KERNEL_PAGE);
                kputs("Memory table staring...", 0, 20);
            }

            memmap.physical_memory_ranges = (struct physical_memory_range*)MEMMAP_START;
            memmap.physical_memory_ranges_len = 0;
            memmap.physical_memory_ranges_next_addr = (void *)((uint64_t)MEMMAP_START+(uint64_t)(i*PAGE_LEN));

            uint64_t addr = entry->base;

            kputs("Memory table start physical address: 0x", 0, 2);
            for(int j=39;addr>>4;j++) {
                char index[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
                kputchar(index[(addr & 0xF)], j, 2, 0xFFFFFF, 0x000000);

                addr = addr>>4;
            }

            break;
        }

        entry = (struct limine_memmap_entry*) ((uint64_t)entry + sizeof(struct limine_memmap_entry));
    }

    entry = *memmap_response->entries;
    while(entry < memmap_response->entry_count * sizeof(struct limine_memmap_entry)) {
        struct physical_memory_range range;
        range.start = (void *)entry->base;
        range.length = (void *)entry->length;

        if (((uint64_t)memmap.physical_memory_ranges_next_addr - (uint64_t)memmap.physical_memory_ranges) > (memmap.physical_memory_ranges_len * sizeof(struct physical_memory_range))); {
            kputs("Out of space in memtable page.", 0, 14);
            break;
        }

        // use the entry
        switch (entry->type) {
            case LIMINE_MEMMAP_ACPI_NVS:
                range.type = PHYSICAL_MEMORY_ACPI_NVS;
                break;
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
                range.type = PHYSICAL_MEMORY_ACPI_RECLAIMABLE;
                break;
            case LIMINE_MEMMAP_BAD_MEMORY:
                range.type = PHYSICAL_MEMORY_BADMEMORY;
                break;
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                range.type = PHYSICAL_MEMORY_UNKNOWN;
                break;
            case LIMINE_MEMMAP_FRAMEBUFFER:
                range.type = PHYSICAL_MEMORY_RESERVED;
                break;
            case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
                range.type = PHYSICAL_MEMORY_RESERVED;
                break;
            case LIMINE_MEMMAP_RESERVED:
                range.type = PHYSICAL_MEMORY_RESERVED;
                break;
            case LIMINE_MEMMAP_USABLE:
                range.type = PHYSICAL_MEMORY_USEABLE;
                break;
            default:
                range.type = PHYSICAL_MEMORY_UNKNOWN;
                break;
        }

        memmap.physical_memory_ranges[memmap.physical_memory_ranges_len] = range;
        memmap.physical_memory_ranges_len++;

        entry = (struct limine_memmap_entry*) ((uint64_t)entry + sizeof(struct limine_memmap_entry));
    }
}

void init_memory_manager(struct limine_memmap_response* memmap_response) {
    init_paging();

    parse_memmap_limine(memmap_response);
}