#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "memory/kmalloc.h"
#include "task.h"

#define DEFAULT_FDS 64

static fixed_size_allocator_t _task_handles_fsa = FSA_DEFAULT(sizeof(task_handles_t));

task_handles_t *task_handles_new(void) {
    task_handles_t *handles = fsa_alloc(&_task_handles_fsa);
    if (!handles) {
        kout(KERNEL_WARN, "task_handles_new(): fsa_alloc() failed (OOM?)\n");
        return NULL;
    }

    handles->fds_max = DEFAULT_FDS;

    size_t fds_size = sizeof(vfs_handle_t *) * handles->fds_max;

    handles->fds = kmalloc_heap(fds_size);
    if (!handles->fds) {
        kout(KERNEL_WARN, "task_handles_new(): couldn't allocate fds table (OOM?)\n");
        fsa_free(&_task_handles_fsa, handles);
        return NULL;
    }
    memset(handles->fds, 0, fds_size);

    handles->ref_cnt = 0;
    handles->lock    = RWSEM_INIT;

    return handles;
}

vfs_handle_t *task_handles_get_fd(task_handles_t *handles, int fd) {
    if (!handles) {
        panic("task_handles_get_fd(): handles %p is NULL\n", handles);
    }

    rwsem_acquire_read(&handles->lock);

    vfs_handle_t *res = NULL;

    if (fd < 0 || fd >= handles->fds_max) {
        goto ret;
    }

    res = handles->fds[fd];

ret:
    rwsem_release_read(&handles->lock);
    return res;
}

int task_handles_alloc_fd(task_handles_t *handles, vfs_handle_t *handle) {
    if (!handles || !handle) {
        panic("task_handles_alloc_fd(): handles %p or handle %p is NULL\n", handles, handle);
    }

    rwsem_acquire_write(&handles->lock);

    int i = 0;
    for (; i < handles->fds_max; i++) {
        if (handles->fds[i] == NULL) {
            handles->fds[i] = handle;
            goto ret;
        }
    }

    // Fail
    i = -1;

ret:
    rwsem_release_write(&handles->lock);
    return i;
}

// WARNING: this does *not* free the underlying vfs_handle_t!
int task_handles_free_fd(task_handles_t *handles, int fd) {
    if (!handles) {
        panic("task_handles_free_fd(): handles %p is NULL\n", handles);
    }

    rwsem_acquire_write(&handles->lock);
    int res = 0;

    if (fd < 0 || fd >= handles->fds_max || !handles->fds[fd]) {
        res = -EBADF;
        goto ret;
    }

    handles->fds[fd] = NULL;

ret:
    rwsem_release_write(&handles->lock);
    return res;
}

int task_handles_ref(task_handles_t *handles) {
    return atomic_fetch_add(&handles->ref_cnt, 1);
}

int task_handles_unref(task_handles_t *handles) {
    int prev = atomic_fetch_sub(&handles->ref_cnt, 1);

    if (prev == 1) {
        // Free
        for (int i = 0; i < handles->fds_max; i++) {
            if (handles->fds[i]) {
                int res = vfs_close(handles->fds[i]);
                if (res) {
                    kout(KERNEL_WARN,
                         "task_handles_unref(): vfs_close failed with res %d for handle %d on handles %p; continuing "
                         "anyways.\n",
                         res, i, handles);
                }
                handles->fds[i] = NULL;
            }
        }

        kfree_heap(handles->fds);
        fsa_free(&_task_handles_fsa, handles);
    }

    return prev;
}