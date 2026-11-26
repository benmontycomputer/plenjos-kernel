#pragma once

#include "lib/lock.h"
#include "plenjos/limits.h"
#include "plenjos/stat.h"
#include "plenjos/types.h"
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
    struct kstat stat; // TODO: atomics?

    char name[NAME_MAX + 1];

    // This is used to stop this from being freed while it is still in use. You must acquire read_lock (NOT write_lock)
    // before incrementing this. However, you do NOT need to acquire any locks at all to decrement.
    atomic_int ref_count;

    rw_lock_t rwlock;

    // This stops children from being modified while traversing the tree
    atomic_int tree_lock;

    fscache_flags_t flags;

    const vfs_ops_block_t *fsops;

    _Atomic(fscache_node_t *) parent_node;
    _Atomic(fscache_node_t *) first_child;
    _Atomic(fscache_node_t *) prev_sibling;
    _Atomic(fscache_node_t *) next_sibling;

    uint64_t internal_data[4];
};

int fscache_request_node(const char *path, uid_t uid, fscache_node_t **out);

// Outputs a write-locked node that is a valid mount point
int fscache_request_node_for_mounting(const char *path, uid_t uid, fscache_node_t **out);

void _fscache_wait_for_node_readable(fscache_node_t *node);
void _fscache_release_node_readable(fscache_node_t *node);
void _fscache_wait_for_node_modifiable(fscache_node_t *node);
void _fscache_release_node_modifiable(fscache_node_t *node);

// This isn't a true upgrade; see rw_lock_upgrade_read_to_write for details. However, it does use ref_count to prevent
// deletion.
void _fscache_upgrade_node_to_modifiable(fscache_node_t *node);
void _fscache_downgrade_node_to_readable(fscache_node_t *node);

int fscache_node_ref(fscache_node_t *node);
int fscache_node_unref(fscache_node_t *node);

// This must be called after kernelfs_init()
int fscache_init(void);

fscache_node_t *fscache_allocate_node(void);

void fscache_node_populate(fscache_node_t *node, fscache_flags_t flags, const char *name, struct kstat stat,
                           const vfs_ops_block_t *fsops);