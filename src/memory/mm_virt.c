#include <stdint.h>

#include "memory/mm.h"
#include "memory/detect.h"

#include "lib/stdio.h"

uint64_t phys_to_virt(uint64_t phys) {
    return phys + HHDM_OFFSET;
}

uint64_t virt_to_phys(uint64_t virt) {
    return virt - HHDM_OFFSET;
}

int alloc_virtual_memory(uint64_t virt, uint8_t flags) {
    page_t *page = find_page(virt, true, kernel_pml4);

    if (!page) {
        printf("Failed to create page at virtual address %p.\n", virt);
        return 1;
    }
    
    if (!(flags & ALLOCATE_VM_EX)) page->nx = 1;
    if (!(flags & ALLOCATE_VM_RO)) page->rw = 1;

    return 0;
}