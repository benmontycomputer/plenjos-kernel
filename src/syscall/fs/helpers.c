#include "syscall/fs/syscall_fs.h"

ssize_t syscall_fs_helper_get_handle_and_check_backing_node(size_t fd, proc_t *proc, vfs_handle_t **out_handle) {
    vfs_handle_t *handle = proc_get_fd(proc, fd);

    if (!handle) {
        printf("syscall_fs_helper_get_handle_and_check_backing_node: invalid fd %p for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return -EINVAL;
    }

    if (!handle->backing_node) {
        printf("syscall_fs_helper_get_handle_and_check_backing_node: vfs_handle has no backing node for fd %p for process %s (pid %p)\n", fd,
               proc->name, proc->pid);
        return -EIO;
    }

    *out_handle = handle;
    return 0;
}

ssize_t syscall_fs_helper_get_dir_handle(size_t fd, proc_t *proc, vfs_handle_t **out_handle) {
    vfs_handle_t *handle = NULL;
    ssize_t res = syscall_fs_helper_get_handle_and_check_backing_node(fd, proc, &handle);
    if (res < 0) {
        return res;
    }

    if (handle->backing_node->type != DT_DIR) {
        printf("syscall_fs_helper_get_dir_handle: fd %p for process %s (pid %p) is not a directory\n", fd,
               proc->name, proc->pid);
        return -ENOTDIR;
    }

    *out_handle = handle;
    return 0;
}

ssize_t syscall_fs_helper_get_not_dir_handle(size_t fd, proc_t *proc, vfs_handle_t **out_handle) {
    vfs_handle_t *handle = NULL;
    ssize_t res = syscall_fs_helper_get_handle_and_check_backing_node(fd, proc, &handle);
    if (res < 0) {
        return res;
    }

    if (handle->backing_node->type == DT_DIR) {
        printf("syscall_fs_helper_get_not_dir_handle: fd %p for process %s (pid %p) is a directory\n", fd,
               proc->name, proc->pid);
        return -EISDIR;
    }

    *out_handle = handle;
    return 0;
}