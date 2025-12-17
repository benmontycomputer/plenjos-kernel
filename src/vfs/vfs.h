#pragma once

// TODO: implement locks or something to keep track of what files are opened by who

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "kernel.h"

#include "plenjos/dirent.h"

typedef struct fscache_node fscache_node_t;

typedef ssize_t (*vfs_read_func_t)(vfs_handle_t *, void *, size_t);
typedef ssize_t (*vfs_write_func_t)(vfs_handle_t *, const void *, size_t);
typedef ssize_t (*vfs_seek_func_t)(vfs_handle_t *, ssize_t, vfs_seek_whence_t);
typedef void (*vfs_close_func_t)(vfs_handle_t *);
typedef ssize_t (*vfs_load_func_t)(fscache_node_t *node, const char *name, fscache_node_t *out);

typedef enum vfs_seek_whence {
    VFS_SEEK_SET = 0,
    VFS_SEEK_CUR = 1,
    VFS_SEEK_END = 2,
} vfs_seek_whence_t;

typedef struct vfs_handle vfs_handle_t;

struct vfs_handle {
    uint8_t type;

    uid_t uid;
    gid_t gid;
    mode_t mode;

    uint8_t reserved[5];

    // This doubles as the getdents function for directories
    vfs_read_func_t read;
    vfs_write_func_t write;
    vfs_seek_func_t seek;
    vfs_close_func_t close;

    // This is filesystem-specific
    uint64_t internal_data[4];
};

typedef struct vfs_ops_block vfs_ops_block_t;

struct vfs_ops_block {
    char *fsname;

    vfs_read_func_t read;
    vfs_write_func_t write;
    vfs_seek_func_t seek;
    vfs_close_func_t close;
    vfs_load_func_t load_node;
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