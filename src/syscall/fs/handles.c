#include "lib/stdio.h"
#include "lib/string.h"
#include "plenjos/errno.h"
#include "syscall/fs/syscall_fs.h"
#include "vfs/vfs.h"

int syscall_routine_open(const char *restrict path, syscall_open_flags_t flags, mode_t mode, task_t *task) {
    int res = 0;

    char *path_abs = kmalloc_heap(PATH_MAX);
    if (!path_abs) {
        printf("OOM Error: syscall_routine_open: OOM error allocating memory for absolute path buffer\n");
        return -ENOMEM;
    }

    res = handle_relative_path(path, task, path_abs);
    if (res < 0) {
        printf("syscall_routine_open: failed to handle relative path %s for task %s (tid %p), errno %d\n", path,
               task->name, task->tid, res);
        kfree_heap(path_abs);
        return res;
    }

    vfs_handle_t *handle = NULL;

    res = vfs_open(path_abs, flags, mode, task->uid, &handle);
    if (res < 0) {
        printf("syscall_routine_open: vfs_open failed for path %s for task %s (tid %p), errno %d\n", path_abs,
               task->name, task->tid, res);
        kfree_heap(path_abs);
        return res;
    }

    kfree_heap(path_abs);

    int fd = task_alloc_fd(task, handle);
    if (fd == -1) {
        printf("syscall_routine_open: no available fd for task %s (tid %p)\n", task->name, task->tid);
        vfs_close(handle);
        return -EMFILE;
    }

    return fd;
}

int syscall_routine_close(int fd, task_t *task) {
    vfs_handle_t *handle = task_get_fd(task, fd);
    if (!handle) {
        printf("syscall_routine_close: invalid fd %p for task %s (tid %p)\n", fd, task->name, task->tid);
        return -EINVAL;
    }

    task_free_fd(task, fd);

    vfs_close(handle);
    return 0;
}