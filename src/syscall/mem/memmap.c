#include "memmap.h"

#include "kernel.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/mm.h"
#include "plenjos/errno.h"
#include "syscall/syscall_helpers.h"

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
            return -EEXIST;
            // TODO: handle this better
            // However, this must throw an error to prevent syscall_routine_memmap_from_buffer from using its write
            // override on an existing mapping
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

    printf("Mapping from buffer to addr %p length %p\n", addr, length);

    uint64_t start = (uint64_t)addr - ((uint64_t)addr % PAGE_LEN);
    uint64_t end   = (uint64_t)addr + length;
    if (end % PAGE_LEN != 0) {
        end += PAGE_LEN - (end % PAGE_LEN);
    }

    size_t to_copy = ((uint64_t)addr + buffer_length > end) ? (end - (uint64_t)addr) : buffer_length;

    // We mapped these pages just now (otherwise the memmap routine would have errored), so we can override the write
    // check
    copy_to_user_buf(addr, buffer, to_copy, true, current_pml4);

    return 0;
}

int syscall_routine_memprotect(void *addr, size_t length, syscall_memmap_flags_t flags, pml4_t *current_pml4) {
    uint64_t voffs = (uint64_t)addr - ((uint64_t)addr % PAGE_LEN);
    if ((uint64_t)addr % PAGE_LEN != 0) {
        length += (uint64_t)addr % PAGE_LEN;
    }
    if (length % PAGE_LEN != 0) {
        length += PAGE_LEN - (length % PAGE_LEN);
    }

    for (voffs; voffs < (uint64_t)addr + length; voffs += PAGE_LEN) {
        page_t *page = find_page(voffs, false, current_pml4);
        if (!page || !(page->present)) {
            printf("Vaddr %p not mapped\n", voffs);
            return -EFAULT;
        }

        uint64_t frame = page->frame << 12;
        if (!frame) {
            printf("Vaddr %p is mapped to frame 0x0\n", voffs);
            return -EFAULT;
        }

        uint64_t curr_flags = PAGE_FLAG_PRESENT;
        if (page->rw) {
            curr_flags |= PAGE_FLAG_WRITE;
        }
        if (page->user) {
            curr_flags |= PAGE_FLAG_USER;
        } else {
            // TODO: this might want to fail silently instead of printing so that user processes can't detect kernel
            // pages
            printf("syscall_routine_memprotect: attempting to change permissions on a kernel page at vaddr %p (not "
                   "allowed!)\n",
                   voffs);
            return -EFAULT;
        }
        if (page->nx) {
            curr_flags |= PAGE_FLAG_NX;
        }

        // Update flags according to requested protections
        if (flags & SYSCALL_MEMMAP_FLAG_WR) {
            if (!(curr_flags & PAGE_FLAG_WRITE)) {
                // Can't add write permission
                printf("syscall_routine_memprotect: cannot add write permission to vaddr %p\n", voffs);
                return -EINVAL;
            }
        } else {
            curr_flags &= ~PAGE_FLAG_WRITE;
        }

        if (flags & SYSCALL_MEMMAP_FLAG_EX) {
            if (curr_flags & PAGE_FLAG_NX) {
                // Can't add execute permission
                printf("syscall_routine_memprotect: cannot add execute permission to vaddr %p\n", voffs);
                return -EINVAL;
            }
        } else {
            curr_flags |= PAGE_FLAG_NX;
        }

        map_virtual_memory_using_alloc(frame, voffs, PAGE_LEN, curr_flags, alloc_paging_node, current_pml4);
    }
}