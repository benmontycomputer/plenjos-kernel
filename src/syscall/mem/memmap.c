#include "memmap.h"

#include "kernel.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/mm.h"
#include "plenjos/errno.h"

int syscall_routine_memmap(void *addr, size_t length, syscall_memmap_flags_t flags, pml4_t *current_pml4) {
    uint64_t voffs = (uint64_t)addr - ((uint64_t)addr % PAGE_LEN);
    if ((uint64_t)addr % PAGE_LEN != 0) {
        length += (uint64_t)addr % PAGE_LEN;
    }
    if (length % PAGE_LEN != 0) {
        length += PAGE_LEN - (length % PAGE_LEN);
    }

    for (voffs; voffs < (uint64_t)addr + length; voffs += PAGE_LEN) {
        if (get_physaddr(voffs, current_pml4)) {
            printf("WARNING: the pml4 table at vaddr %p already has %p mapped.\n", current_pml4, voffs);
            continue;
            // TODO: handle this better
        }
        uint64_t frame = find_next_free_frame();
        if (!frame) {
            printf("Failed to find free frame for vaddr %p\n", voffs);
            return -ENOMEM;
        }
        memset((void *)phys_to_virt(frame), 0, PAGE_LEN);
        map_virtual_memory_using_alloc(frame, voffs, PAGE_LEN,
                                       PAGE_FLAG_PRESENT | PAGE_FLAG_USER
                                           | (flags & SYSCALL_MEMMAP_FLAG_WR ? PAGE_FLAG_WRITE : 0)
                                           | (flags & SYSCALL_MEMMAP_FLAG_EX ? 0 : PAGE_FLAG_NX),
                                       alloc_paging_node, current_pml4);
    }

    return 0;
}

int syscall_routine_memmap_from_buffer(void *addr, size_t length, syscall_memmap_flags_t flags, void *buffer,
                                       size_t buffer_length, pml4_t *current_pml4) {
    int res = syscall_routine_memmap(addr, length, flags, current_pml4);
    if (res != 0) {
        return res;
    }

    uint64_t start = (uint64_t)addr - ((uint64_t)addr % PAGE_LEN);
    uint64_t end   = (uint64_t)addr + length;
    if (end % PAGE_LEN != 0) {
        end += PAGE_LEN - (end % PAGE_LEN);
    }

    size_t to_copy = ((uint64_t)addr + buffer_length > end) ? (end - (uint64_t)addr) : buffer_length;

    memcpy(addr, buffer, to_copy);

    return 0;
}