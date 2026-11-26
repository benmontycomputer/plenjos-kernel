#include "bstree.h"

#include "lib/stdio.h"

static void _bstree_arg_validate(bstree_t *tree, bstnode_t *node, const char *func) {
    if (!tree) {
        kout(KERNEL_PANIC, "%s(): tree is NULL!\n", func);
    }

    if (!node) {
        kout(KERNEL_PANIC, "%s(): node is NULL!\n", func);
    }
}

static void _bstree_short_arg_validate(bstree_t *tree, const char *func) {
    if (!tree) {
        kout(KERNEL_PANIC, "%s(): tree is NULL!\n", func);
    }
}

bstnode_t *bstree_find_comparable(bstree_t *tree, bstnode_t *comparable_node) {
    _bstree_arg_validate(tree, comparable_node, __func__);

    if (!tree->root) {
        return NULL;
    }

    bstnode_t *curr = tree->root;
    int cmp         = 0;

    while (curr) {
        cmp = tree->cmp(comparable_node, curr);

        if (cmp < 0) {
            curr = curr->left;
        } else if (cmp > 0) {
            curr = curr->right;
        } else {
            break;
        }
    }

    return curr;
}

bstnode_t *bstree_find_comparable_or_closest_predecessor(bstree_t *tree, bstnode_t *comparable_node) {
    _bstree_arg_validate(tree, comparable_node, __func__);

    if (!tree->root) {
        return NULL;
    }

    bstnode_t *curr = tree->root;
    bstnode_t *pred = NULL;

    while (curr) {
        int cmp = tree->cmp(comparable_node, curr);
        if (cmp < 0) {
            curr = curr->left;
        } else if (cmp > 0) {
            pred = curr;
            curr = curr->right;
        } else {
            return curr;
        }
    }
    return pred;
}

bstnode_t *bstree_find_min(bstree_t *tree) {
    _bstree_short_arg_validate(tree, __func__);

    if (!tree->root) {
        return NULL;
    }

    bstnode_t *curr = tree->root;

    while (curr->left) {
        curr = curr->left;
    }

    return curr;
}

bstnode_t *bstree_find_max(bstree_t *tree) {
    _bstree_short_arg_validate(tree, __func__);

    if (!tree->root) {
        return NULL;
    }

    bstnode_t *curr = tree->root;

    while (curr->right) {
        curr = curr->right;
    }

    return curr;
}

int bstree_insert_node(bstree_t *tree, bstnode_t *node) {
    _bstree_arg_validate(tree, node, __func__);

    bstnode_t *curr = tree->root;
    int cmp         = 0;

    if (!curr) {
        tree->root = node;
        tree->size++;
        return 0;
    }

    while (true) {
        cmp = tree->cmp(node, curr);

        if (cmp < 0) {
            if (curr->left) {
                curr = curr->left;
            } else {
                curr->left   = node;
                node->parent = curr;
                break;
            }
        } else {
            if (curr->right) {
                curr = curr->right;
            } else {
                curr->right  = node;
                node->parent = curr;
                break;
            }
        }
    }

    tree->size++;
    return 0;
}

/**
 * DO NOT USE THIS FUNCTION; use _bstree_remove_left(), _bstree_remove_right(), and _bstree_remove_root() instead
 */
static inline bstnode_t *_bstree_unlink_node(bstree_t *tree, bstnode_t *node, bstnode_t **replacement_parent) {
    bstnode_t *left  = node->left;
    bstnode_t *right = node->right;

    bstnode_t *new = NULL;

    bool set_parent = false;

    if (right) {
        new = right;

        if (left) {
            // Replace with the in-order successor
            bstnode_t *newparent = new;
            while (newparent->left) {
                newparent = newparent->left;
            }

            if (replacement_parent && newparent != right) {
                set_parent          = true;
                *replacement_parent = newparent->parent;
            }

            if (newparent != right) {
                newparent->parent->left = newparent->right;

                if (newparent->right) {
                    newparent->right->parent = newparent->parent;
                }

                newparent->right = right;
                right->parent    = newparent;
            }

            newparent->left = left;
            left->parent    = newparent;

            new = newparent;
        }
    } else {
        if (left) {
            new = left;
        } else {
            new = NULL;
        }
    }

    if (new) {
        new->parent = node->parent;
    }

    if (!set_parent && replacement_parent) {
        *replacement_parent = node->parent;
    }

    node->left   = NULL;
    node->right  = NULL;
    node->parent = NULL;

    tree->size--;
    return new;
}

static int _bstree_remove_left(bstree_t *tree, bstnode_t *parent, bstnode_t **replaced_by_out,
                               bstnode_t **replacement_parent) {
    bstnode_t *out = parent->left;
    parent->left   = _bstree_unlink_node(tree, out, replacement_parent);
    if (replaced_by_out) *replaced_by_out = parent->left;
    return 0;
}

static int _bstree_remove_right(bstree_t *tree, bstnode_t *parent, bstnode_t **replaced_by_out,
                                bstnode_t **replacement_parent) {
    bstnode_t *out = parent->right;
    parent->right  = _bstree_unlink_node(tree, out, replacement_parent);
    if (replaced_by_out) *replaced_by_out = parent->right;
    return 1;
}

static int _bstree_remove_root(bstree_t *tree, bstnode_t **replaced_by_out, bstnode_t **replacement_parent) {
    bstnode_t *out = tree->root;
    tree->root     = _bstree_unlink_node(tree, out, replacement_parent);
    if (replaced_by_out) *replaced_by_out = tree->root;
    return 0;
}

int bstree_remove_node(bstree_t *tree, bstnode_t *node, bstnode_t **replaced_by_out, bstnode_t **replacement_parent) {
    _bstree_arg_validate(tree, node, __func__);

    bstnode_t *curr = tree->root;
    int cmp         = 0;

    if (!curr) {
        return -1;
    }

    if (node == curr) {
        return _bstree_remove_root(tree, replaced_by_out, replacement_parent);
    }

    while (curr) {
        cmp = tree->cmp(node, curr);

        if (cmp < 0) {
            if (curr->left == node) {
                return _bstree_remove_left(tree, curr, replaced_by_out, replacement_parent);
            } else {
                curr = curr->left;
            }
        } else {
            if (curr->left == node) {
                // Still needed as duplicates can be on either side after rotations
                return _bstree_remove_left(tree, curr, replaced_by_out, replacement_parent);
            } else if (curr->right == node) {
                return _bstree_remove_right(tree, curr, replaced_by_out, replacement_parent);
            } else {
                curr = curr->right;
            }
        }
    }

    return -1;
}

int bstree_remove_first(bstree_t *tree, bstnode_t **replaced_by_out, bstnode_t **replacement_parent) {
    _bstree_short_arg_validate(tree, __func__);

    bstnode_t *curr = tree->root;
    if (curr == NULL) {
        return -1;
    }

    if (curr->left) {
        while (curr->left->left) {
            curr = curr->left;
        }
        return _bstree_remove_left(tree, curr, replaced_by_out, replacement_parent);
    } else {
        return _bstree_remove_root(tree, replaced_by_out, replacement_parent);
    }
}

int bstree_remove_last(bstree_t *tree, bstnode_t **replaced_by_out, bstnode_t **replacement_parent) {
    _bstree_short_arg_validate(tree, __func__);

    bstnode_t *curr = tree->root;
    if (curr == NULL) {
        return -1;
    }

    if (curr->right) {
        while (curr->right->right) {
            curr = curr->right;
        }
        return _bstree_remove_right(tree, curr, replaced_by_out, replacement_parent);
    } else {
        return _bstree_remove_root(tree, replaced_by_out, replacement_parent);
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

    node->right = new->left;

    if (new->left) new->left->parent = node;

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
    new->parent = node->parent;

    node->parent = new;
}

void bstree_right_rotate(bstree_t *tree, bstnode_t *node) {
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

    node->left = new->right;

    if (new->right) new->right->parent = node;

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
    new->parent = node->parent;

    node->parent = new;
}