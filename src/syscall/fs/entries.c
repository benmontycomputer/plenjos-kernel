#include "syscall/fs/syscall_fs.h"

ssize_t syscall_routine_getdents(int fd, void *buf, size_t count, proc_t *proc, pml4_t *current_pml4) {
    printf("Getdents syscall called on fd %p for process %s (pid %p) for %p bytes.\n", fd, proc->name, proc->pid,
           count);

    vfs_handle_t *handle = NULL;
    ssize_t res          = syscall_fs_helper_get_dir_handle(fd, proc, &handle);
    if (res < 0) {
        printf("syscall_routine_getdents: failed to get directory handle\n");
        return res;
    }

    void *buf_tmp = kmalloc_heap(count);

    if (!buf_tmp) {
        printf("syscall_routine_getdents: failed to allocate temporary buffer for read of %p bytes for process %s (pid "
               "%p)\n",
               count, proc->name, proc->pid);
        return -ENOMEM;
    }

    // We don't split entries; we only read the entries that can fully fit in the buffer.
    size_t count_adj = (count / sizeof(struct plenjos_dirent)) * sizeof(struct plenjos_dirent);

    // TODO: (SECURITY RISK) check that the final output buffer is valid before reading from the fs at all.
    res = vfs_read(handle, buf_tmp, count_adj);

    if (res > 0) {
        int check = copy_to_user_buf(buf, buf_tmp, res, false, current_pml4);
        if (check < 0) {
            printf(
                "syscall_routine_getdents: failed to copy data to user buffer %p for process %s (pid %p), errno %d\n",
                buf, proc->name, proc->pid, check);
            kfree_heap(buf_tmp);
            return (ssize_t)check;
        }
    }

    kfree_heap(buf_tmp);
    return res;
}

int syscall_routine_mkdir(const char *restrict path, mode_t mode, proc_t *proc) {
    int res = 0;

    char *path_abs = kmalloc_heap(PATH_MAX);
    if (!path_abs) {
        printf("OOM Error: syscall_routine_mkdir: OOM error allocating memory for absolute path buffer\n");
        return -ENOMEM;
    }

    res = handle_relative_path(path, proc, path_abs);
    if (res < 0) {
        printf("syscall_routine_mkdir: failed to handle relative path %s for process %s (pid %p), errno %d\n", path,
               proc->name, proc->pid, res);
        kfree_heap(path_abs);
        return res;
    }

    kfree_heap(path_abs);
    return vfs_mkdir(path_abs, proc->uid, 0 /* TODO: use egid here */, mode);
}

int syscall_routine_rmdir(const char *restrict path, proc_t *proc) {
    return -ENOSYS;
}