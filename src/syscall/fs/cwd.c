#include "lib/string.h"
#include "syscall/fs/syscall_fs.h"
#include "vfs/fscache.h"

// TODO: do something to make sure the task can't be closed while we're changing its cwd
int syscall_routine_chdir(const char *restrict path, task_t *task) {
    int res = 0;

    char *path_abs = kmalloc_heap(PATH_MAX);
    if (!path_abs) {
        printf("syscall_routine_chdir: OOM error allocating memory for absolute path buffer\n");
        return -ENOMEM;
    }

    res = handle_relative_path(path, task, path_abs);
    if (res < 0) {
        printf("syscall_routine_chdir: failed to handle relative path %s for task %s (tid %p), errno %d\n", path,
               task->name, task->tid, res);
        kfree_heap(path_abs);
        return res;
    }

    res = fscache_request_node(path_abs, task->uid, NULL);
    if (res < 0) {
        kfree_heap(path_abs);
        return res;
    } else if (res == FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY) {
        kfree_heap(path_abs);
        return -ENOENT;
    }

    memset(task->cwd, 0, PATH_MAX);
    strncpy(task->cwd, path_abs, PATH_MAX);
    task->cwd[PATH_MAX - 1] = '\0';
    kfree_heap(path_abs);
    return 0;
}

ssize_t syscall_routine_getcwd(uint64_t out_buf_ptr, ssize_t size, task_t *task, mm_t *mm) {
    ssize_t res = 0;

    if (size == 0) {
        return -EINVAL;
    }

    // It's impossible for strlen to return a value >= SSIZE_MAX, because any string that long would not fit in virtual
    // memory (all 64 bits would need to be addressable)
    ssize_t cwd_len = (ssize_t)strlen(task->cwd);

    if (cwd_len + 1 > size) {
        return -ERANGE;
    }

    res = (ssize_t)copy_to_user_buf((void *)out_buf_ptr, task->cwd, cwd_len + 1, false, mm);
    if (res < 0) {
        printf("syscall_routine_getcwd: failed to copy cwd to user buffer %p for task %s (tid %p), errno %d\n",
               (void *)out_buf_ptr, task->name, task->tid, res);
        return res;
    }

    return (ssize_t)(cwd_len + 1);
}