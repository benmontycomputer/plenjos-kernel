#include <stdint.h>

#include "vfs/kernelfs.h"

#include "memory/kmalloc.h"

#include "lib/stdio.h"
#include "lib/string.h"

kernelfs_node_t *root_kernelfs_node = NULL;

ssize_t kernelfs_read(vfs_handle_t *f, void *buf, size_t len);
ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len);
ssize_t kernelfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence);

int kernelfs_find_node(const char *path, kernelfs_node_t **node_out, vfs_handle_t **handle_out) {
    // Split the path using '/' as delimiter using strtok
    char *path_copy = kmalloc_heap(strlen(path) + 1);
    strncpy(path_copy, path, strlen(path) + 1);

    if (path_copy[0] == '/') {
        // Remove leading slashes
        strtok(path_copy, "/");
    }

    char *token = strtok(path_copy, "/");

    kernelfs_node_t *current_node = root_kernelfs_node;

    while (token != NULL) {
        // TODO: handle mounted filesystems
        if (current_node->type != VFS_NODE_TYPE_DIR) {
            // Can't traverse into a non-directory
            kfree_heap(path_copy);
            return -1;
        }

        current_node = current_node->children;

        while (current_node) {
            if (strncmp(current_node->name, token, 256) == 0) {
                // Found the node
                break;
            }
            current_node = current_node->next;
        }

        if (!current_node) {
            // Node not found
            kfree_heap(path_copy);
            return -1;
        }

        token = strtok(path_copy, "/");
    }

    kfree_heap(path_copy);

    if (node_out) *node_out = current_node;
    if (handle_out) {
        vfs_handle_t *handle = kmalloc_heap(sizeof(vfs_handle_t));
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

    if (node && node->read) {
        return node->read(node, buf, len);
    }
    return -1;
}

ssize_t kernelfs_write(vfs_handle_t *f, const void *buf, size_t len) {
    kernelfs_node_t *node = (kernelfs_node_t *)f->func_args;
    
    if (node && node->write) {
        return node->write(node, buf, len);
    }
    return -1;
}

ssize_t kernelfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence) {
    kernelfs_node_t *node = (kernelfs_node_t *)f->func_args;

    if (node && node->seek) {
        return node->seek(node, offset, whence);
    }
    return -1;
}

vfs_handle_t *kernelfs_open(const char *path, const char *mode) {
    vfs_handle_t *handle = NULL;

    int res = kernelfs_find_node(path, NULL, &handle);

    return handle;
}

void kernelfs_close(vfs_handle_t *f) {
    kfree_heap(f);
}

int kernelfs_create_node(const char *path, const char *name, vfs_node_type_t type,
                          ssize_t (*read)(kernelfs_node_t *, void *, size_t),
                          ssize_t (*write)(kernelfs_node_t *, const void *, size_t),
                          ssize_t (*seek)(kernelfs_node_t *, ssize_t, vfs_seek_whence_t)) {
    kernelfs_node_t *new_node = kmalloc_heap(sizeof(kernelfs_node_t));

    printf("Creating kernelfs node %s at %s\n", name, path);

    memset(new_node, 0, sizeof(kernelfs_node_t));

    strncpy(new_node->name, name, 256);
    new_node->type = type;
    new_node->read = read;
    new_node->write = write;
    new_node->seek = seek;

    kernelfs_node_t *parent_node = NULL;

    int res = kernelfs_find_node(path, &parent_node, NULL);

    if (!parent_node) {
        kfree_heap(new_node);
        return -1;
    }

    if (parent_node->type != VFS_NODE_TYPE_DIR) {
        kfree_heap(new_node);
        return -2;
    }

    // Insert into the parent node's children
    if (!parent_node->children) {
        parent_node->children = new_node;
    } else {
        kernelfs_node_t *child = parent_node->children;
        while (child->next) {
            child = child->next;
        }
        child->next = new_node;
        new_node->prev = child;
    }

    new_node->parent = parent_node;

    return 0;
}

void kernelfs_init() {
    root_kernelfs_node = kmalloc_heap(sizeof(kernelfs_node_t));

    memset(root_kernelfs_node, 0, sizeof(kernelfs_node_t));

    root_kernelfs_node->type = VFS_NODE_TYPE_DIR;
}