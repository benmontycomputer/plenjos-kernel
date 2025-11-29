#include <stdint.h>
#include <stddef.h>

#include "kernel.h"

#include "vfs/vfs.h"
#include "proc/proc.h"

#include "lib/stdio.h"

#include "syscall/syscall.h"

ssize_t syscall_routine_read(size_t fd, void *buf, size_t count) {
    proc_t *proc = _get_proc_kernel();

    vfs_handle_t *handle = proc_get_fd(proc, fd);

    if (!handle) {
        printf("syscall_routine_read: invalid fd %lu for process %s (pid %lu)\n", fd, proc->name, proc->pid);
        return -1;
    }

    return vfs_read(handle, buf, count);
}

ssize_t syscall_routine_write(size_t fd, const void *buf, size_t count) {
    proc_t *proc = _get_proc_kernel();

    vfs_handle_t *handle = proc_get_fd(proc, fd);

    if (!handle) {
        printf("syscall_routine_write: invalid fd %lu for process %s (pid %lu)\n", fd, proc->name, proc->pid);
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
        printf("syscall_routine_open: failed to open %s for process %s (pid %lu)\n", path, proc->name, proc->pid);
        return (size_t)-1;
    }

    size_t fd = proc_alloc_fd(proc, handle);
    if (fd == (size_t)-1) {
        printf("syscall_routine_open: no available fd for process %s (pid %lu)\n", proc->name, proc->pid);
        vfs_close(handle);
        return (size_t)-1;
    }

    return fd;
}

int syscall_routine_close(size_t fd) {
    proc_t *proc = _get_proc_kernel();

    vfs_handle_t *handle = proc_get_fd(proc, fd);
    if (!handle) {
        printf("syscall_routine_close: invalid fd %lu for process %s (pid %lu)\n", fd, proc->name, proc->pid);
        return -1;
    }

    proc_free_fd(proc, fd);

    vfs_close(handle);
    return 0;
}