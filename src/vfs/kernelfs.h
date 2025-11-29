#pragma once

#include "vfs/vfs.h"

#include <stdint.h>

typedef struct kernelfs_node kernelfs_node_t;

// This represents nodes in the kernel filesystem. None of these nodes have fixed data; they all trigger calls to the kernel.
struct kernelfs_node {
    char name[256];

    vfs_node_type_t type;

    ssize_t (*read)(struct kernelfs_node *, void *, size_t);
    ssize_t (*write)(struct kernelfs_node *, const void *, size_t);
    ssize_t (*seek)(struct kernelfs_node *, ssize_t, vfs_seek_whence_t);

    kernelfs_node_t *parent;
    kernelfs_node_t *children;
    kernelfs_node_t *prev;
    kernelfs_node_t *next;
};

void kernelfs_init();
vfs_handle_t *kernelfs_open(const char *path, const char *mode);
void kernelfs_close(vfs_handle_t *f);
int kernelfs_create_node(const char *path, const char *name, vfs_node_type_t type,
                          ssize_t (*read)(kernelfs_node_t *, void *, size_t),
                          ssize_t (*write)(kernelfs_node_t *, const void *, size_t),
                          ssize_t (*seek)(kernelfs_node_t *, ssize_t, vfs_seek_whence_t));