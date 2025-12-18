#pragma once

#include "plenjos/limits.h"
#include "plenjos/syscall.h"

#include "proc/proc.h"
#include "vfs/fscache.h"
#include "vfs/vfs.h"

#include <stdint.h>

typedef struct kernelfs_node kernelfs_node_t;

typedef struct kernelfs_handle_instance_data {
    uint64_t pos;
    uint64_t unused[3];
} kernelfs_handle_instance_data_t;

typedef struct kernelfs_cache_data {
    kernelfs_node_t *node;
    uint64_t unused[3];
} kernelfs_cache_data_t;

typedef ssize_t (*kernelfs_open_func_t)(const char *, syscall_open_flags_t, fscache_node_t *);

// This represents nodes in the kernel filesystem. None of these nodes have fixed data; they all trigger calls to the
// kernel.
struct kernelfs_node {
    char name[NAME_MAX + 1];

    dirent_type_t type;

    uid_t uid;
    gid_t gid;
    mode_t mode; // rwxrwxrwx

    // This is only for block devices; it opens at the given path under the block device filesystem
    kernelfs_open_func_t open;
    vfs_read_func_t read;
    vfs_write_func_t write;
    vfs_seek_func_t seek;
    // vfs_close_func_t close;

    void *func_args;

    kernelfs_node_t *parent;
    kernelfs_node_t *children;
    kernelfs_node_t *prev;
    kernelfs_node_t *next;
};

kernelfs_node_t *kernelfs_get_node_from_handle(vfs_handle_t *handle);

void kernelfs_init();
// ssize_t kernelfs_open(const char *path, uint64_t flags, uint64_t mode, proc_t *proc, kernelfs_node_t **out);
int kernelfs_close(vfs_handle_t *f);
ssize_t kernelfs_create_child(fscache_node_t *parent, const char *name, dirent_type_t type, uid_t uid, gid_t gid,
                              mode_t mode, fscache_node_t *node);

int kernelfs_helper_mkdir(const char *path, uid_t uid, gid_t gid, mode_t mode);
int kernelfs_helper_create_file(const char *parent, const char *name, uint8_t type, uid_t uid, gid_t gid, mode_t mode,
                                vfs_read_func_t read, vfs_write_func_t write, vfs_seek_func_t seek, void *func_args);

// Internal use only
/* int kernelfs_create_node(kernelfs_node_t *parent_node, const char *name, uint8_t type, uid_t uid, gid_t gid,
                         mode_t mode, kernelfs_open_func_t open, vfs_read_func_t read,
                         vfs_write_func_t write, vfs_seek_func_t seek, void *func_args, kernelfs_node_t **out); */
/* int kernelfs_create_node(const char *path, const char *name, uint8_t type,
                         ssize_t (*open)(const char *, const char *),
                         ssize_t (*read)(kernelfs_node_t *, void *, size_t),
                         ssize_t (*write)(kernelfs_node_t *, const void *, size_t),
                         ssize_t (*seek)(kernelfs_node_t *, ssize_t, vfs_seek_whence_t), void *func_args); */