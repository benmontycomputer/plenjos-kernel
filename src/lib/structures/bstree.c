#include "bstree.h"

#include "lib/stdio.h"

void _bstree_arg_validate(bstree_t *tree, bstnode_t *node, const char *func) {
    if (!tree) {
        kout(KERNEL_PANIC, "%s(): tree is NULL!\n", func);
    }

    if (!node) {
        kout(KERNEL_PANIC, "%s(): node is NULL!\n", func);
    }
}

void _bstree_short_arg_validate(bstree_t *tree, const char *func) {
    if (!tree) {
        kout(KERNEL_PANIC, "%s(): tree is NULL!\n", func);
    }
}

static inline void _bstree_read_lock(bstnode_t *node) {
    rw_lock_read_lock(&node->lock);
}

static inline void _bstree_read_unlock(bstnode_t *node) {
    rw_lock_read_unlock(&node->lock);
}

static inline void _bstree_write_lock(bstnode_t *node) {
    rw_lock_write_lock(&node->lock);
}

static inline void _bstree_write_unlock(bstnode_t *node) {
    rw_lock_write_unlock(&node->lock);
}

static inline bstnode_t *_bstree_transfer_read_lock(bstnode_t *old, bstnode_t *new) {
    _bstree_read_lock(new);
    _bstree_read_unlock(old);
    return new;
}

static inline bstnode_t *_bstree_transfer_write_lock(bstnode_t *old, bstnode_t *new) {
    _bstree_write_lock(new);
    _bstree_write_unlock(old);
    return new;
}

bstnode_t *bstree_find_comparable(bstree_t *tree, bstnode_t *comparable_node) {
    _bstree_arg_validate(tree, comparable_node, __func__);

    if (!tree->root) {
        return NULL;
    }

    rw_lock_read_lock(&tree->root_lock);

    bstnode_t *curr = tree->root;
    int cmp         = 0;

    rw_lock_read_lock(curr);
    rw_lock_read_unlock(&tree->root_lock);

    while (true) {
        cmp = tree->cmp(comparable_node, curr);

        if (cmp < 0) {
            if (!curr->left) {
                goto fail;
            }
            curr = _bstree_transfer_read_lock(curr, curr->left);
        } else if (cmp > 0) {
            if (!curr->right) {
                goto fail;
            }
            curr = _bstree_transfer_read_lock(curr, curr->right);
        } else {
            // Keep the read lock
            return curr;
        }
    }

fail:
    rw_lock_read_unlock(&curr->lock);
    return NULL;
}

bstnode_t *bstree_find_closest_predecessor(bstree_t *tree, bstnode_t *comparable_node) {
    _bstree_arg_validate(tree, comparable_node, __func__);

    if (!tree->root) {
        return NULL;
    }

    rw_lock_read_lock(&tree->root_lock);

    bstnode_t *curr = tree->root;
    int cmp         = 0;

    rw_lock_read_lock(curr);
    rw_lock_read_unlock(&tree->root_lock);

    while (true) {
        cmp = tree->cmp(comparable_node, curr);

        if (cmp < 0) {
            if (!curr->left) {
                goto fail;
            }
            curr = _bstree_transfer_read_lock(curr, curr->left);
        } else if (cmp > 0) {
            if (!curr->right) {
                // Keep the read lock
                return curr;
            }
            curr = _bstree_transfer_read_lock(curr, curr->right);
        } else {
            // Keep the read lock
            return curr;
        }
    }

fail:
    rw_lock_read_unlock(&curr->lock);
    return NULL;
}

int bstree_insert_node(bstree_t *tree, bstnode_t *node) {
    _bstree_arg_validate(tree, node, __func__);

    rw_lock_write_lock(&tree->root_lock);

    bstnode_t *curr = tree->root;
    int cmp         = 0;

    if (!curr) {
        tree->root = node;
        tree->size++;
        rw_lock_write_unlock(&tree->root_lock);
        return 0;
    }

    _bstree_write_lock(curr);
    rw_lock_write_unlock(&tree->root_lock);

    while (true) {
        cmp = tree->cmp(node, curr);

        if (cmp < 0) {
            if (curr->left) {
                curr = _bstree_transfer_write_lock(curr, curr->left);
                continue;
            } else {
                curr->left   = node;
                node->parent = curr;
                break;
            }
        } else {
            if (curr->right) {
                curr = _bstree_transfer_write_lock(curr, curr->right);
                continue;
            } else {
                curr->right  = node;
                node->parent = curr;
                break;
            }
        }
    }

    _bstree_write_unlock(curr);
    tree->size++;
    return 0;
}

/**
 * DO NOT USE THIS FUNCTION; use _bstree_remove_left(), _bstree_remove_right(), and _bstree_remove_root() instead
 */
static inline bstnode_t *_bstree_unlink_node(bstree_t *tree, bstnode_t *node, bool skip_lock) {
    if (!skip_lock)
        _bstree_write_lock(node); // I think a read lock might actually be sufficient if we didn't set left, right,
                                  // parent to NULL, but doesn't really matter.

    bstnode_t *left  = node->left;
    bstnode_t *right = node->right;

    bstnode_t *new = NULL;

    if (right) {
        new = right;
        _bstree_write_lock(new);

        if (left) {
            // Replace with the in-order successor
            // There are write locks further up the chain; we aren't slowing anything down by write locking here
            // Note: 2 nodes are being write locked at once here
            bstnode_t *newparent = NULL;
            while (new->left) {
                if (newparent && newparent != right) _bstree_write_unlock(newparent);
                newparent = new;
                new       = new->left;
                _bstree_write_lock(new);
            }

            if (newparent) {
                newparent->left    = new->right;
                new->right->parent = newparent;
                new->right         = right;
                right->parent      = new;

                if (newparent != right) {
                    _bstree_write_unlock(newparent);
                }
            }

            new->left    = left;
            left->parent = new;

            if (new != right) {
                _bstree_write_unlock(right);
            }
        }
    } else {
        if (left) {
            new = left;
            _bstree_write_lock(new);
        } else {
            new = NULL;
        }
    }

    if (new) {
        new->parent = node->parent;
        _bstree_write_unlock(new);
    }

    node->left   = NULL;
    node->right  = NULL;
    node->parent = NULL;
    if (!skip_lock) _bstree_write_unlock(node);

    tree->size--;
    return new;
}

/**
 * Parent must be write-locked
 */
static bstnode_t *_bstree_remove_left(bstree_t *tree, bstnode_t *parent) {
    bstnode_t *out = parent->left;
    parent->left   = _bstree_unlink_node(tree, out, false);
    return out;
}

/**
 * Parent must be write-locked
 */
static bstnode_t *_bstree_remove_right(bstree_t *tree, bstnode_t *parent) {
    bstnode_t *out = parent->right;
    parent->right  = _bstree_unlink_node(tree, out, false);
    return out;
}

/**
 * Tree's root lock must be write-locked, and tree's root node must also be write-locked (so functions that already
 * write-lock root can keep their lock).
 */
static bstnode_t *_bstree_remove_root_nolock(bstree_t *tree) {
    bstnode_t *out = tree->root;
    tree->root     = _bstree_unlink_node(tree, out, true);
    return out;
}

/**
 * Tree's root lock must be write locked, and tree's root node must NOT be write-locked.
 */
static bstnode_t *_bstree_remove_root(bstree_t *tree) {
    bstnode_t *out = tree->root;
    tree->root     = _bstree_unlink_node(tree, out, false);
    return out;
}

bstnode_t *bstree_remove_node(bstree_t *tree, bstnode_t *node) {
    _bstree_arg_validate(tree, node, __func__);

    rw_lock_write_lock(&tree->root_lock);

    bstnode_t *curr = tree->root;
    int cmp         = 0;

    if (!curr) {
        return NULL;
    }

    if (node == curr) {
        bstnode_t *res = _bstree_remove_root(tree);
        rw_lock_write_unlock(&tree->root_lock);
        return res;
    }

    _bstree_write_lock(curr);
    rw_lock_write_unlock(&tree->root_lock);

    bstnode_t *res = NULL;

    while (true) {
        cmp = tree->cmp(node, curr);

        if (cmp < 0) {
            if (!curr->left) {
                goto end;
            }
            if (curr->left == node) {
                res = _bstree_remove_left(tree, curr);
                goto end;
            } else {
                curr = _bstree_transfer_write_lock(curr, curr->left);
            }
        } else {
            if (!curr->right) {
                goto end;
            }
            if (curr->right == node) {
                res = _bstree_remove_right(tree, curr);
                goto end;
            } else {
                curr = _bstree_transfer_write_lock(curr, curr->right);
            }
        }
    }

end:
    _bstree_write_unlock(curr);
    return res;
}

bstnode_t *bstree_remove_first(bstree_t *tree) {
    _bstree_short_arg_validate(tree, __func__);

    rw_lock_write_lock(&tree->root_lock);

    bstnode_t *curr = tree->root;
    if (curr == NULL) {
        rw_lock_write_unlock(&tree->root_lock);
        return NULL;
    }

    bstnode_t *res = NULL;

    _bstree_write_lock(curr);

    if (curr->left) {
        rw_lock_write_unlock(&tree->root_lock);
        while (curr->left->left) {
            curr = _bstree_transfer_write_lock(curr, curr->left);
        }
        bstnode_t *res = _bstree_remove_left(tree, curr);
        _bstree_write_unlock(curr);
        return res;
    } else {
        bstnode_t *res = _bstree_remove_root_nolock(tree);
        _bstree_write_unlock(curr);
        rw_lock_write_unlock(&tree->root_lock);
        return res;
    }
}

bstnode_t *bstree_remove_last(bstree_t *tree) {
    _bstree_short_arg_validate(tree, __func__);

    rw_lock_write_lock(&tree->root_lock);

    bstnode_t *curr = tree->root;
    if (curr == NULL) {
        rw_lock_write_unlock(&tree->root_lock);
        return NULL;
    }

    bstnode_t *res = NULL;

    _bstree_write_lock(curr);

    if (curr->right) {
        rw_lock_write_unlock(&tree->root_lock);
        while (curr->right->right) {
            curr = _bstree_transfer_write_lock(curr, curr->right);
        }
        bstnode_t *res = _bstree_remove_right(tree, curr);
        _bstree_write_unlock(curr);
        return res;
    } else {
        bstnode_t *res = _bstree_remove_root_nolock(tree);
        _bstree_write_unlock(curr);
        rw_lock_write_unlock(&tree->root_lock);
        return res;
    }
}

void bstree_left_rotate(bstree_t *tree, bstnode_t *node) {
    /**
     *     X
     *    / \
     *   A   Y
     *      / \
     *     B   C
     *
     * Becomes:
     *
     *       Y
     *      / \
     *     X   C
     *    / \
     *   A   B
     */
    bstnode_t *new = node->right;
    if (new == NULL) {
        // Can't rotate
        // TODO: should we panic?
        return;
    }

    _bstree_write_lock(new);
    _bstree_write_lock(new->left);

    node->right       = new->left;
    new->left->parent = node;

    _bstree_write_unlock(new->left);

    new->left = node;

    if (node->parent) {
        if (node == node->parent->left) {
            node->parent->left = new;
        } else {
            node->parent->right = new;
        }
    } else {
        tree->root = new;
    }

    node->parent = new;
}

bstree_right_rotate(bstree_t *tree, bstnode_t *node) {
    /**
     *       X
     *      / \
     *     Y   A
     *    / \
     *   B   C
     *
     * Becomes:
     *
     *      Y
     *     / \
     *    B   X
     *       / \
     *      C   A
     */
    bstnode_t *new = node->left;
    if (new == NULL) {
        // Can't rotate
        // TODO: should we panic?
        return;
    }

    _bstree_write_lock(new);
    _bstree_write_lock(new->right);

    node->left       = new->right;
    new->right->parent = node;

    _bstree_write_unlock(new->right);

    new->right = node;

    if (node->parent) {
        if (node == node->parent->left) {
            node->parent->left = new;
        } else {
            node->parent->right = new;
        }
    } else {
        tree->root = new;
    }

    node->parent = new;
}