#include <stdint.h>

#include "vfs/kernelfs.h"

#include "memory/kmalloc.h"
#include "memory/mm.h"

#include "lib/stdio.h"
#include "lib/string.h"

kernelfs_node_t *root_kernelfs_node = NULL;

ssize_t kernelfs_read(vfs_handle_t *f, void *buf, size_t len);
ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len);
ssize_t kernelfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence);

typedef enum kernelfs_find_node_result {
    KERNELFS_FIND_NODE_PATH_NULL = -1,
    KERNELFS_FIND_NODE_NOT_A_DIRECTORY = -2,
    KERNELFS_FIND_NODE_NOT_FOUND = -3,
    KERNELFS_FIND_NODE_ALLOC_FAILED = -4,
    KERNELFS_FIND_NODE_ROOT_NODE_NULL = -5,
} kernelfs_find_node_result_t;

kernelfs_find_node_result_t kernelfs_find_node(const char *path, kernelfs_node_t **node_out,
                                               vfs_handle_t **handle_out) {
    if (!path) { return KERNELFS_FIND_NODE_PATH_NULL; }

    // Split the path using '/' as delimiter using strtok
    size_t path_copy_len = strlen(path) + 1;
    char *path_copy = kmalloc_heap(path_copy_len);
    if (!path_copy) { return KERNELFS_FIND_NODE_ALLOC_FAILED; }

    strncpy(path_copy, path, path_copy_len);

    char *token;

    // TODO: strtok is NOT thread-safe
    token = strtok(path_copy, "/");

    volatile kernelfs_node_t volatile *current_node = root_kernelfs_node;

    if (!current_node) {
        kfree_heap(path_copy);
        return KERNELFS_FIND_NODE_ROOT_NODE_NULL;
    }

    while (token != NULL) {
        printf("token: %s\n", token);
        // TODO: handle mounted filesystems
        if (current_node->type != VFS_NODE_TYPE_DIR) {
            // Can't traverse into a non-directory
            kfree_heap(path_copy);
            return KERNELFS_FIND_NODE_NOT_A_DIRECTORY;
        }

        current_node = current_node->children;

        while (current_node) {
            printf("checking node %s\n", current_node->name);
            if (strncmp(current_node->name, token, 256) == 0) {
                // Found the node
                break;
            }
            current_node = current_node->next;
        }

        if (!current_node) {
            // Node not found
            kfree_heap(path_copy);
            return KERNELFS_FIND_NODE_NOT_FOUND;
        }

        token = strtok(NULL, "/");
    }

    kfree_heap(path_copy);

    if (node_out) *node_out = current_node;
    if (handle_out) {
        vfs_handle_t *handle = kmalloc_heap(sizeof(vfs_handle_t));
        if (!handle) { return KERNELFS_FIND_NODE_ALLOC_FAILED; }
        handle->type = current_node->type;
        handle->read = kernelfs_read;
        handle->write = kernelfs_write;
        handle->seek = kernelfs_seek;
        handle->close = kernelfs_close;
        handle->func_args = (void *)current_node;

        *handle_out = handle;
    }

    return 0;
}

ssize_t kernelfs_read(vfs_handle_t *f, void *buf, size_t len) {
    kernelfs_node_t *node = (kernelfs_node_t *)f->func_args;

    if (node && node->read) { return node->read(node, buf, len); }
    return -1;
}

ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len) {
    kernelfs_node_t *node = (kernelfs_node_t *)f->func_args;

    if (node && node->write) { return node->write(node, buf, len); }
    return -1;
}

ssize_t kernelfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence) {
    kernelfs_node_t *node = (kernelfs_node_t *)f->func_args;

    if (node && node->seek) { return node->seek(node, offset, whence); }
    return -1;
}

vfs_handle_t *kernelfs_open(const char *path, const char *mode) {
    vfs_handle_t *handle = NULL;

    kernelfs_find_node_result_t res = kernelfs_find_node(path, NULL, &handle);

    return handle;
}

void kernelfs_close(vfs_handle_t *f) {
    kfree_heap(f);
}

int kernelfs_create_node(const char *path, const char *name, vfs_node_type_t type,
                         ssize_t (*read)(kernelfs_node_t *, void *, size_t),
                         ssize_t (*write)(kernelfs_node_t *, const void *, size_t),
                         ssize_t (*seek)(kernelfs_node_t *, ssize_t, vfs_seek_whence_t), void *func_args) {
    printf("Creating kernelfs node %s at %s\n", name, path);

    kernelfs_node_t *parent_node = NULL;

    kernelfs_find_node_result_t res = kernelfs_find_node(path, &parent_node, NULL);

    if (!parent_node || res != 0) {
        // kfree_heap(new_node);
        printf("Failed to find parent node at %s (result %d)\n", path, res);
        return -1;
    }

    if (parent_node->type != VFS_NODE_TYPE_DIR) {
        // kfree_heap(new_node);
        printf("Parent node at %s is not a directory\n", path);
        return -2;
    }

    kernelfs_node_t *new_node = (kernelfs_node_t *)kmalloc_heap(sizeof(kernelfs_node_t));

    if (!new_node) {
        printf("kernelfs_create_node: allocation failed for node %s at %s\n", name, path);
        return -3;
    }

    memset(new_node, 0, sizeof(kernelfs_node_t));

    strncpy(new_node->name, name, 256);
    new_node->name[255] = '\0';
    new_node->type = type;
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

    return 0;
}

void kernelfs_init() {
    root_kernelfs_node = kmalloc_heap(sizeof(kernelfs_node_t));

    memset(root_kernelfs_node, 0, sizeof(kernelfs_node_t));

    root_kernelfs_node->type = VFS_NODE_TYPE_DIR;
    root_kernelfs_node->name[0] = 0;
}