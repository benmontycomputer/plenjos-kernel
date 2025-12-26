#include "lib/stdio.h"
#include "plenjos/errno.h"
#include "proc/proc.h"
#include "syscall/fs/syscall_fs.h"
#include "vfs/vfs.h"

int syscall_routine_open(const char *restrict path, syscall_open_flags_t flags, mode_t mode, proc_t *proc) {
    printf("Open syscall called with path %s and flags %o.\n", path, flags);

    int res = 0;

    char *path_abs = kmalloc_heap(PATH_MAX);
    if (!path_abs) {
        printf("OOM Error: syscall_routine_open: OOM error allocating memory for absolute path buffer\n");
        return -ENOMEM;
    }

    res = handle_relative_path(path, proc, path_abs);
    if (res < 0) {
        printf("syscall_routine_open: failed to handle relative path %s for process %s (pid %p), errno %d\n", path,
               proc->name, proc->pid, res);
        kfree_heap(path_abs);
        return res;
    }

    vfs_handle_t *handle = NULL;

    res = vfs_open(path_abs, flags, mode, proc->uid, &handle);
    kfree_heap(path_abs);
    if (res < 0) {
        printf("syscall_routine_open: vfs_open failed for path %s for process %s (pid %p), errno %d\n", path_abs,
               proc->name, proc->pid, res);
        return res;
    }

    int fd = proc_alloc_fd(proc, handle);
    if (fd == -1) {
        printf("syscall_routine_open: no available fd for process %s (pid %p)\n", proc->name, proc->pid);
        vfs_close(handle);
        return -EMFILE;
    }

    return fd;
}

int syscall_routine_close(int fd, proc_t *proc) {
    vfs_handle_t *handle = proc_get_fd(proc, fd);
    if (!handle) {
        printf("syscall_routine_close: invalid fd %p for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return -EINVAL;
    }

    proc_free_fd(proc, fd);

    vfs_close(handle);
    return 0;
}