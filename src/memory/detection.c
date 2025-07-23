#define LIMINE_API_REVISION 3

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <limits.h>

#include "memory/detection.h"
#include "memory/memorymanager.h"

#include "devices/kconsole.h"

__attribute__((used, section(".limine_requests")))
static volatile struct limine_paging_mode_request paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 3
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 3
};

uint64_t PAGING_MODE;
uint64_t HHDM_OFFSET;
uint64_t PHYS_MEM_HEAD;

uint64_t PHYS_MEM_USEABLE_LENGTH;
uint64_t PHYS_MEM_USEABLE_START;
uint64_t PHYS_MEM_USEABLE_END;

static void parse_memmap_limine(struct limine_memmap_response* memmap_response) {
    memmap.mem_entries     = memmap_response->entries;
    memmap.mem_entry_count = memmap_response->entry_count;

    for (size_t i = 0; i < memmap.mem_entry_count; i++){
        uint64_t base = memmap.mem_entries[i]->base;
        uint64_t length = memmap.mem_entries[i]->length;
        uint64_t type = memmap.mem_entries[i]->type;

        if (type == LIMINE_MEMMAP_USABLE){
            if (length > PHYS_MEM_USEABLE_LENGTH){
                // Load details of physical memory
                PHYS_MEM_USEABLE_LENGTH = length;
                PHYS_MEM_USEABLE_START  = base;
                PHYS_MEM_USEABLE_END    = PHYS_MEM_USEABLE_START + PHYS_MEM_USEABLE_LENGTH;
            }
        }
    }

    PHYS_MEM_HEAD = PHYS_MEM_USEABLE_START;
}

void detect_memory(struct limine_memmap_response* memmap_response) {
    if (!hhdm_request.response) {
        // Error!
        kputs("Couldn't get HHDM offset.", 0, 22);
    } else {
        HHDM_OFFSET = hhdm_request.response->offset;
    }

    if (!paging_mode_request.response) {
        // Error!
        kputs("Couldn't get paging mode.", 0, 23);
    } else {
        PAGING_MODE = paging_mode_request.response->mode;
    }

    parse_memmap_limine(memmap_response);
}