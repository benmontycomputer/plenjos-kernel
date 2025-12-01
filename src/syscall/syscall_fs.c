#include <stdint.h>
#include <stddef.h>

#include "kernel.h"

#include "memory/mm.h"
#include "memory/kmalloc.h"

#include "vfs/vfs.h"
#include "proc/proc.h"

#include "lib/stdio.h"

#include "syscall/syscall.h"

// TODO: any attempts to read/write from an out-of-bounds buffer should kill the process (segfault?)

int copy_to_buf(void *dest, void *src, size_t count, pml4_t *current_pml4) {
    size_t first_page_len = PAGE_LEN - ((uint64_t)dest % PAGE_LEN);
    size_t last_page_len = ((uint64_t)dest + count) % PAGE_LEN;

    uint64_t offs = 0;

    while (offs < count) {
        uint64_t addr = (uint64_t)dest + offs;

        page_t *page = find_page(addr, false, current_pml4);
        if (!page || !(page->present)) {
            printf("check_buf: page not mapped at addr %p\n", addr);
            return -1;
        }
        if (!page->user) {
            printf("check_buf: permission denied (not user accessible) at addr %p\n", addr);
            return -1;
        }
        if (!page->rw) {
            printf("check_buf: permission denied (not writeable) at addr %p\n", addr);
            return -1;
        }

        if (offs == 0) {
            // First page (or we only need 1 page); might not have to write a full page
            size_t to_copy = (count < first_page_len) ? count : first_page_len;
            memcpy((void *)phys_to_virt((page->frame << 12) + (addr % PAGE_LEN)), src + offs, to_copy);
            offs += to_copy;
        } else if (offs + PAGE_LEN > count) {
            // Last page; might not have to write a full page
            memcpy((void *)phys_to_virt((page->frame << 12) + (addr % PAGE_LEN)), src + offs, last_page_len);
            offs += last_page_len;
        } else {
            // Full page
            memcpy((void *)phys_to_virt(page->frame << 12), src + offs, PAGE_LEN);
            offs += PAGE_LEN;
        }
    }
}

ssize_t syscall_routine_read(size_t fd, void *buf, size_t count, pml4_t *current_pml4) {
    printf("syscall_routine_read called with fd %p, buf %p, count %p\n", fd, buf, count);

    proc_t *proc = _get_proc_kernel();

    vfs_handle_t *handle = proc_get_fd(proc, fd);

    if (!handle) {
        printf("syscall_routine_read: invalid fd %p for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return -1;
    }

    void *buf_tmp = kmalloc_heap(count);

    if (!buf_tmp) {
        printf("syscall_routine_read: failed to allocate temporary buffer for read of %p bytes for process %s (pid %p)\n",
               count, proc->name, proc->pid);
        return -1;
    }

    ssize_t res = vfs_read(handle, buf_tmp, count);

    if (res > 0) {
        int check = copy_to_buf(buf, buf_tmp, res, current_pml4);
        if (check < 0) {
            printf("syscall_routine_read: failed to copy data to user buffer %p for process %s (pid %p)\n", buf,
                   proc->name, proc->pid);
            kfree_heap(buf_tmp);
            return -1;
        }
    }

    kfree_heap(buf_tmp);

    return res;
}

ssize_t syscall_routine_write(size_t fd, const void *buf, size_t count, pml4_t *current_pml4) {
    proc_t *proc = _get_proc_kernel();

    vfs_handle_t *handle = proc_get_fd(proc, fd);

    if (!handle) {
        printf("syscall_routine_write: invalid fd %p for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return -1;
    }

    return vfs_write(handle, buf, count);
}

size_t syscall_routine_open(const char *path, const char *mode) {
    proc_t *proc = _get_proc_kernel();
    if (!proc) {
        printf("syscall_routine_open: failed to get current process!\n");
        return (size_t)-1;
    }

    printf("Open syscall called with path %s and mode %s.\n", path, mode);

    vfs_handle_t *handle = vfs_open(path, mode);
    if (!handle) {
        printf("syscall_routine_open: failed to open %s for process %s (pid %p)\n", path, proc->name, proc->pid);
        return (size_t)-1;
    }

    size_t fd = proc_alloc_fd(proc, handle);
    if (fd == (size_t)-1) {
        printf("syscall_routine_open: no available fd for process %s (pid %p)\n", proc->name, proc->pid);
        vfs_close(handle);
        return (size_t)-1;
    }

    return fd;
}

int syscall_routine_close(size_t fd) {
    proc_t *proc = _get_proc_kernel();

    vfs_handle_t *handle = proc_get_fd(proc, fd);
    if (!handle) {
        printf("syscall_routine_close: invalid fd %p for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return -1;
    }

    proc_free_fd(proc, fd);

    vfs_close(handle);
    return 0;
}