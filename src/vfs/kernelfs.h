#pragma once

#include "plenjos/limits.h"

#include "vfs/vfs.h"
#include "proc/proc.h"

#include <stdint.h>

typedef struct kernelfs_node kernelfs_node_t;

// This represents nodes in the kernel filesystem. None of these nodes have fixed data; they all trigger calls to the
// kernel.
struct kernelfs_node {
    char name[NAME_MAX + 1];

    dirent_type_t type;

    uid_t uid;
    gid_t gid;
    mode_t mode; // rwxrwxrwx

    // This is only for block devices; it opens at the given path under the block device filesystem
    ssize_t (*open)(const char *, const char *);

    ssize_t (*read)(struct kernelfs_node *, void *, size_t);
    ssize_t (*write)(struct kernelfs_node *, const void *, size_t);
    ssize_t (*seek)(struct kernelfs_node *, ssize_t, vfs_seek_whence_t);

    void *func_args;

    kernelfs_node_t *parent;
    kernelfs_node_t *children;
    kernelfs_node_t *prev;
    kernelfs_node_t *next;
};

void kernelfs_init();
vfs_handle_t *kernelfs_open(const char *path, uint64_t flags, uint64_t mode, proc_t *proc);
void kernelfs_close(vfs_handle_t *f);

// Internal use only
/* int kernelfs_create_node(const char *path, const char *name, uint8_t type,
                         ssize_t (*open)(const char *, const char *),
                         ssize_t (*read)(kernelfs_node_t *, void *, size_t),
                         ssize_t (*write)(kernelfs_node_t *, const void *, size_t),
                         ssize_t (*seek)(kernelfs_node_t *, ssize_t, vfs_seek_whence_t), void *func_args); */