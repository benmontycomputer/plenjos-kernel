#pragma once

// TODO: implement locks or something to keep track of what files are opened by who

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "kernel.h"

#include "plenjos/dirent.h"

typedef enum vfs_seek_whence {
    VFS_SEEK_SET = 0,
    VFS_SEEK_CUR = 1,
    VFS_SEEK_END = 2,
} vfs_seek_whence_t;

typedef struct vfs_handle vfs_handle_t;

struct vfs_handle {
    uint8_t type;
    uint8_t reserved[7];

    // This doubles as the getdents function for directories
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

void vfs_close(vfs_handle_t *f);

ssize_t vfs_read(vfs_handle_t *f, void *buf, size_t len);
ssize_t vfs_write(vfs_handle_t *f, const void *buf, size_t len);

ssize_t vfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence);