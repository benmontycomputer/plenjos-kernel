#include <stdint.h>

#include "vfs/kernelfs.h"

#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "proc/proc.h"

#include "lib/stdio.h"
#include "lib/string.h"

#include "plenjos/dirent.h"
#include "plenjos/errno.h"
#include "plenjos/syscall.h"

kernelfs_node_t *root_kernelfs_node = NULL;

ssize_t kernelfs_read(vfs_handle_t *f, void *buf, size_t len);
ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len);
ssize_t kernelfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence);
ssize_t kernelfs_load(fscache_node_t *node, const char *name, fscache_node_t *out);

typedef enum kernelfs_find_node_result {
    KERNELFS_FIND_NODE_HANDLE_ONLY = 1,
    KERNELFS_FIND_NODE_SUCCESS = 0,
    KERNELFS_FIND_NODE_PATH_NULL = -1,
    KERNELFS_FIND_NODE_NOT_A_DIRECTORY = -2,
    KERNELFS_FIND_NODE_NOT_FOUND = -3,
    KERNELFS_FIND_NODE_ALLOC_FAILED = -4,
    KERNELFS_FIND_NODE_ROOT_NODE_NULL = -5,
} kernelfs_find_node_result_t;

/* The following few functions are meant to only be put inside of a vfs_handle_t. Permissions MUST be checked beforehand. */
ssize_t kernelfs_read(vfs_handle_t *f, void *buf, size_t len) {
    kernelfs_node_t *node = ((kernelfs_handle_instance_data_t *)f->internal_data)->node;

    if (node && node->read) { return node->read(node, buf, len); }
    return -1;
}

ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len) {
    kernelfs_node_t *node = ((kernelfs_handle_instance_data_t *)f->internal_data)->node;

    if (node && node->write) { return node->write(node, buf, len); }
    return -1;
}

ssize_t kernelfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence) {
    kernelfs_node_t *node = ((kernelfs_handle_instance_data_t *)f->internal_data)->node;

    if (node && node->seek) { return node->seek(node, offset, whence); }
    return -1;
}

// IMPORTANT: "out" must point to an already allocated fscache_node_t pointer
ssize_t kernelfs_load(fscache_node_t *node, const char *name, fscache_node_t *out) {
    kernelfs_node_t *parent_node = ((kernelfs_cache_data_t *)node->internal_data)->node;

    kernelfs_node_t *current_node = parent_node->children;
    while (current_node) {
        if (strncmp(current_node->name, name, NAME_MAX + 1) == 0) {
            // Found the node
            if (out) {
                strncpy(out->name, current_node->name, NAME_MAX + 1);

                out->type = current_node->type;
                out->uid = current_node->uid;
                out->gid = current_node->gid;
                out->mode = current_node->mode;

                out->flags = 0;

                out->fsops = kmalloc_heap(sizeof(vfs_ops_block_t));
                if (!out->fsops) { return -ENOMEM; }

                // Clear everything first; only assign to the pointers we actually want
                memset(out->fsops, 0, sizeof(vfs_ops_block_t));

                out->fsops->read = current_node->read;
                out->fsops->write = current_node->write;
                out->fsops->seek = current_node->seek;
                out->fsops->close = NULL;
                out->fsops->load_node = kernelfs_load;

                ((kernelfs_cache_data_t *)out->internal_data)->node = current_node;
            }
            return 0;
        }
        current_node = current_node->next;
    }
}

// TODO: we need to implement permissions and locks
/* ssize_t kernelfs_open(const char *path, uint64_t flags, uint64_t mode, proc_t *proc, vfs_handle_t **out) {
    if (!path) { return KERNELFS_FIND_NODE_PATH_NULL; }

    // Split the path using '/' as delimiter using strtok
    size_t path_copy_len = strlen(path) + 1;
    char *path_copy = kmalloc_heap(path_copy_len);
    if (!path_copy) { return KERNELFS_FIND_NODE_ALLOC_FAILED; }

    strncpy(path_copy, path, path_copy_len);

    char *token;

    // TODO: strtok is NOT thread-safe
    token = strtok(path_copy, "/");

    volatile kernelfs_node_t *parent_node = NULL;
    volatile kernelfs_node_t *current_node = root_kernelfs_node;

    if (!current_node) {
        kfree_heap(path_copy);
        return KERNELFS_FIND_NODE_ROOT_NODE_NULL;
    }

    // TODO: handle "." and ".."
    while (token != NULL) {
        printf("token: %s\n", token);
        // TODO: handle mounted filesystems
        /* if (current_node->type == DT_BLK) {
            char *rem = strtok(NULL, "");
            printf("Opening block device (path_copy: %s, token: %s, rem: %s)\n", path_copy, token, rem ? rem :
        "{NULL}"); if (current_node->open) {
                // We're at the end of the path

                // current_node->open(, mode);
            } else {
                printf("Trying to open a block device node with no open function! (path_copy: %s, token: %s)\n",
        path_copy, token);
            }

            return KERNELFS_FIND_NODE_NOT_FOUND;
        } *//*
        // TODO: handle links
        if (current_node->type != DT_DIR) {
            // Can't traverse into a non-directory
            kfree_heap(path_copy);
            return KERNELFS_FIND_NODE_NOT_A_DIRECTORY;
        }

        parent_node = current_node;
        current_node = current_node->children;

        while (current_node) {
            printf("checking node %s\n", current_node->name);
            if (strncmp(current_node->name, token, NAME_MAX + 1) == 0) {
                // Found the node
                goto end_kfs_open_loop;
            }
            current_node = current_node->next;
        }

        if (!current_node) {
            char *rem = strtok(NULL, "");

            if (!rem) {
                // We are at the end; do we want to create the file/directory?
                // Also, this means that we are creating a node in the kernelfs, not in any mounted filesystems

                if (flags & SYSCALL_OPEN_FLAG_CREATE) {
                    uint8_t type = (flags & SYSCALL_OPEN_FLAG_DIRECTORY) ? DT_DIR : DT_REG;
                    int res = kernelfs_create_node(parent_node, token, type,  NULL, NULL, NULL, NULL, NULL, out);

                    kfree_heap(path_copy);
                    return res;
                }
            }

            // Node not found
            kfree_heap(path_copy);
            return KERNELFS_FIND_NODE_NOT_FOUND;
        }

        token = strtok(NULL, "/");
    }

end_kfs_open_loop:

    kfree_heap(path_copy);

    if (out == NULL) { return 0; }

    *out = current_node;

    return 0;
}*/

void kernelfs_close(vfs_handle_t *f) {
    kfree_heap(f);
}

int kernelfs_create_node(kernelfs_node_t *parent_node, const char *name, uint8_t type, uid_t uid, gid_t gid,
                         mode_t mode, ssize_t (*open)(const char *path, const char *mode),
                         ssize_t (*read)(kernelfs_node_t *, void *, size_t),
                         ssize_t (*write)(kernelfs_node_t *, const void *, size_t),
                         ssize_t (*seek)(kernelfs_node_t *, ssize_t, vfs_seek_whence_t), void *func_args,
                         kernelfs_node_t **out) {
    printf("Creating kernelfs node %s\n", name);

    if (!parent_node) {
        printf("Parent node can't be null!\n");
        return -1;
    }

    if (parent_node->type != DT_DIR) {
        printf("Parent node is not a directory!\n");
        return -2;
    }

    kernelfs_node_t *new_node = (kernelfs_node_t *)kmalloc_heap(sizeof(kernelfs_node_t));

    if (!new_node) {
        printf("kernelfs_create_node: allocation failed for node %s.\n", name);
        return -3;
    }

    memset(new_node, 0, sizeof(kernelfs_node_t));

    strncpy(new_node->name, name, NAME_MAX);
    new_node->name[NAME_MAX] = '\0';
    new_node->type = type;
    new_node->open = open;
    new_node->read = read;
    new_node->write = write;
    new_node->seek = seek;

    new_node->prev = NULL;
    new_node->next = NULL;
    new_node->children = NULL;
    new_node->parent = parent_node;

    new_node->func_args = func_args;

    // Insert into the parent node's children
    if (parent_node->children == NULL) {
        parent_node->children = new_node;
    } else {
        kernelfs_node_t *child = parent_node->children;
        while (child->next != NULL) {
            // printf("loopt");
            child = child->next;
        }
        child->next = new_node;
        new_node->prev = child;
    }

    if (out) *out = new_node;

    return 0;
}

void kernelfs_init() {
    root_kernelfs_node = kmalloc_heap(sizeof(kernelfs_node_t));

    memset(root_kernelfs_node, 0, sizeof(kernelfs_node_t));

    root_kernelfs_node->type = DT_DIR;
    root_kernelfs_node->name[0] = 0;

    kernelfs_create_node(root_kernelfs_node, "testfs", DT_DIR, NULL, NULL, NULL, NULL, NULL);

    // kernelfs_open("/testfs", "r");
    // kernelfs_open("/testfs/tmp", "r");
}