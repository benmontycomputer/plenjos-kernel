#include "vfs/vfs.h"

#include "lib/mode.h"
#include "lib/stdio.h"
#include "memory/kmalloc.h"
#include "plenjos/errno.h"
#include "plenjos/syscall.h"
#include "vfs/kernelfs.h"

#include <stdint.h>

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

    if (f->backing_node == NULL) {
        printf("vfs_read: backing_node is NULL\n");
        return -EIO;
    }

    if (f->backing_node->fsops == NULL) {
        printf("vfs_read: fsops is NULL\n");
        return -EIO;
    }

    if (f->backing_node->fsops->read == NULL) {
        printf("vfs_read: read function is NULL\n");
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

off_t vfs_seek(vfs_handle_t *f, off_t offset, vfs_seek_whence_t whence) {
    if (!f) {
        return -EINVAL;
    }

    if (f->backing_node == NULL || f->backing_node->fsops == NULL || f->backing_node->fsops->seek == NULL) {
        return -EIO;
    }

    return f->backing_node->fsops->seek(f, offset, whence);
}

// Helper function to create a vfs_handle_t; DOES NOT check permissions. out MUST NOT be NULL.
static int vfs_helper_create_handle(fscache_node_t *node, access_t access, vfs_handle_t **out) {
    if (!node) {
        return -EINVAL;
    }

    vfs_handle_t *handle = (vfs_handle_t *)kmalloc_heap(sizeof(vfs_handle_t));
    if (!handle) {
        return -ENOMEM;
    }

    handle->backing_node = node;
    int oldval           = atomic_fetch_add(&node->ref_count, 1);
    if (oldval < 0 || oldval == INT32_MAX) {
        atomic_fetch_sub(&node->ref_count, 1);
        // Failed to acquire reference
        kfree_heap(handle);
        return -EIO;
    }

    handle->access = access;

    // TODO: implement this properly?
    memset(handle->instance_data, 0, sizeof(handle->instance_data));
    *out = handle;

    return 0;
}

// If out is NULL, we just check for existence and permissions
// Permissions are ONLY checked upon file opening
// TODO: implement groups
// TODO: do we want SYSCALL_OPEN_FLAG_EX to actually be a valid flag?
int vfs_open(const char *path, syscall_open_flags_t flags, mode_t mode_if_create, uid_t uid, vfs_handle_t **out) {
    if (!path) {
        return -EINVAL;
    }

    if (path[0] != '/') {
        // Only absolute paths are supported; otherwise we'd need to pass another argument to this function which
        // doesn't make much sense.
        printf("ERROR: vfs_open: only absolute paths are supported (got %s). This indicates a severe bug or error in the (kernel code) caller.\n", path);
        return -EINVAL;
    }

    int res = 0;

    if ((flags & SYSCALL_OPEN_FLAG_CREATE) && (flags & SYSCALL_OPEN_FLAG_DIRECTORY)) {
        // Invalid combination of flags
        return -EINVAL;
    }

    // First, find the fscache node
    fscache_node_t *node = NULL;
    res                  = fscache_request_node(path, uid, &node);

    if (res == FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY) {
        // The node doesn't exist, but its parent does. If the CREATE flag is set, create it.
        if (flags & SYSCALL_OPEN_FLAG_CREATE) {
            // We've already checked that the DIRECTORY flag is not set alongside CREATE
            access_t actual_access = access_check(node->mode, node->uid, node->gid, uid);
            if (!(actual_access & ACCESS_WRITE) || !(actual_access & ACCESS_EXECUTE)) {
                res = -EACCES;
                goto cleanup_and_return;
            }

            const char *p          = path;
            const char *last_slash = NULL;
            while (*p != '\0') {
                if (*p == '/') {
                    last_slash = p;
                }
                p++;
            }

            last_slash = last_slash ? last_slash + 1 : path;

            fscache_node_t *new_node = NULL;
            // This function properly masks the mode; we don't have to do that here.
            res                      = vfs_creatat(node, last_slash, uid, 0 /* TODO: use egid here */, mode_if_create, &new_node);

            if (res < 0) {
                goto cleanup_and_return;
            }

            access_t handle_access = 0;
            if (flags & SYSCALL_OPEN_FLAG_READ) {
                handle_access |= ACCESS_READ;
            }
            if (flags & SYSCALL_OPEN_FLAG_WRITE) {
                handle_access |= ACCESS_WRITE;
            }
            if (flags & SYSCALL_OPEN_FLAG_EX) {
                handle_access |= ACCESS_EXECUTE;
            }

            if (out) {
                res = vfs_helper_create_handle(new_node, handle_access, out);
            }

            _fscache_release_node_readable(new_node);

            goto cleanup_and_return;
        } else {
            res = -ENOENT;
            goto cleanup_and_return;
        }
    } else if (res == 0) {
        if ((flags & SYSCALL_OPEN_FLAG_CREATE) && (flags & SYSCALL_OPEN_FLAG_EXCL)) {
            res = -EEXIST;
            goto cleanup_and_return;
        }

        if ((flags & SYSCALL_OPEN_FLAG_DIRECTORY) && node->type != DT_DIR) {
            res = -ENOTDIR;
            goto cleanup_and_return;
        }

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
            res = vfs_helper_create_handle(node, handle_access, out);
        }

        goto cleanup_and_return;
    } else {
        goto cleanup_and_return;
    }

cleanup_and_return:
    if (node) {
        _fscache_release_node_readable(node);
    }
    return res;
}

// Unlike other functions, this actually handles allocation of the fscache_node_t.
// If out isn't passed as NULL and the function succeeds, the resulting node is read-locked.
int vfs_creatat(fscache_node_t *parent, const char *name, uid_t uid, gid_t gid, mode_t mode, fscache_node_t **out) {
    if (!parent || !name) {
        return -EINVAL;
    }

    if (parent->fsops == NULL || parent->fsops->create_child == NULL) {
        printf("vfs_creatat: file system doesn't support create_child\n");
        return -EIO;
    }

    fscache_node_t *new_node = fscache_allocate_node();
    if (!new_node) {
        printf("vfs_creatat: failed to allocate fscache node for new file %s\n", name);
        return -ENOMEM;
    }

    mode_t masked_mode  = mode & ~022; // TODO: apply actual umask from process
    masked_mode        &= 0xFFF;       // Only lower 12 bits are honored

    int res = parent->fsops->create_child(parent, name, DT_REG, uid, gid, mode, new_node);

    if (res < 0) {
        _fscache_release_node_readable(new_node);
        return res;
    }

    if (out) {
        *out = new_node;
    } else {
        _fscache_release_node_readable(new_node);
    }

    return res;
}

// This honors the lower 10 (not 12) bits of mode (the rwxrwxrwx bits and the sticky bit).
// Unlike vfs_creatat, this function does NOT honor the setuid and setgid bits, as they have no meaning on directories.
int vfs_mkdir(const char *path, uid_t uid, gid_t gid, mode_t mode) {
    if (!path) {
        return -EINVAL;
    }

    if (path[0] != '/') {
        // Only absolute paths are supported; otherwise we'd need to pass another argument to this function which
        // doesn't make much sense.
        printf("ERROR: vfs_mkdir: only absolute paths are supported (got %s). This indicates a severe bug or error in the (kernel code) caller.\n", path);
        return -EINVAL;
    }

    fscache_node_t *parent = NULL;
    int res            = fscache_request_node(path, uid, &parent);
    if (res == FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY) {
        // We need w and x permissions on the parent
        access_t actual_access = access_check(parent->mode, parent->uid, parent->gid, uid);
        if (!(actual_access & ACCESS_WRITE) || !(actual_access & ACCESS_EXECUTE)) {
            res = -EACCES;
            goto cleanup;
        }

        // Parent exists; create the directory under it
        char *last_slash = NULL;
        const char *p    = path;
        while (*p != '\0') {
            if (*p == '/') {
                last_slash = (char *)p;
            }
            p++;
        }

        last_slash = last_slash ? last_slash + 1 : (char *)path;

        if (!parent->fsops || !parent->fsops->create_child) {
            printf("vfs_mkdir: filesystem does not support create_child on path %s\n", path);
            res = -EIO;
            goto cleanup;
        }

        fscache_node_t *new_dir_node = fscache_allocate_node();
        if (!new_dir_node) {
            printf("vfs_mkdir: failed to allocate fscache node for new directory %s\n", last_slash);
            res = -ENOMEM;
            goto cleanup;
        }

        mode_t masked_mode = mode & ~022 & (0777 | S_ISVTX); // TODO: apply actual umask from process

        res = parent->fsops->create_child(parent, last_slash, DT_DIR, uid, gid, masked_mode,
                                          new_dir_node);
        _fscache_release_node_readable(new_dir_node);
        goto cleanup;
    } else if (res == 0) {
        // Directory already exists
        res = -EEXIST;
        goto cleanup;
    } else {
        // Some other error
        goto cleanup;
    }

cleanup:
    if (parent) {
        _fscache_release_node_readable(parent);
    }
    return res;
}