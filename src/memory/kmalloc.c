#include <stdint.h>

#include "devices/kconsole.h"

#include "memory/kmalloc.h"
#include "memory/memorymanager.h"
#include "memory/detection.h"

static inline bool is_page_misaligned(uint64_t ptr) {
    return (bool)(ptr & 0xFFF);
}

// page aligned (4KiB)
void *kmalloc_a(uint64_t size, bool align) {
    if (align && (PHYS_MEM_HEAD & is_page_misaligned(PHYS_MEM_HEAD))) {
        // Align the physical memory head to page size

        PHYS_MEM_HEAD &= 0xFFFFFFFFFFFFF000;
        PHYS_MEM_HEAD += PAGE_LEN;
    }

    if (PHYS_MEM_HEAD + size >= PHYS_MEM_USEABLE_END) {
        kputs("kmalloc_a: out of physical memory.", 0, 24);
        return NULL;
    }

    uint64_t ret = PHYS_MEM_HEAD;
    PHYS_MEM_HEAD += size;

    return (void *)phys_to_virt(ret);
}