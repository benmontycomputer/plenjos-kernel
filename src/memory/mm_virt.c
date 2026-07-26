#include <stdint.h>

#include "memory/mm.h"
#include "memory/detect.h"

#include "lib/stdio.h"

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
        kout(KERNEL_PANIC, "free_virtual_memory(): couldn't get mapping for virt %p (pml4 %p)\n", virt, pml4);
    }

    phys_mem_unref_frame(phys_addr_to_frame_struct(phys));
}