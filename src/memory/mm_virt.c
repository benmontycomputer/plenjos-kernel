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

// Returns the physical address of the allocated memory
uint64_t alloc_virtual_memory(uint64_t virt, uint8_t flags, pml4_t *pml4) {
    page_t *page = find_page_using_alloc(virt, true, alloc_paging_node, (flags & ALLOCATE_VM_USER) ? 1 : 0, pml4);
    // printf("Allocating page at vaddr %p paddr %p\n", virt, page->frame << 12);

    if (!page) {
        printf("Failed to create page at virtual address %p.\n", virt);
        return 1;
    }
    
    page->nx = !(flags & ALLOCATE_VM_EX);

    page->rw = !(flags & ALLOCATE_VM_RO);

    return page->frame << 12;
}

void free_virtual_memory(uint64_t virt, pml4_t *pml4) {
    uint64_t phys = get_physaddr(virt, pml4);

    if (!phys) {
        kout(KERNEL_SEVERE_FAULT, "free_virtual_memory(): couldn't get mapping for virt %p (pml4 %p)\n", virt, pml4);
    }

    phys_mem_unref_frame((phys_mem_free_frame_t *)phys_addr_to_frame_addr(phys));
}