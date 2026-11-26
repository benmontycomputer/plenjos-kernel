#include "lib/stdio.h"
#include "memory/kmalloc.h"
#include "plenjos/errno.h"
#include "syscall/fs/syscall_fs.h"
#include "syscall/syscall_helpers.h"
#include "vfs/fscache.h"
#include "vfs/vfs.h"

ssize_t syscall_routine_read(int fd, void *buf, size_t count, task_t *task, mm_t *mm) {
    vfs_handle_t *handle = NULL;

    ssize_t res = (ssize_t)syscall_fs_helper_get_not_dir_handle(fd, task, &handle);
    if (res < 0) {
        printf("syscall_routine_read: failed to get non-directory handle\n");
        return res;
    }

    void *buf_tmp = kmalloc_heap(count);

    if (!buf_tmp) {
        printf("syscall_routine_read: failed to allocate temporary buffer for read of %p bytes for task %s (tid %p)\n",
               count, task->name, task->tid);
        return -ENOMEM;
    }

    // TODO: (SECURITY RISK) check that the final output buffer is valid before reading from the fs at all.
    res = vfs_read(handle, buf_tmp, count);
    // TODO: Sockets, etc. should BLOCK by default

    if (res > 0) {
        int check = copy_to_user_buf(buf, buf_tmp, res, false, mm);
        if (check < 0) {
            printf("syscall_routine_read: failed to copy data to user buffer %p for task %s (tid %p), errno %d\n", buf,
                   task->name, task->tid, check);
            kfree_heap(buf_tmp);
            return (ssize_t)check;
        }
    }

    kfree_heap(buf_tmp);
    return res;
}

ssize_t syscall_routine_write(int fd, const void *buf, size_t count, task_t *task) {
    vfs_handle_t *handle = NULL;

    ssize_t res = (ssize_t)syscall_fs_helper_get_not_dir_handle(fd, task, &handle);
    if (res < 0) {
        printf("syscall_routine_write: failed to get non-directory handle\n");
        return res;
    }

    return vfs_write(handle, buf, count);
}

off_t syscall_routine_lseek(int fd, off_t offset, int whence, task_t *task) {
    vfs_handle_t *handle = NULL;

    off_t res = (off_t)syscall_fs_helper_get_handle_and_check_backing_node(fd, task, &handle);
    if (res < 0) {
        printf("syscall_routine_lseek: failed to get handle with backing node\n");
        return res;
    }

    return vfs_seek(handle, offset, (vfs_seek_whence_t)whence);
}