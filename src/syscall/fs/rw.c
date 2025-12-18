#include "syscall/fs/syscall_fs.h"
#include "syscall/syscall_helpers.h"

#include "proc/proc.h"
#include "vfs/vfs.h"
#include "vfs/fscache.h"

#include "memory/kmalloc.h"

#include "lib/stdio.h"

#include "plenjos/errno.h"

ssize_t syscall_routine_read(size_t fd, void *buf, size_t count, proc_t *proc, pml4_t *current_pml4) {
    printf("syscall_routine_read called with fd %p, buf %p, count %p\n", fd, buf, count);

    vfs_handle_t *handle = NULL;

    ssize_t res = syscall_fs_helper_get_not_dir_handle(fd, proc, &handle);
    if (res < 0) {
        printf("syscall_routine_read: failed to get non-directory handle\n");
        return res;
    }

    void *buf_tmp = kmalloc_heap(count);

    if (!buf_tmp) {
        printf("syscall_routine_read: failed to allocate temporary buffer for read of %p bytes for process %s (pid %p)\n",
               count, proc->name, proc->pid);
        return -ENOMEM;
    }

    // TODO: (SECURITY RISK) check that the final output buffer is valid before reading from the fs at all.
    res = vfs_read(handle, buf_tmp, count);

    if (res > 0) {
        int check = copy_to_user_buf(buf, buf_tmp, res, current_pml4);
        if (check < 0) {
            printf("syscall_routine_read: failed to copy data to user buffer %p for process %s (pid %p), errno %d\n", buf,
                   proc->name, proc->pid, check);
            kfree_heap(buf_tmp);
            return (ssize_t)check;
        }
    }

    kfree_heap(buf_tmp);
    return res;
}

ssize_t syscall_routine_write(size_t fd, const void *buf, size_t count, proc_t *proc) {
    vfs_handle_t *handle = NULL;

    ssize_t res = syscall_fs_helper_get_not_dir_handle(fd, proc, &handle);
    if (res < 0) {
        printf("syscall_routine_write: failed to get non-directory handle\n");
        return res;
    }

    return vfs_write(handle, buf, count);
}

ssize_t syscall_routine_lseek(size_t fd, ssize_t offset, int whence, proc_t *proc) {
    vfs_handle_t *handle = NULL;

    ssize_t res = syscall_fs_helper_get_handle_and_check_backing_node(fd, proc, &handle);
    if (res < 0) {
        printf("syscall_routine_lseek: failed to get handle with backing node\n");
        return res;
    }

    return vfs_seek(handle, offset, (vfs_seek_whence_t)whence);
}