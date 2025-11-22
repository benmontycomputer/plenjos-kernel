#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include "vfs/vfs.h"
#include "proc/proc.h"

#include "lib/stdio.h"

#include "syscall/syscall.h"

ssize_t syscall_routine_read(size_t fd, void *buf, size_t count) {
    proc_t *proc = _get_proc_kernel();

    vfs_node_t *node = proc_get_fd(proc, fd);

    if (!node) {
        printf("syscall_routine_read: invalid fd %lu for process %s (pid %lu)\n", fd, proc->name, proc->pid);
        return -1;
    }

    return vfs_read(node, buf, count);
}

ssize_t syscall_routine_write(size_t fd, const void *buf, size_t count) {
    proc_t *proc = _get_proc_kernel();

    vfs_node_t *node = proc_get_fd(proc, fd);

    if (!node) {
        printf("syscall_routine_write: invalid fd %lu for process %s (pid %lu)\n", fd, proc->name, proc->pid);
        return -1;
    }

    return vfs_write(node, buf, count);
}

size_t syscall_routine_open(const char *path, const char *mode) {
    proc_t *proc = _get_proc_kernel();

    vfs_node_t *node = vfs_open(path, mode);
    if (!node) {
        printf("syscall_routine_open: failed to open %s for process %s (pid %lu)\n", path, proc->name, proc->pid);
        return (size_t)-1;
    }

    size_t fd = proc_alloc_fd(proc, node);
    if (fd == (size_t)-1) {
        printf("syscall_routine_open: no available fd for process %s (pid %lu)\n", proc->name, proc->pid);
        vfs_close(node);
        return (size_t)-1;
    }

    return fd;
}

int syscall_routine_close(size_t fd) {
    proc_t *proc = _get_proc_kernel();

    vfs_node_t *node = proc_get_fd(proc, fd);
    if (!node) {
        printf("syscall_routine_close: invalid fd %lu for process %s (pid %lu)\n", fd, proc->name, proc->pid);
        return -1;
    }

    proc_free_fd(proc, fd);

    vfs_close(node);
    return 0;
}