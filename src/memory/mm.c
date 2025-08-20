#define LIMINE_API_REVISION 3

// partially adapted from KeblaOS

#include <limine.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory/detect.h"
#include "memory/mm.h"

#include "lib/stdio.h"

#include "kernel.h"

kernel_memory_map_t memmap;

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_memmap_request memmap_request
    = { .id = LIMINE_MEMMAP_REQUEST, .revision = 3 };

void init_memory_manager() {
    if (!memmap_request.response) {
        printf("\nNo memmap detected. Halt!\n");
        hcf();
    }

    detect_memory(memmap_request.response);

    init_paging();

    parse_memmap_limine(memmap_request.response);

    // This will crash if paging isn't properly set up.
    // *((char *)(KERNEL_START_ADDR)) = 0;
}