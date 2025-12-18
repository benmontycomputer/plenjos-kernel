#include "syscall/fs/syscall_fs.h"

#include "proc/proc.h"
#include "vfs/vfs.h"

#include "lib/stdio.h"

#include "plenjos/errno.h"

ssize_t syscall_routine_open(const char *restrict path, syscall_open_flags_t flags, mode_t mode, proc_t *proc) {
    printf("Open syscall called with path %s and flags %o.\n", path, flags);

    vfs_handle_t *handle = NULL;
    ssize_t res = vfs_open(path, flags, mode, proc->uid, &handle);
    if (res < 0) {
        printf("syscall_routine_open: vfs_open failed for path %s for process %s (pid %p), errno %d\n", path, proc->name, proc->pid, res);
        return res;
    }

    ssize_t fd = proc_alloc_fd(proc, handle);
    if (fd == -1) {
        printf("syscall_routine_open: no available fd for process %s (pid %p)\n", proc->name, proc->pid);
        vfs_close(handle);
        return -EMFILE;
    }

    return fd;
}

int syscall_routine_close(size_t fd, proc_t *proc) {
    vfs_handle_t *handle = proc_get_fd(proc, fd);
    if (!handle) {
        printf("syscall_routine_close: invalid fd %p for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return -EINVAL;
    }

    proc_free_fd(proc, fd);

    vfs_close(handle);
    return 0;
}