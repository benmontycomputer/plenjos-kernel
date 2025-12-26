#include "lib/string.h"
#include "syscall/fs/syscall_fs.h"
#include "vfs/fscache.h"

// TODO: do something to make sure the process can't be closed while we're changing its cwd
int syscall_routine_chdir(const char *restrict path, proc_t *proc) {
    int res = 0;

    char *path_abs = kmalloc_heap(PATH_MAX);
    if (!path_abs) {
        printf("syscall_routine_chdir: OOM error allocating memory for absolute path buffer\n");
        return -ENOMEM;
    }

    res = handle_relative_path(path, proc, path_abs);
    if (res < 0) {
        printf("syscall_routine_chdir: failed to handle relative path %s for process %s (pid %p), errno %d\n", path,
               proc->name, proc->pid, res);
        kfree_heap(path_abs);
        return res;
    }

    res = collapse_path(path_abs, path_abs);
    if (res < 0) {
        printf("syscall_routine_chdir: failed to collapse path %s for process %s (pid %p), errno %d\n", path_abs,
               proc->name, proc->pid, res);
        kfree_heap(path_abs);
        return res;
    }

    res = fscache_request_node(path_abs, proc->uid, NULL);
    if (res < 0) {
        kfree_heap(path_abs);
        return res;
    } else if (res == FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY) {
        kfree_heap(path_abs);
        return -ENOENT;
    }

    memset(proc->cwd, 0, PATH_MAX);
    strncpy(proc->cwd, path_abs, PATH_MAX);
    proc->cwd[PATH_MAX - 1] = '\0';
    kfree_heap(path_abs);
    return 0;
}

ssize_t syscall_routine_getcwd(uint64_t out_buf_ptr, ssize_t size, proc_t *proc, pml4_t *current_pml4) {
    ssize_t res = 0;

    if (size == 0) {
        return -EINVAL;
    }

    // It's impossible for strlen to return a value >= SSIZE_MAX, because any string that long would not fit in virtual
    // memory (all 64 bits would need to be addressable)
    ssize_t cwd_len = (ssize_t)strlen(proc->cwd);

    if (cwd_len + 1 > size) {
        return -ERANGE;
    }

    res = (ssize_t)copy_to_user_buf((void *)out_buf_ptr, proc->cwd, cwd_len + 1, current_pml4);
    if (res < 0) {
        printf("syscall_routine_getcwd: failed to copy cwd to user buffer %p for process %s (pid %p), errno %d\n",
               (void *)out_buf_ptr, proc->name, proc->pid, res);
        return res;
    }

    return (ssize_t)(cwd_len + 1);
}