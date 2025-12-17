#include <stdint.h>

#include "vfs/kernelfs.h"

#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "proc/proc.h"

#include "lib/stdio.h"
#include "lib/string.h"

#include "plenjos/dirent.h"
#include "plenjos/errno.h"
#include "plenjos/stat.h"
#include "plenjos/syscall.h"

kernelfs_node_t *root_kernelfs_node = NULL;

ssize_t kernelfs_read(vfs_handle_t *f, void *buf, size_t len);
ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len);
ssize_t kernelfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence);
ssize_t kernelfs_load(fscache_node_t *node, const char *name, fscache_node_t *out);

int kernelfs_create_node(kernelfs_node_t *parent_node, const char *name, uint8_t type, uid_t uid, gid_t gid,
                         mode_t mode, kernelfs_open_func_t open, vfs_read_func_t read, vfs_write_func_t write,
                         vfs_seek_func_t seek, void *func_args, kernelfs_node_t **out);

typedef enum kernelfs_find_node_result {
    KERNELFS_FIND_NODE_HANDLE_ONLY = 1,
    KERNELFS_FIND_NODE_SUCCESS = 0,
    KERNELFS_FIND_NODE_PATH_NULL = -1,
    KERNELFS_FIND_NODE_NOT_A_DIRECTORY = -2,
    KERNELFS_FIND_NODE_NOT_FOUND = -3,
    KERNELFS_FIND_NODE_ALLOC_FAILED = -4,
    KERNELFS_FIND_NODE_ROOT_NODE_NULL = -5,
} kernelfs_find_node_result_t;

kernelfs_node_t *kernelfs_get_node_from_handle(vfs_handle_t *handle) {
    if (!handle || !handle->backing_node) { return NULL; }

    kernelfs_cache_data_t *cache_data = (kernelfs_cache_data_t *)handle->backing_node->internal_data;
    if (!cache_data) { return NULL; }

    return cache_data->node;
}

/* The following few functions are meant to only be put inside of a vfs_handle_t. Permissions MUST be checked
 * beforehand. */
ssize_t kernelfs_read(vfs_handle_t *f, void *buf, size_t len) {
    kernelfs_node_t *node = kernelfs_get_node_from_handle(f);

    if (node && node->read) { return node->read(f, buf, len); }
    return -1;
}

ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len) {
    kernelfs_node_t *node = kernelfs_get_node_from_handle(f);

    if (node && node->write) { return node->write(f, buf, len); }
    return -1;
}

ssize_t kernelfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence) {
    kernelfs_node_t *node = kernelfs_get_node_from_handle(f);

    if (node && node->seek) { return node->seek(f, offset, whence); }
    return -1;
}

// IMPORTANT: "out" must point to an already allocated fscache_node_t pointer
ssize_t kernelfs_load(fscache_node_t *node, const char *name, fscache_node_t *out) {
    kernelfs_node_t *parent_node = node ? ((kernelfs_cache_data_t *)node->internal_data)->node : root_kernelfs_node;

    kernelfs_node_t *current_node;

    if (name == NULL) {
        current_node = parent_node;
        goto node_found;
    } else {
        current_node = parent_node->children;
    }

    while (current_node) {
        if (strncmp(current_node->name, name, NAME_MAX + 1) == 0) { goto node_found; }
        current_node = current_node->next;
    }

    printf("kernelfs_load: node %s not found under parent %s\n", name, parent_node ? parent_node->name : "(null)");
    return -ENOENT;

// Found the node
node_found:
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

void kernelfs_close(vfs_handle_t *f) {
    kfree_heap(f);
}

int kernelfs_helper_mkdir(const char *parent, const char *name, uid_t uid, gid_t gid, mode_t mode) {
    vfs_handle_t *parent_handle = NULL;
    ssize_t res = vfs_open(parent, SYSCALL_OPEN_FLAG_DIRECTORY | SYSCALL_OPEN_FLAG_WRITE, 0, &parent_handle);
    if (res < 0) {
        printf("kernelfs_helper_mkdir: failed to open parent directory %s, errno %d\n", parent, res);
        return res;
    }

    kernelfs_node_t *parent_node = kernelfs_get_node_from_handle(parent_handle);
    if (!parent_node) {
        printf("kernelfs_helper_mkdir: failed to get kernelfs node from handle for parent directory %s\n", parent);
        vfs_close(parent_handle);
        return -EIO;
    }

    kernelfs_node_t *new_node = NULL;
    // TODO: implement some of these functions?
    res = kernelfs_create_node(parent_node, name, DT_DIR, uid, gid, mode, NULL, NULL, NULL, NULL, NULL, &new_node);
    if (res < 0) {
        printf("kernelfs_helper_mkdir: failed to create kernelfs node %s under parent %s, errno %d\n", name, parent,
               res);
        vfs_close(parent_handle);
        return res;
    }

    return 0;
}

int kernelfs_helper_create_file(const char *parent, const char *name, uint8_t type, uid_t uid, gid_t gid, mode_t mode,
                                vfs_read_func_t read, vfs_write_func_t write, vfs_seek_func_t seek, void *func_args) {
    vfs_handle_t *parent_handle = NULL;
    ssize_t res = vfs_open(parent, SYSCALL_OPEN_FLAG_DIRECTORY | SYSCALL_OPEN_FLAG_WRITE, 0, &parent_handle);
    if (res < 0) {
        printf("kernelfs_helper_create_file: failed to open parent directory %s, errno %d\n", parent, res);
        return res;
    }

    kernelfs_node_t *parent_node = kernelfs_get_node_from_handle(parent_handle);
    if (!parent_node) {
        printf("kernelfs_helper_create_file: failed to get kernelfs node from handle for parent directory %s\n",
               parent);
        vfs_close(parent_handle);
        return -EIO;
    }

    kernelfs_node_t *new_node = NULL;
    res = kernelfs_create_node(parent_node, name, type, uid, gid, mode, NULL, read, write, seek, func_args, &new_node);
    if (res < 0) {
        printf("kernelfs_helper_create_file: failed to create kernelfs node %s under parent %s, errno %d\n", name,
               parent, res);
        vfs_close(parent_handle);
        return res;
    }

    return 0;
}

int kernelfs_create_node(kernelfs_node_t *parent_node, const char *name, uint8_t type, uid_t uid, gid_t gid,
                         mode_t mode, kernelfs_open_func_t open, vfs_read_func_t read, vfs_write_func_t write,
                         vfs_seek_func_t seek, void *func_args, kernelfs_node_t **out) {
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
    new_node->uid = uid;
    new_node->gid = gid;
    new_node->mode = mode;
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

    root_kernelfs_node->uid = 0;
    root_kernelfs_node->gid = 0;
    root_kernelfs_node->mode = S_DFLT; // | S_IFDIR;

    kernelfs_create_node(root_kernelfs_node, "testfs", DT_DIR, 0, 0, S_DFLT, NULL, NULL, NULL, NULL, NULL, NULL);

    // kernelfs_open("/testfs", "r");
    // kernelfs_open("/testfs/tmp", "r");
}