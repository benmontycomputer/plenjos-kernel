#include <stdint.h>

#include "vfs/kernelfs.h"
#include "vfs/kernelfs.h"

#include "memory/kmalloc.h"

#include "lib/string.h"

kernelfs_node_t *root_kernelfs_node = NULL;

vfs_handle_t *kernelfs_open(const char *path, const char *mode) {
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
            return NULL;
        }

        token = strtok(path_copy, "/");
    }
    
    if (current_node != NULL) {
        // Root directory
        vfs_handle_t *handle = kmalloc_heap(sizeof(vfs_handle_t));
        handle->type = current_node->type;
        handle->read = current_node->read;
        handle->write = current_node->write;
        handle->seek = current_node->seek;
        handle->close = NULL;

        kfree_heap(path_copy);
        return handle;
    }

    return NULL;
}

void kernelfs_close(vfs_handle_t *f) {
    kfree_heap(f);
}

void kernelfs_init() {
    root_kernelfs_node = kmalloc_heap(sizeof(kernelfs_node_t));

    memset(root_kernelfs_node, 0, sizeof(kernelfs_node_t));

    root_kernelfs_node->type = VFS_NODE_TYPE_DIR;
}