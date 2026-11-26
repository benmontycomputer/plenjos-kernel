#include "memmap.h"

#include "kernel.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/mm.h"
#include "plenjos/errno.h"
#include "syscall/syscall_helpers.h"

void *syscall_routine_memmap(void *addr, size_t length, syscall_memmap_flags_t flags, mm_t *mm) {
    vma_flags_t req_flags = VMA_USER;
    if (flags & SYSCALL_MEMMAP_FLAG_WR) {
        req_flags |= VMA_RW;
    }
    if (flags & SYSCALL_MEMMAP_FLAG_EX) {
        req_flags |= VMA_EXEC;
    }
    if (flags & SYSCALL_MEMMAP_FLAG_FIXED) {
        req_flags |= VMALLOC_FIXED;
    }

    vm_area_t *vma = NULL;
    if (vmalloc(addr, length, req_flags, &vma, mm)) {
        return NULL;
    }
    return (void *)vma->start;
}

void *syscall_routine_memmap_from_buffer(void *addr_req, size_t length, syscall_memmap_flags_t flags, void *buffer,
                                       size_t buffer_length, mm_t *mm) {
    void *addr = syscall_routine_memmap(addr_req, length, flags, mm);
    if (!addr) {
        return NULL;
    }

    uint64_t end = (uint64_t)addr + length;
    if (end % PAGE_LEN != 0) {
        end += PAGE_LEN - (end % PAGE_LEN);
    }

    size_t to_copy = ((uint64_t)addr + buffer_length > end) ? (end - (uint64_t)addr) : buffer_length;

    // We mapped these pages just now (otherwise the memmap routine would have errored), so we can override the write
    // check
    copy_to_user_buf(addr, buffer, to_copy, true, mm);

    return 0;
}

/* int syscall_routine_memprotect(void *addr, size_t length, syscall_memmap_flags_t flags, pml4_t *current_pml4) {
    uint64_t voffs = (uint64_t)addr - ((uint64_t)addr % PAGE_LEN);
    if ((uint64_t)addr % PAGE_LEN != 0) {
        length += (uint64_t)addr % PAGE_LEN;
    }
    if (length % PAGE_LEN != 0) {
        length += PAGE_LEN - (length % PAGE_LEN);
    }

    for (; voffs < (uint64_t)addr + length; voffs += PAGE_LEN) {
        page_t *page = find_page(voffs, false, current_pml4);
        if (!page || !(page->present)) {
            printf("syscall_routine_memprotect: bad address: vaddr %p not mapped\n", voffs);
            return -EFAULT;
        }

        uint64_t frame = page->frame << 12;
        if (!frame) {
            printf("syscall_routine_memprotect: bad address: vaddr %p is mapped to frame 0x0\n", voffs);
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

    return 0;
} */