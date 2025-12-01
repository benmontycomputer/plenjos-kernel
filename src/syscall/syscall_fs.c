#include <stdint.h>
#include <stddef.h>

#include "kernel.h"

#include "vfs/vfs.h"
#include "proc/proc.h"

#include "lib/stdio.h"

#include "syscall/syscall.h"

int copy_to_buf(void *buf, size_t count, pml4_t *current_pml4) {
    size_t first_page_len = PAGE_LEN - ((uint64_t)buf % PAGE_LEN);

    for (size_t offset = 0; offset < count; offset += PAGE_LEN) {
        uint64_t addr = (uint64_t)buf + offset;
        page_t *page = find_page(addr, false, current_pml4);
        if (!page || !(page->present)) {
            printf("check_buf: page not mapped at addr %p\n", addr);
            return -1;
        }
        if (!page->user) {
            printf("check_buf: permission denied (not user accessible) at addr %p\n", addr);
            return -1;
        }

        if (offset == 0) {

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

    return vfs_read(handle, buf, count);
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