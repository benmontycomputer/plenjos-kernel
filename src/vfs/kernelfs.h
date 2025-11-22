#pragma once

#include "vfs/vfs.h"

#include <stdint.h>

// This represents nodes in the kernel filesystem. None of these nodes have fixed data; they all trigger calls to the kernel.
typedef struct kernelfs_node {
    char name[256];

    vfs_node_type_t type;

    size_t (*read)(struct kernelfs_node *, void *, size_t);
    size_t (*write)(struct kernelfs_node *, const void *, size_t);
    ssize_t (*seek)(struct kernelfs_node *, ssize_t, vfs_seek_whence_t);

    kernelfs_node_t *parent;
    kernelfs_node_t *children;
    kernelfs_node_t *prev;
    kernelfs_node_t *next;
} kernelfs_node_t;