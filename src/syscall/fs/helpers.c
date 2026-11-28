#include "syscall/fs/syscall_fs.h"

int syscall_fs_helper_get_handle_and_check_backing_node(int fd, task_t *task, vfs_handle_t **out_handle) {
    vfs_handle_t *handle = task_handles_get_fd(task->handles, fd);

    if (!handle) {
        printf("syscall_fs_helper_get_handle_and_check_backing_node: invalid fd %p for task %s (tid %p)\n", fd, task->name, task->tid);
        return -EINVAL;
    }

    if (!handle->backing_node) {
        printf("syscall_fs_helper_get_handle_and_check_backing_node: vfs_handle has no backing node for fd %p for task %s (tid %p)\n", fd,
               task->name, task->tid);
        return -EIO;
    }

    *out_handle = handle;
    return 0;
}

int syscall_fs_helper_get_dir_handle(int fd, task_t *task, vfs_handle_t **out_handle) {
    vfs_handle_t *handle = NULL;
    int res = syscall_fs_helper_get_handle_and_check_backing_node(fd, task, &handle);
    if (res < 0) {
        return res;
    }

    if (!S_ISDIR(handle->backing_node->stat.mode)) {
        printf("syscall_fs_helper_get_dir_handle: fd %p for task %s (tid %p) is not a directory\n", fd,
               task->name, task->tid);
        return -ENOTDIR;
    }

    *out_handle = handle;
    return 0;
}

int syscall_fs_helper_get_not_dir_handle(int fd, task_t *task, vfs_handle_t **out_handle) {
    vfs_handle_t *handle = NULL;
    int res = syscall_fs_helper_get_handle_and_check_backing_node(fd, task, &handle);
    if (res < 0) {
        return res;
    }

    if (S_ISDIR(handle->backing_node->stat.mode)) {
        printf("syscall_fs_helper_get_not_dir_handle: fd %p for task %s (tid %p) is a directory\n", fd,
               task->name, task->tid);
        return -EISDIR;
    }

    *out_handle = handle;
    return 0;
}