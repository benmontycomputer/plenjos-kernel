#include "syscall/fs/syscall_fs.h"

#include "lib/string.h"

// TODO: do something to make sure the process can't be closed while we're changing its cwd
ssize_t syscall_routine_chdir(const char *restrict path, proc_t *proc) {
    ssize_t res = 0;

    char path_abs[PATH_MAX] = { 0 };

    res = (ssize_t)handle_relative_path(path, proc, path_abs);
    if (res < 0) {
        printf("syscall_routine_chdir: failed to handle relative path %s for process %s (pid %p), errno %d\n", path,
               proc->name, proc->pid, res);
        return res;
    }

    memset(proc->cwd, 0, PATH_MAX);
    strncpy(proc->cwd, path_abs, PATH_MAX - 1);
    return 0;
}

ssize_t syscall_routine_getcwd(uint64_t out_buf_ptr, size_t size, proc_t *proc, pml4_t *current_pml4) {
    ssize_t res = 0;

    if (size == 0) {
        return -EINVAL;
    }

    size_t cwd_len = strlen(proc->cwd);

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