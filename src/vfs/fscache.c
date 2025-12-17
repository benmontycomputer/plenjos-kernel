#include "vfs/fscache.h"
#include "vfs/vfs.h"

#include "memory/kmalloc.h"

#include "lib/string.h"

#include "plenjos/errno.h"

// This does not include permanent nodes (e.g. kernelfs nodes)
size_t fscache_cached_nodes_count = 0;

fscache_block_header_t *fscache_head = NULL;
fscache_block_header_t *fscache_tail = NULL;

fscache_node_t *fscache_root_node = NULL;

static void _fscache_wait_for_node_readable(fscache_node_t *node) {
    rw_lock_read_lock(&node->rwlock);
}

static void _fscache_release_node_readable(fscache_node_t *node) {
    rw_lock_read_unlock(&node->rwlock);
}

static void _fscache_wait_for_node_modifyable(fscache_node_t *node) {
    rw_lock_write_lock(&node->rwlock);
}

static void _fscache_release_node_modifyable(fscache_node_t *node) {
    rw_lock_write_unlock(&node->rwlock);
}

static bool _fscache_try_lock_node_for_delete(fscache_node_t *node) {
    if (atomic_compare_exchange_strong(&node->ref_count, &(int){ 0 }, -1)) {
        // Now, we must try to
    }
}

// Try to acquire a usage reference on a node. Returns true if the ref_count
// was incremented and the caller now holds a stable reference. Returns false
// if the node is being deleted (ref_count < 0) and cannot be acquired.
static bool fscache_try_acquire_node(fscache_node_t *node) {
    int old = atomic_load(&node->ref_count);
    while (old >= 0) {
        int desired = old + 1;
        if (atomic_compare_exchange_strong(&node->ref_count, &old, desired)) {
            return true;
        }
        // atomic_compare_exchange_strong updated `old` on failure; loop continues
    }
    return false;
}

static void fscache_release_node_ref(fscache_node_t *node) {
    atomic_fetch_sub(&node->ref_count, 1);
}

fscache_node_t *fscache_replace_node(fscache_node_t *old_node, fscache_node_t *new_node,
                                     fscache_node_t *new_node_parent) {
    if (old_node == NULL || new_node == NULL) { return NULL; }

    if (atomic_compare_exchange_strong(&old_node->ref_count, &(int){ 0 }, -1)) {
        /* Node marked for destruction; safe to replace. We don't need to check old_node->cache_ref_count; it must also
         * be 0. */

        // Do this just to be extra safe; this will set old_node->cache_ref_count to -1 if it isn't already
        _fscache_wait_for_node_modifyable(old_node);
        if (old_node->prev) {
            _fscache_wait_for_node_modifyable(old_node->prev);

            if (old_node->next) {
                _fscache_wait_for_node_modifyable(old_node->next);

                old_node->prev->next = old_node->next;
                old_node->next->prev = old_node->prev;

                _fscache_release_node_modifyable(old_node->next);
            } else {
                old_node->prev->next = NULL;
            }

            _fscache_release_node_modifyable(old_node->prev);
        } else if (old_node->next) {
            _fscache_wait_for_node_modifyable(old_node->next);

            old_node->next->prev = NULL;

            _fscache_release_node_modifyable(old_node->next);
        }

        if (old_node->parent && old_node->parent->children == old_node) {
            _fscache_wait_for_node_modifyable(old_node->parent);

            if (old_node->next) {
                old_node->parent->children = old_node->next;
                // We don't need to update old_node->next->parent; it still has the same parent
            } else {
                old_node->parent->children = NULL;
            }

            _fscache_release_node_modifyable(old_node->parent);
        }

        if (new_node_parent) {
            _fscache_wait_for_node_modifyable(new_node_parent);

            new_node->parent = new_node_parent;
            new_node->next = new_node_parent->children;
            if (new_node->next) {
                _fscache_wait_for_node_modifyable(new_node->next);

                new_node->next->prev = new_node;

                _fscache_release_node_modifyable(new_node->next);
            }
            new_node_parent->children = new_node;

            _fscache_release_node_modifyable(new_node_parent);
        }

        return new_node;
    }

    return NULL;
}

// The parent node must be read-locked before calling this.
// The resulting node is read-locked.
static fscache_node_t *_fscache_find_in_children(fscache_node_t *parent_node, char *token, size_t token_len) {
    if (!parent_node) return NULL;
    if (!parent_node->children) return NULL;

    _fscache_wait_for_node_readable(parent_node->children);
    fscache_node_t *current_node = parent_node->children;

    while (current_node) {
        fscache_node_t *tmp_node = current_node;
        if (strncmp(current_node->name, token, token_len) == 0 && current_node->name[token_len] == '\0') {
            // Found the node
            break;
        }

        // Lock the node for the next iteration
        if (current_node->next) { _fscache_wait_for_node_readable(current_node->next); }

        current_node = current_node->next;

        // Release this iteration's node
        _fscache_release_node_readable(tmp_node);
    }

    return current_node;
}

// This should be returned as positive, not negative
#define FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY 1

int fscache_request_node(const char *path, fscache_node_t **out) {
    if (!path) { return -EINVAL; }

    int res = 0;

    fscache_node_t *parent_node = NULL;
    fscache_node_t *current_node = fscache_root_node;

    size_t path_copy_len = strlen(path) + 1;
    char *path_copy = kmalloc_heap(path_copy_len);
    if (!path_copy) { return -ENOMEM; }

    strncpy(path_copy, path, path_copy_len);

    _fscache_wait_for_node_readable(current_node);

    char *token = path_copy;
    while (*token != '\0') {
        // parent_node is unlocked here (it won't be used in between here and its next assignment)
        // current_node is read-locked here

        // Handle leading slashes
        while (*token == '/')
            token++;

        // Find next slash
        char *next_slash = token;
        while (*next_slash != '/' && *next_slash != '\0')
            next_slash++;
        size_t token_len = next_slash - token;

        // Preserve whatever was here originally so we know whether to stop, etc.
        char next_slash_char = *next_slash;
        *next_slash = '\0';

        // Empty token (e.g. trailing slash, etc.)
        if (token_len == 0) break;

        // Search for the token in the current node's children
        parent_node = current_node;

        // After this operation, current_node is read-locked if not NULL
        current_node = _fscache_find_in_children(parent_node, token, token_len);

        if (!current_node) {
            // Node not found; attempt to load it.
            _fscache_upgrade_node_to_modifyable(parent_node);

            // TODO: do we actually need to check this again? (circle back after finishing locks)
            // After this operation, current_node is read-locked if not NULL
            current_node = _fscache_find_in_children(parent_node, token, token_len);

            if (!current_node) {
                fscache_node_t *new_node = fscache_allocate_node();
                if (!new_node) {
                    res = -ENOMEM;
                    goto res_finish;
                }

                // Call load while holding the parent's read lock (we only read parent's metadata in load)
                ssize_t load_res = parent_node->fsops->load_node(parent_node, token, new_node);

                if (load_res != 0) {
                    atomic_store(&new_node->type, 0);
                    // load_node indicated node doesn't exist
                    if (next_slash_char == '\0' && load_res == -ENOENT) {
                        // We are at the end; node not found — caller may want parent
                        if (out) { *out = parent_node; }
                        // mark the allocated node as free again
                        res = FSCACHE_REQUEST_NODE_ONE_LEVEL_AWAY;
                        goto res_finish;
                    }

                    res = load_res;
                    goto res_finish;
                }

                // Prepare new_node fields (not yet linked)
                new_node->parent = parent_node;
                new_node->prev = NULL;
                new_node->next = NULL;

                // Acquire a read-lock on this node before inserting it
                _fscache_wait_for_node_readable(new_node);

                // Insert at head of parent's children list
                new_node->next = parent_node->children;
                if (new_node->next) {
                    _fscache_wait_for_node_modifyable(new_node->next);

                    new_node->next->prev = new_node;

                    _fscache_release_node_modifyable(new_node->next);
                }
                parent_node->children = new_node;

                // current_node is read-locked after this
                current_node = new_node;
            }

            _fscache_release_node_modifyable(parent_node);
        } else {
            // We found an existing child during the search. Release the parent's read lock now and continue.
            _fscache_release_node_readable(parent_node);
        }
        *next_slash = next_slash_char;
        token = next_slash;
    }

    if (current_node) { _fscache_release_node_readable(current_node); }
    if (out != NULL) { *out = current_node; }

    res_finish:
    kfree_heap(path_copy);
    return res;
}

// This is guaranteed to return either NULL or a cleared node with type set to DT_UNKNOWN and with an initialized rwlock
fscache_node_t *find_next_available_node_in_block(fscache_block_header_t *block) {
    if (!block) { return NULL; }

    fscache_node_t *nodes = (fscache_node_t *)(block + 1);

    for (size_t i = 0; i < block->node_count; i++) {
        if (atomic_compare_exchange_strong(&nodes[i].type, &(uint8_t){ 0 }, DT_UNKNOWN)) {
            memset((uint8_t *)&nodes[i] + sizeof(dirent_type_t), 0, sizeof(fscache_node_t) - sizeof(dirent_type_t));
            rw_lock_init(&nodes[i].rwlock);
            return &nodes[i];
        }
    }

    return NULL;
}

fscache_block_header_t *fscache_create_block(size_t node_count) {
    fscache_block_header_t *new_block =
        (fscache_block_header_t *)kmalloc_heap(sizeof(fscache_block_header_t) + node_count * sizeof(fscache_node_t));

    if (!new_block) { return NULL; }

    memset(new_block, 0, sizeof(fscache_block_header_t) + node_count * sizeof(fscache_node_t));

    new_block->node_count = node_count;
    new_block->prev = fscache_tail;
    new_block->next = NULL;

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
// This is guaranteed to return either NULL or a cleared node with type set to DT_UNKNOWN
fscache_node_t *fscache_allocate_node() {
    fscache_block_header_t *current_block = fscache_head;

    while (current_block) {
        fscache_node_t *node = find_next_available_node_in_block(current_block);
        if (node) {
            // Found a node
            return node;
        }
        current_block = current_block->next;
    }

    // No available nodes; create a new block
    current_block = fscache_create_block(FSCACHE_INIT_NODES);
    if (!current_block) { return NULL; }

    return find_next_available_node_in_block(current_block);
}

void fscache_init() {
    fscache_cached_nodes_count = 0;

    fscache_create_block(FSCACHE_INIT_NODES);
}