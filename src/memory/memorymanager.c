#define LIMINE_API_REVISION 3

// partially adapted from KeblaOS

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <limits.h>

#include "memory/memorymanager.h"
#include "memory/paging.h"
#include "memory/detection.h"

#include "devices/kconsole.h"

#include "kernel.h"

struct kernel_memory_map memmap;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 3
};

uint64_t phys_to_virt(uint64_t phys) {
    return phys + HHDM_OFFSET;
}

uint64_t virt_to_phys(uint64_t virt) {
    return virt - HHDM_OFFSET;
}

void init_memory_manager() {
    if (!memmap_request.response) {
        kputs("No memmap detected. Halt!", 0, 19);
        hcf();
    }

    detect_memory(memmap_request.response);

    init_paging();
}