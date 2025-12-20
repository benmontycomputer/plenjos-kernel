#include "vfs/fscache.h"

#include "lib/mode.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "plenjos/errno.h"
#include "vfs/vfs.h"

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

// This does not include permanent nodes (e.g. kernelfs nodes)
size_t fscache_cached_nodes_count = 0;

fscache_block_header_t *fscache_head = NULL;
fscache_block_header_t *fscache_tail = NULL;

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

// This isn't a true upgrade; see rw_lock_upgrade_read_to_write for details
void _fscache_upgrade_node_to_modifiable(fscache_node_t *node) {
    rw_lock_upgrade_read_to_write(&node->rwlock);
}

// This IS a true downgrade, unlike how the upgrade works; see rw_lock_downgrade_write_to_read for details
void _fscache_downgrade_node_to_readable(fscache_node_t *node) {
    rw_lock_downgrade_write_to_read(&node->rwlock);
}

bool _fscache_try_lock_node_for_eviction(fscache_node_t *node) {
    if (atomic_compare_exchange_strong(&node->ref_count, &(int) { 0 }, -1)) {
        // Now, we must try to
    }
}

// The parent node must be read-locked before calling this.
// No locks are modified in this function.
static fscache_node_t *_fscache_find_in_children(fscache_node_t *parent_node, char *token, size_t token_len) {
    if (!parent_node) return NULL;
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

        switch (cur->type) {
        case DT_DIR:
            // OK
            access_t mode = access_check(cur->mode, cur->uid, cur->gid, uid);
            if (mode & ACCESS_EXECUTE) {
                // OK
            } else {
                printf("fscache_request_node: execute access denied on directory %s for uid %u; directory mode %B, "
                       "calculated effective mode %B\n",
                       cur->name, uid, (uint64_t)cur->mode, (uint64_t)mode);
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

                ssize_t load_res = cur->fsops->load_node(cur, token, child);

                if (load_res != 0) {
                    atomic_store(&child->type, 0);
                    // load_node indicated node doesn't exist
                    if (next_slash_char == '\0' && load_res == -ENOENT) {
                        // We are at the end; node not found — caller may want parent
                        if (out) {
                            *out = cur;
                        }
                        // mark the allocated node as free again
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
        if (cur->type != DT_DIR) {
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

// This is guaranteed to return either NULL or a cleared node with type set to DT_UNKNOWN and with an initialized rwlock
fscache_node_t *find_next_available_node_in_block(fscache_block_header_t *block) {
    if (!block) {
        return NULL;
    }

    fscache_node_t *nodes = (fscache_node_t *)(block + 1);

    for (size_t i = 0; i < block->node_count; i++) {
        if (atomic_compare_exchange_strong(&nodes[i].type, &(uint8_t) { 0 }, DT_UNKNOWN)) {
            memset((uint8_t *)&nodes[i] + sizeof(dirent_type_t), 0, sizeof(fscache_node_t) - sizeof(dirent_type_t));
            rw_lock_init(&nodes[i].rwlock);
            return &nodes[i];
        }
    }

    return NULL;
}

fscache_block_header_t *fscache_create_block(size_t node_count) {
    fscache_block_header_t *new_block
        = (fscache_block_header_t *)kmalloc_heap(sizeof(fscache_block_header_t) + node_count * sizeof(fscache_node_t));

    if (!new_block) {
        return NULL;
    }

    memset(new_block, 0, sizeof(fscache_block_header_t) + node_count * sizeof(fscache_node_t));

    new_block->node_count = node_count;
    new_block->prev       = fscache_tail;
    new_block->next       = NULL;

    if (fscache_tail) {
        fscache_tail->next = new_block;
    }
    fscache_tail = new_block;

    if (!fscache_head) {
        fscache_head = new_block;
    }

    return new_block;
}

// TODO: limit how many nodes can be cached; evict old nodes if necessary
// TODO: ensure this node is properly read-locked so it can't be evicted immediately
// This is guaranteed to return either NULL or a cleared, read-locked node with type set to DT_UNKNOWN
fscache_node_t *fscache_allocate_node() {
    fscache_block_header_t *current_block = fscache_head;

    while (current_block) {
        fscache_node_t *node = find_next_available_node_in_block(current_block);
        if (node) {
            // Found a node
            _fscache_wait_for_node_readable(node);
            return node;
        }
        current_block = current_block->next;
    }

    // No available nodes; create a new block
    current_block = fscache_create_block(FSCACHE_INIT_NODES);
    if (!current_block) {
        return NULL;
    }

    fscache_node_t *node = find_next_available_node_in_block(current_block);
    if (node) {
        _fscache_wait_for_node_readable(node);
    }
    return node;
}

ssize_t kernelfs_load(fscache_node_t *node, const char *name, fscache_node_t *out);

int fscache_init() {
    fscache_cached_nodes_count = 0;

    fscache_create_block(FSCACHE_INIT_NODES);

    fscache_root_node = fscache_allocate_node();
    if (!fscache_root_node) {
        printf("fscache_init: failed to allocate root node!\n");
        return -1;
    }

    kernelfs_load(NULL, NULL, fscache_root_node);
    printf("fscache_init: loaded root node of type %u\n", fscache_root_node->type);

    _fscache_release_node_readable(fscache_root_node);

    return 0;
}