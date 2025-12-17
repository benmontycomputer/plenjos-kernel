#pragma once

// TODO: implement locks or something to keep track of what files are opened by who

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "kernel.h"

#include "lib/mode.h"

#include "plenjos/dirent.h"
#include "plenjos/syscall.h"

typedef struct fscache_node fscache_node_t;
typedef struct vfs_handle vfs_handle_t;
typedef struct vfs_ops_block vfs_ops_block_t;

typedef enum vfs_seek_whence {
    VFS_SEEK_SET = 0,
    VFS_SEEK_CUR = 1,
    VFS_SEEK_END = 2,
} vfs_seek_whence_t;

typedef ssize_t (*vfs_read_func_t)(vfs_handle_t *handle, void *buf, size_t len);
typedef ssize_t (*vfs_write_func_t)(vfs_handle_t *handle, const void *buf, size_t len);
typedef ssize_t (*vfs_seek_func_t)(vfs_handle_t *handle, ssize_t offset, vfs_seek_whence_t whence);
typedef int (*vfs_close_func_t)(vfs_handle_t *handle);
typedef ssize_t (*vfs_load_func_t)(fscache_node_t *node, const char *name, fscache_node_t *out);

// The access remains unchanged throughout the lifetime of the handle; it is only checked on file open.
struct vfs_handle {
    access_t access;
    fscache_node_t *backing_node;

    // This is filesystem-specific
    uint64_t internal_data[4];
};

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

int vfs_close(vfs_handle_t *f);

ssize_t vfs_read(vfs_handle_t *f, void *buf, size_t len);
ssize_t vfs_write(vfs_handle_t *f, const void *buf, size_t len);

ssize_t vfs_seek(vfs_handle_t *f, ssize_t offset, vfs_seek_whence_t whence);

ssize_t vfs_open(const char *path, syscall_open_flags_t flags, uid_t uid, vfs_handle_t **out);