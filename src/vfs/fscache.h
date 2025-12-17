#pragma once

#include "vfs/fscache.h"

#include "proc/proc.h"
#include "vfs/kernelfs.h"
#include "vfs/vfs.h"

#include "plenjos/limits.h"
#include "plenjos/stat.h"
#include "plenjos/types.h"

#include "lib/lock.h"

#include <stdatomic.h>
#include <stdint.h>

#define FSCACHE_FLAG_DIRTY 0x1
#define FSCACHE_FLAG_MOUNT_POINT 0x2

#define FSCACHE_INIT_NODES 4096

typedef uint16_t fscache_flags_t;

typedef struct fscache_node fscache_node_t;

struct fscache_node {
    // This is at the beginning to make it easy to use memset to clear a node without unsetting type (allowing us to keep it reserved)
    dirent_type_t type;

    char name[NAME_MAX + 1];

    // This is used to stop this from being freed while it is still in use.
    atomic_int ref_count;

    rw_lock_t rwlock;

    // This stops children from being modified while traversing the tree
    atomic_int tree_lock;

    fscache_flags_t flags;

    uid_t uid;
    gid_t gid;
    mode_t mode;

    vfs_ops_block_t *fsops;

    fscache_node_t *parent;
    fscache_node_t *children;
    fscache_node_t *prev;
    fscache_node_t *next;

    uint64_t internal_data[4];
} __attribute__((packed));

typedef struct fscache_block_header fscache_block_header_t;

// The nodes are stored in the memory addresses directly following this header
struct fscache_block_header {
    size_t node_count;
    fscache_block_header_t *prev;
    fscache_block_header_t *next;
} __attribute__((packed));