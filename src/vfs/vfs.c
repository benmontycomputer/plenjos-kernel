#include <stdint.h>

#include "memory/kmalloc.h"

#include "proc/proc.h"

#include "vfs/vfs.h"
#include "vfs/kernelfs.h"

#include "lib/mode.h"
#include "lib/stdio.h"

#include "plenjos/errno.h"
#include "plenjos/syscall.h"

void vfs_init() {
    kernelfs_init();
    fscache_init();
}

int vfs_close(vfs_handle_t *f) {
    if (!f) {
        return -EINVAL;
    }

    if (f->backing_node == NULL || f->backing_node->fsops == NULL || f->backing_node->fsops->close == NULL) {
        return -EIO;
    }

    return f->backing_node->fsops->close(f);
}

ssize_t vfs_read(vfs_handle_t *f, void *buf, size_t len) {
    if (!f) {
        return -EINVAL;
    }

    if ((f->access & ACCESS_READ) == 0) {
        return -EACCES;
    }

    if (f->backing_node == NULL || f->backing_node->fsops == NULL || f->backing_node->fsops->read == NULL) {
        return -EIO;
    }

    return f->backing_node->fsops->read(f, buf, len);
}

ssize_t vfs_write(vfs_handle_t *f, const void *buf, size_t len) {
    if (!f) {
        return -EINVAL;
    }

    if ((f->access & ACCESS_WRITE) == 0) {
        return -EACCES;
    }

    if (f->backing_node == NULL || f->backing_node->fsops == NULL || f->backing_node->fsops->write == NULL) {
        return -EIO;
    }

    return f->backing_node->fsops->write(f, buf, len);
}

ssize_t vfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence) {
    if (!f) {
        return -EINVAL;
    }

    if (f->backing_node == NULL || f->backing_node->fsops == NULL || f->backing_node->fsops->seek == NULL) {
        return -EIO;
    }

    return f->backing_node->fsops->seek(f, offset, whence);
}

// If out is NULL, we just check for existence and permissions
// Permissions are ONLY checked upon file opening
// TODO: implement groups
ssize_t vfs_open(const char *path, syscall_open_flags_t flags, uid_t uid, vfs_handle_t **out) {
    ssize_t res = 0;
    
    // First, find the fscache node
    fscache_node_t *node = NULL;
    res = (ssize_t)fscache_request_node(path, uid, &node);

    if (res == FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY) {
        // The node doesn't exist, but its parent does. If the CREATE flag is set, create it.
        if (flags & SYSCALL_OPEN_FLAG_CREATE) {
            // TODO: implement this
            printf("vfs_open: file creation not yet implemented!\n");
            res = -EIO;
            goto cleanup_and_return;
        } else {
            res = -ENOENT;
            goto cleanup_and_return;
        }
    } else if (res == 0) {
        access_t actual_access = access_check(node->mode, node->uid, node->gid, uid);
        access_t handle_access = 0;

        if (flags & SYSCALL_OPEN_FLAG_READ) {
            if (!(actual_access & ACCESS_READ)) {
                res = -EACCES;
                goto cleanup_and_return;
            }

            handle_access |= ACCESS_READ;
        }

        if (flags & SYSCALL_OPEN_FLAG_WRITE) {
            if (!(actual_access & ACCESS_WRITE)) {
                res = -EACCES;
                goto cleanup_and_return;
            }

            handle_access |= ACCESS_WRITE;
        }

        if (flags & SYSCALL_OPEN_FLAG_EX) {
            if (!(actual_access & ACCESS_EXECUTE)) {
                res = -EACCES;
                goto cleanup_and_return;
            }

            handle_access |= ACCESS_EXECUTE;
        }

        if (out) {
            vfs_handle_t *handle = (vfs_handle_t *)kmalloc_heap(sizeof(vfs_handle_t));
            if (!handle) {
                res = -ENOMEM;
                goto cleanup_and_return;
            }

            handle->backing_node = node;
            int oldval = atomic_fetch_add(&node->ref_count, 1);
            if (oldval < 0 || oldval == INT32_MAX) {
                atomic_fetch_sub(&node->ref_count, 1);
                // Failed to acquire reference
                kfree_heap(handle);
                res = -EIO;
                goto cleanup_and_return;
            }

            handle->access = handle_access;

            // TODO: implement this properly?
            memset(handle->internal_data, 0, sizeof(handle->internal_data));
            *out = handle;
        }

        goto cleanup_and_return;
    } else {
        goto cleanup_and_return;
    }

cleanup_and_return:
    if (node) { _fscache_release_node_readable(node); }
    return res;
}