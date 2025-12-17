#include <stdint.h>
#include <stddef.h>

#include "kernel.h"
#include "plenjos/errno.h"

#include "memory/mm.h"
#include "memory/kmalloc.h"

#include "vfs/vfs.h"
#include "vfs/kernelfs.h"
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
            return -EFAULT;
        }
        if (!page->user) {
            printf("check_buf: permission denied (not user accessible) at addr %p\n", addr);
            return -EFAULT;
        }
        if (!page->rw) {
            printf("check_buf: permission denied (not writeable) at addr %p\n", addr);
            return -EFAULT;
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

    return 0;
}

ssize_t syscall_routine_read(size_t fd, void *buf, size_t count, pml4_t *current_pml4) {
    printf("syscall_routine_read called with fd %p, buf %p, count %p\n", fd, buf, count);

    proc_t *proc = _get_proc_kernel();

    if (!proc) {
        printf("syscall_routine_read: failed to get current process!\n");

        // TODO: handle this more gracefully (we might actually want to panic; this indicates a serious kernel issue)
        return -1;
    }

    vfs_handle_t *handle = proc_get_fd(proc, fd);

    if (!handle) {
        printf("syscall_routine_read: invalid fd %p for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return -EINVAL;
    }

    void *buf_tmp = kmalloc_heap(count);

    if (!buf_tmp) {
        printf("syscall_routine_read: failed to allocate temporary buffer for read of %p bytes for process %s (pid %p)\n",
               count, proc->name, proc->pid);
        return -ENOMEM;
    }

    // TODO: (SECURITY RISK) check that the final output buffer is valid before reading from the fs at all.
    ssize_t res = vfs_read(handle, buf_tmp, count);

    if (res > 0) {
        int check = copy_to_buf(buf, buf_tmp, res, current_pml4);
        if (check < 0) {
            printf("syscall_routine_read: failed to copy data to user buffer %p for process %s (pid %p), errno %d\n", buf,
                   proc->name, proc->pid, check);
            kfree_heap(buf_tmp);
            return check;
        }
    }

    kfree_heap(buf_tmp);

    return res;
}

ssize_t syscall_routine_write(size_t fd, const void *buf, size_t count, pml4_t *current_pml4) {
    proc_t *proc = _get_proc_kernel();

    if (!proc) {
        printf("syscall_routine_write: failed to get current process!\n");

        // TODO: handle this more gracefully (we might actually want to panic; this indicates a serious kernel issue)
        return -1;
    }

    vfs_handle_t *handle = proc_get_fd(proc, fd);

    if (!handle) {
        printf("syscall_routine_write: invalid fd %p for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return -EINVAL;
    }

    return vfs_write(handle, buf, count);
}

ssize_t syscall_routine_open(const char *path, uint64_t flags, uint64_t mode) {
    proc_t *proc = _get_proc_kernel();
    if (!proc) {
        printf("syscall_routine_open: failed to get current process!\n");

        // TODO: handle this more gracefully (we might actually want to panic; this indicates a serious kernel issue)
        return -1;
    }

    printf("Open syscall called with path %s and mode %o.\n", path, mode);

    vfs_handle_t *handle = NULL;
    ssize_t res = kernelfs_open(path, flags, mode, proc, &handle);
    if (!handle) {
        printf("syscall_routine_open: failed to open %s for process %s (pid %p)\n", path, proc->name, proc->pid);
        return -EINVAL;
    }

    ssize_t fd = proc_alloc_fd(proc, handle);
    if (fd == -1) {
        printf("syscall_routine_open: no available fd for process %s (pid %p)\n", proc->name, proc->pid);
        vfs_close(handle);
        return -EMFILE;
    }

    return fd;
}

int syscall_routine_close(size_t fd) {
    proc_t *proc = _get_proc_kernel();

    if (!proc) {
        printf("syscall_routine_close: failed to get current process!\n");

        // TODO: handle this more gracefully (we might actually want to panic; this indicates a serious kernel issue)
        return -1;
    }

    vfs_handle_t *handle = proc_get_fd(proc, fd);
    if (!handle) {
        printf("syscall_routine_close: invalid fd %p for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return -EINVAL;
    }

    proc_free_fd(proc, fd);

    vfs_close(handle);
    return 0;
}