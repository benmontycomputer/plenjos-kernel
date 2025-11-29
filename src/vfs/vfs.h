#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "kernel.h"

typedef enum vfs_node_type {
    VFS_NODE_TYPE_FILE,
    VFS_NODE_TYPE_DIR,
    VFS_NODE_TYPE_DEV,
    VFS_NODE_TYPE_MOUNT,
} vfs_node_type_t;

typedef enum vfs_seek_whence {
    VFS_SEEK_SET,
    VFS_SEEK_CUR,
    VFS_SEEK_END,
} vfs_seek_whence_t;

typedef struct vfs_handle vfs_handle_t;

struct vfs_handle {
    vfs_node_type_t type : 8;
    uint8_t reserved[7];

    ssize_t (*read)(vfs_handle_t *, void *, size_t);
    ssize_t (*write)(vfs_handle_t *, const void *, size_t);
    ssize_t (*seek)(vfs_handle_t *, ssize_t, vfs_seek_whence_t);
    void (*close)(vfs_handle_t *);

    void *func_args;
};

typedef struct vfs_filesystem {
    const char *name;
    bool (*mount)(const char *device, vfs_handle_t *mount_point);
} vfs_filesystem_t;

void vfs_init();

vfs_handle_t *vfs_open(const char *path, const char *mode);
void vfs_close(vfs_handle_t *f);

ssize_t vfs_read(vfs_handle_t *f, void *buf, size_t len);
ssize_t vfs_write(vfs_handle_t *f, const void *buf, size_t len);

ssize_t vfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence);