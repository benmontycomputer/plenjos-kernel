#include "vfs/fscache.h"

#include "kernel.h" // for _KERNEL_FSCACHE_CHECK_LOCKS
#include "lib/mode.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "plenjos/errno.h"
#include "vfs/vfs.h"

static fixed_size_allocator_t _fscache_fsa = FSA_DEFAULT(sizeof(fscache_node_t));

/**
 * Explanation of memory-locking model of the fscache:
 * Each node has a rw_lock_t rwlock which is used to control access to the node's children (but not siblings). To modify
 * a node's siblings, acquire the parent's lock instead.
 *
 * When reading data from a node (e.g., reading its name, type, permissions, etc.), a thread must acquire a read lock on
 * the node's rwlock. This allows multiple threads to read the node's data concurrently.
 *
 * When modifying data on a node (e.g., changing its name, type, permissions, etc.), a thread must acquire a write lock
 * on BOTH the node's rwlock and the parent's rwlock. This ensures that no other threads can read or write to the node
 * while it is being modified. The parent's rwlock must be acquired to allow for traversal of the tree without having
 * to try to lock each child node individually.
 */

fscache_node_t *fscache_root_node = NULL;

void _fscache_wait_for_node_readable(fscache_node_t *node) {
    rw_lock_read_lock(&node->rwlock);
}

void _fscache_release_node_readable(fscache_node_t *node) {
    rw_lock_read_unlock(&node->rwlock);
}

void _fscache_wait_for_node_modifiable(fscache_node_t *node) {
    rw_lock_write_lock(&node->rwlock);
}

void _fscache_release_node_modifiable(fscache_node_t *node) {
    rw_lock_write_unlock(&node->rwlock);
}

// This isn't a true upgrade; see rw_lock_upgrade_read_to_write for details. However, it does use ref_count to prevent
// deletion.
void _fscache_upgrade_node_to_modifiable(fscache_node_t *node) {
    fscache_node_ref(node);
    rw_lock_upgrade_read_to_write(&node->rwlock);
    fscache_node_unref(node);
}

// This IS a true downgrade, unlike how the upgrade works; see rw_lock_downgrade_write_to_read for details
void _fscache_downgrade_node_to_readable(fscache_node_t *node) {
    rw_lock_downgrade_write_to_read(&node->rwlock);
}

// This never got finished and isn't referenced, so I'm pretty sure it's unnecessary. However, I don't remember what it
// was supposed to do, so I'm leaving it here in case I find later that it was actually needed.
/* bool _fscache_try_lock_node_for_eviction(fscache_node_t *node) {
    if (atomic_compare_exchange_strong(&node->ref_count, &(int) { 0 }, -1)) {
        // Now, we must try to
    }
} */

// TODO: deletion:
// use rw_lock_try_write_lock(); if successful, then try to write lock the parent; if successful, check that ref_count
// is 0, there are no children, and there IS a parent (specifically check for parent presence, not just whether this is
// root); if those are satisfied, then we can delete safely.
// Write locking the parent as well as the node is necessary
// to prevent deletion of a node that has pending read/write locks from e.g. traversal (the parent node MUST be locked
// before a node can be locked, except for root node). These locks must all be try's to prevent deadlock. Special case
// in traversal: if you have a lock on a node, its parent node is guaranteed to have the node as a child, so the parent
// is protected from deletion already, so you can lock a parent node of a locked node without locking the parent's
// parent.

// The parent node must be read-locked before calling this.
// No locks are modified in this function.
static fscache_node_t *_fscache_find_in_children(fscache_node_t *parent_node, char *token, size_t token_len) {
    if (!parent_node) return NULL;

#ifdef _KERNEL_FSCACHE_CHECK_LOCKS
    if (!rw_lock_is_some_locked(&parent_node->rwlock)) {
        kout(KERNEL_PANIC, "_fscache_find_in_children(): parent_node must already be locked!\n");
    }
#endif

    if (!parent_node->first_child) return NULL;

    fscache_node_t *current_node = atomic_load_explicit(&parent_node->first_child, memory_order_acquire);

    // We don't need to lock any children here; for any of them to be modified externally, the
    // parent node's lock would need to be acquired by that thread. However, we already hold the
    // parent node's lock, so no modifications can happen.
    while (current_node) {
        if (strncmp(current_node->name, token, token_len) == 0 && current_node->name[token_len] == '\0') {
            // Found the node
            break;
        }

        current_node = atomic_load_explicit(&current_node->next_sibling, memory_order_acquire);
    }

#ifdef _KERNEL_FSCACHE_CHECK_LOCKS
    if (!rw_lock_is_some_locked(&parent_node->rwlock)) {
        kout(KERNEL_PANIC, "_fscache_find_in_children(): parent_node became unlocked!\n");
    }
#endif

    return current_node;
}

// This assumes that the write lock is already held on the parent node
static void _fscache_link_node(fscache_node_t *parent, fscache_node_t *child) {
    if (!parent || !child) {
        return;
    }

    fscache_node_t *first_child = atomic_load_explicit(&parent->first_child, memory_order_acquire);

    if (first_child) {
        atomic_store_explicit(&child->next_sibling, first_child, memory_order_release);
        atomic_store_explicit(&first_child->prev_sibling, child, memory_order_release);
    }

    atomic_store_explicit(&parent->first_child, child, memory_order_release);
    atomic_store_explicit(&child->parent_node, parent, memory_order_release);
}

// We don't need to handle the scenario where a node is write-locked and its parent suddenly becomes write-locked.
// Why? if the original node is write-locked, the lock only applies to its children; any write-lock on the parent
// doesn't give that locker jurisdiction over the original node's children. Also, the original node is write-locked, so
// it is protected from eviction.

// TODO: implement links
// TODO: implement groups

// During traversal, we must ALWAYS hold a read or modify lock on either the current node, its parent, or both.
// This prevents external eviction while traversing.
// IMPORTANT: out is updated to be the last successfully found node (read-locked). The read-lock is released
// automatically if NULL is passed.
int fscache_request_node(const char *path, uid_t uid, fscache_node_t **out) {
    if (!path) {
        return -EINVAL;
    }

    if (path[0] != '/') {
        kout(KERNEL_PANIC, "fscache_request_node(): path[0] must be '/'!\n");
    }

    size_t path_len = strlen(path);
    if (path_len == 0) {
        return -EINVAL;
    }

    int res = 0;

    fscache_node_t *cur = fscache_root_node;
    if (!cur) {
        printf("fscache_request_node: fscache root node is NULL!\n");
        return -EIO;
    }

    size_t path_copy_len = path_len + 1;
    char *path_copy      = kmalloc_heap(path_copy_len);
    if (!path_copy) {
        return -ENOMEM;
    }

    strncpy(path_copy, path, path_copy_len);
    path_copy[path_copy_len - 1] = '\0';

    _fscache_wait_for_node_readable(cur);

    char *token = path_copy;
    while (*token != '\0') {
        // parent_node is unlocked here (it won't be used in between here and its next assignment)
        // current_node is read-locked here

        // Handle leading slashes
        while (*token == '/') token++;

        // Find next slash
        char *next_slash = token;
        while (*next_slash != '/' && *next_slash != '\0') next_slash++;
        size_t token_len = next_slash - token;

        // Preserve whatever was here originally so we know whether to stop, etc.
        // TODO: is this still necessary? (do we need to set it to '\0' in the first place? handling of "." and ".." can
        // easily be done without it)
        char next_slash_char = *next_slash;
        *next_slash          = '\0';

        // Empty token (e.g. trailing slash, etc.)
        if (token_len == 0) break;

        switch (cur->stat.mode & S_IFMT) {
        case S_IFDIR:
            // OK
            access_t mode = access_check(cur->stat.mode, cur->stat.uid, cur->stat.gid, uid);
            if (mode & ACCESS_EXECUTE) {
                // OK
            } else {
                printf("fscache_request_node: execute access denied on directory %s for uid %u; directory mode %B, "
                       "calculated effective mode %o\n",
                       cur->name, uid, (uint64_t)cur->stat.mode, (uint32_t)mode);
                res = -EACCES;
                goto res_set_and_return;
            }
            break;
        default:
            // Can't traverse through non-directories
            res = -ENOTDIR;
            goto res_set_and_return;
        }

        // Handle "." and ".." without ruining ".*" (!= "..") or "..*" names
        if (token[0] == '.') {
            if (token[1] == '.') {
                if (token[2] == '\0') {
                    // Parent directory
                    fscache_node_t *parent = atomic_load_explicit(&cur->parent_node, memory_order_acquire);
                    if (!parent) {
                        // No parent; we are at root
                        _fscache_release_node_readable(cur);
                        res = -ENOENT;
                        goto res_set_and_return;
                    }

                    // Acquire read lock on parent before releasing current node
                    _fscache_wait_for_node_readable(parent);
                    _fscache_release_node_readable(cur);
                    cur = parent;

                    *next_slash = next_slash_char;
                    token       = next_slash;
                    continue;
                }
            } else if (token[1] == '\0') {
                // Current directory; no-op
                *next_slash = next_slash_char;
                token       = next_slash;
                continue;
            }
        }

        // After this operation, current_node is read-locked if not NULL
        fscache_node_t *child = _fscache_find_in_children(cur, token, token_len);

        if (!child) {
            // Node not found; attempt to load it.
            _fscache_upgrade_node_to_modifiable(cur);
            kout(KERNEL_DEBUG_2, "fscache miss\n");

            // Because the upgrade isn't a true upgrade, we have to re-check for the child
            // After this operation, current_node is read-locked if not NULL
            child = _fscache_find_in_children(cur, token, token_len);

            if (!child) {
                // This node is read-locked if it is not NULL
                child = fscache_allocate_node();
                if (!child) {
                    res = -ENOMEM;
                    goto res_downgrade_and_set_and_return;
                }

                ssize_t load_res = -EIO;
                if (cur->fsops && cur->fsops->load_node) {
                    load_res = cur->fsops->load_node(cur, token, child);
                } else {
                    kout(KERNEL_WARN, "fscache_request_node(): ran into node with no load_node func (name %s, path %s, mode %o)\n", cur->name, path, cur->stat.mode);
                }

                if (load_res != 0) {
                    // mark the allocated node as free again
                    fsa_free(&_fscache_fsa, child);
                    // load_node indicated node doesn't exist
                    if (next_slash_char == '\0' && load_res == -ENOENT) {
                        res = FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY;
                        goto res_downgrade_and_set_and_return;
                    }

                    res = load_res;
                    goto res_downgrade_and_set_and_return;
                }

                _fscache_link_node(cur, child);
            } else {
                // We found an existing child during the search. Read-lock it now.
                _fscache_wait_for_node_readable(child);
            }

            // Child is already read-locked
            _fscache_release_node_modifiable(cur);
            cur = child;
        } else {
            // We found an existing child during the search. Read-lock it before releasing parent's lock.
            _fscache_wait_for_node_readable(child);
            _fscache_release_node_readable(cur);
            cur = child;
        }
        *next_slash = next_slash_char;
        token       = next_slash;
    }

// Put cur in out as-is
res_set_and_return:
    if (res == 0) {
        if (!S_ISDIR(cur->stat.mode)) {
            if (path[path_len - 1] == '/') {
                // Pathname ended with a slash, but the final node is not a directory
                res = -ENOTDIR;
            }
        }
    }
    if (out != NULL) {
        *out = cur;
    } else {
        _fscache_release_node_readable(cur);
    }
    kfree_heap(path_copy);
    return res;

// Downgrades cur to readable before going through standard set_and_return
res_downgrade_and_set_and_return:
    if (cur) {
        _fscache_downgrade_node_to_readable(cur);
    }
    goto res_set_and_return;
}

// Outputs a write-locked node; if out is NULL, no changes are made; just checks if the node would work for mounting and
// unlocks the node after.
int fscache_request_node_for_mounting(const char *path, uid_t uid, fscache_node_t **out) {
    if (path[0] != '/') {
        kout(KERNEL_PANIC, "fscache_request_node_for_mounting(): path[0] must be '/'!\n");
    }

    fscache_node_t *out_tmp = NULL;
    int res                 = fscache_request_node(path, uid, &out_tmp);
    if (res) {
        if (res == FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY) {
            res = -ENOENT;
        }
        if (out_tmp) {
            _fscache_release_node_readable(out_tmp);
        }
        return res;
    }
    if (!out_tmp) {
        kout(KERNEL_PANIC,
             "fscache_request_node_for_mounting(): fscache_request_node returned 0 but didn't set out_tmp!\n");
    }

    _fscache_upgrade_node_to_modifiable(out_tmp);

    if (!S_ISDIR(out_tmp->stat.mode)) {
        _fscache_release_node_modifiable(out_tmp);
        return -ENOTDIR;
    }

    if (out_tmp->ref_count || out_tmp->first_child) {
        _fscache_release_node_modifiable(out_tmp);
        return -EBUSY;
    }

    if (out) {
        *out = out_tmp;
        fscache_node_ref(out_tmp);
        out_tmp->flags |= FSCACHE_FLAG_MOUNT_POINT;
        out_tmp->fsops  = NULL;
        memset(out_tmp->internal_data, 0, sizeof(out_tmp->internal_data));
    } else {
        _fscache_release_node_modifiable(out_tmp);
    }

    return 0;
}

// TODO: limit how many nodes can be cached; evict old nodes if necessary
// Can't be evicted immediately, because parent is initially NULL plus it isn't linked anywhere in the tree
// This is guaranteed to return either NULL or a cleared, read-locked node with type set to DT_UNKNOWN
fscache_node_t *fscache_allocate_node() {
    fscache_node_t *node = fsa_alloc(&_fscache_fsa);
    if (node) {
        _fscache_wait_for_node_readable(node);
    }
    return node;
}

int fscache_node_ref(fscache_node_t *node) {
#ifdef _KERNEL_FSCACHE_CHECK_LOCKS
    if (!rw_lock_is_some_locked(&node->rwlock)) {
        kout(KERNEL_PANIC, "fscache_node_ref(): node must already be locked!\n");
    }
#endif

    int old = atomic_fetch_add_explicit(&node->ref_count, 1, memory_order_release);

#ifdef _KERNEL_FSCACHE_CHECK_LOCKS
    if (!rw_lock_is_some_locked(&node->rwlock)) {
        kout(KERNEL_PANIC, "fscache_node_ref(): node became unlocked!\n");
    }
#endif

    if (old < 0 || old == INT32_MAX) {
        kout(KERNEL_PANIC, "fscache_node_ref(): ref_count out of range");
        // atomic_fetch_sub_explicit(&node->ref_count, 1, memory_order_release);
        // return -EIO;
    }
    return old + 1;
}

int fscache_node_unref(fscache_node_t *node) {
    int old = atomic_fetch_sub_explicit(&node->ref_count, 1, memory_order_acq_rel);
    if (old <= 0) {
        kout(KERNEL_PANIC, "fscache_node_unref(): node not reffed!\n");
    }
    return old - 1;
}

ssize_t kernelfs_load(fscache_node_t *node, const char *name, fscache_node_t *out);

int fscache_init() {
    fscache_root_node = fscache_allocate_node();
    if (!fscache_root_node) {
        printf("fscache_init: failed to allocate root node!\n");
        return -1;
    }

    kernelfs_load(NULL, NULL, fscache_root_node);
    printf("fscache_init: loaded root node of mode %o\n", fscache_root_node->stat.mode);

    _fscache_release_node_readable(fscache_root_node);

    return 0;
}

void fscache_node_populate(fscache_node_t *node, fscache_flags_t flags, const char *name, struct kstat stat,
                           vfs_ops_block_t *fsops) {
    if (!node) return;

    node->flags = flags;
    strncpy(node->name, name, NAME_MAX);
    node->name[NAME_MAX] = '\0';
    node->stat           = stat;
    node->fsops          = fsops;
}