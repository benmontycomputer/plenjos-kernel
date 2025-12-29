#include "plenjos/stat.h"

#include "lib/stdio.h"
#include "plenjos/errno.h"
#include "plenjos/syscall.h"
#include "proc/proc.h"
#include "syscall/fs/syscall_fs.h"
#include "syscall/syscall_helpers.h"
#include "vfs/fscache.h"
#include "vfs/vfs.h"

// Handle must be valid and have a backing node
static int syscall_routine_stat_from_handle(vfs_handle_t *handle, uint64_t out_stat_ptr, pml4_t *current_pml4) {
    int res = 0;

    struct kstat kstat = { 0 };

    fscache_node_t *node = handle->backing_node;

    kstat.mode = node->mode;
    kstat.uid  = node->uid;
    kstat.gid  = node->gid;
    kstat.size = node->size;

    res = copy_to_user_buf((void *)out_stat_ptr, &kstat, sizeof(struct kstat), false, current_pml4);
    if (res < 0) {
        return res;
    }

    return res;
}

int syscall_routine_stat(const char *restrict path, uint64_t out_stat_ptr, proc_t *proc, pml4_t *current_pml4) {
    int res = 0;

    char *path_abs = kmalloc_heap(PATH_MAX);
    if (!path_abs) {
        printf("OOM Error: syscall_routine_stat: OOM error allocating memory for absolute path buffer\n");
        return -ENOMEM;
    }

    vfs_handle_t *handle = NULL;

    res = handle_relative_path(path, proc, path_abs);
    if (res < 0) {
        printf("syscall_routine_stat: failed to handle relative path %s for process %s (pid %p), errno %d\n", path,
               proc->name, proc->pid, res);
        goto cleanup;
    }

    // No permissions needed on the file itself; we just need to be able to reach it.
    res = vfs_open(path_abs, 0, 0, proc->uid, &handle);
    if (res < 0) {
        printf("syscall_routine_stat: vfs_open failed for path %s for process %s (pid %p), errno %d\n", path_abs,
               proc->name, proc->pid, res);
        goto cleanup;
    }

    if (!handle || handle->backing_node == NULL) {
        printf("syscall_routine_stat: vfs_handle is NULL or has no backing node for path %s for process %s (pid %p)\n",
               path_abs, proc->name, proc->pid);
        res = -EIO;
        goto cleanup;
    }

    res = syscall_routine_stat_from_handle(handle, out_stat_ptr, current_pml4);

cleanup:
    kfree_heap(path_abs);
    if (handle) vfs_close(handle);
    return res;
}

int syscall_routine_fstat(int fd, uint64_t out_stat_ptr, proc_t *proc, pml4_t *current_pml4) {
    vfs_handle_t *handle = NULL;

    int res = syscall_fs_helper_get_handle_and_check_backing_node(fd, proc, &handle);
    if (res < 0) {
        printf("syscall_routine_fstat: failed to get handle with backing node\n");
        return res;
    }

    return syscall_routine_stat_from_handle(handle, out_stat_ptr, current_pml4);
}

int syscall_routine_lstat(const char *restrict path, uint64_t out_stat_ptr, proc_t *proc, pml4_t *current_pml4) {
    // Not implemented yet; for now, just call stat
    // TODO: once links are implemented, change this to not follow links
    return syscall_routine_stat(path, out_stat_ptr, proc, current_pml4);
}