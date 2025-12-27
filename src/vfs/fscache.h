#pragma once

#include "lib/lock.h"
#include "plenjos/limits.h"
#include "plenjos/stat.h"
#include "plenjos/types.h"
#include "proc/proc.h"
#include "vfs/fscache.h"
#include "vfs/kernelfs.h"
#include "vfs/vfs.h"

#include <stdatomic.h>
#include <stdint.h>

#define FSCACHE_FLAG_DIRTY       0x1
#define FSCACHE_FLAG_MOUNT_POINT 0x2

#define FSCACHE_INIT_NODES 4096

// This should be returned as positive, not negative
#define FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY 1

typedef uint16_t fscache_flags_t;

typedef struct fscache_node fscache_node_t;

// Don't pack?
// TODO: if we pack, ensure alignment is correct, especially for internal_data?
struct fscache_node {
    // This is at the beginning to make it easy to use memset to clear a node without unsetting type (allowing us to
    // keep it reserved)
    dirent_type_t type;

    char name[NAME_MAX + 1];

    // This is used to stop this from being freed while it is still in use. You must acquire read_lock (NOT write_lock)
    // before incrementing this.
    atomic_int ref_count;

    rw_lock_t rwlock;

    // This stops children from being modified while traversing the tree
    atomic_int tree_lock;

    fscache_flags_t flags;

    uid_t uid;
    gid_t gid;
    mode_t mode;
    off_t size;

    vfs_ops_block_t *fsops;

    _Atomic(fscache_node_t *) parent_node;
    _Atomic(fscache_node_t *) first_child;
    _Atomic(fscache_node_t *) prev_sibling;
    _Atomic(fscache_node_t *) next_sibling;

    uint64_t internal_data[4];
};

typedef struct fscache_block_header fscache_block_header_t;

// The nodes are stored in the memory addresses directly following this header
struct fscache_block_header {
    size_t node_count;
    fscache_block_header_t *prev;
    fscache_block_header_t *next;
} __attribute__((packed));

int fscache_request_node(const char *path, uid_t uid, fscache_node_t **out);

void _fscache_wait_for_node_readable(fscache_node_t *node);
void _fscache_release_node_readable(fscache_node_t *node);
void _fscache_wait_for_node_modifiable(fscache_node_t *node);
void _fscache_release_node_modifiable(fscache_node_t *node);

// This must be called after kernelfs_init()
int fscache_init();

fscache_node_t *fscache_allocate_node();

void fscache_node_populate(fscache_node_t *node, dirent_type_t type, fscache_flags_t flags, const char *name, uid_t uid, gid_t gid,
                           mode_t mode, off_t size, vfs_ops_block_t *fsops);