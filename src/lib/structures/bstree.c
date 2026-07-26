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

bstnode_t *bstree_find_closest_predecessor(bstree_t *tree, bstnode_t *comparable_node) {
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
            if (!curr->right) {
                // Preserve curr and return it
                break;
            }
            curr = curr->right;
        } else {
            break;
        }
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
static inline bstnode_t *_bstree_unlink_node(bstree_t *tree, bstnode_t *node) {
    bstnode_t *left  = node->left;
    bstnode_t *right = node->right;

    bstnode_t *new = NULL;

    if (right) {
        new = right;

        if (left) {
            // Replace with the in-order successor
            bstnode_t *newparent = NULL;
            while (new->left) {
                newparent = new;
                new       = new->left;
            }

            if (newparent) {
                newparent->left    = new->right;
                new->right->parent = newparent;
                new->right         = right;
                right->parent      = new;
            }

            new->left    = left;
            left->parent = new;
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

    node->left   = NULL;
    node->right  = NULL;
    node->parent = NULL;

    tree->size--;
    return new;
}

static bstnode_t *_bstree_remove_left(bstree_t *tree, bstnode_t *parent) {
    bstnode_t *out = parent->left;
    parent->left   = _bstree_unlink_node(tree, out);
    return out;
}

static bstnode_t *_bstree_remove_right(bstree_t *tree, bstnode_t *parent) {
    bstnode_t *out = parent->right;
    parent->right  = _bstree_unlink_node(tree, out);
    return out;
}

static bstnode_t *_bstree_remove_root(bstree_t *tree) {
    bstnode_t *out = tree->root;
    tree->root     = _bstree_unlink_node(tree, out);
    return out;
}

bstnode_t *bstree_remove_node(bstree_t *tree, bstnode_t *node) {
    _bstree_arg_validate(tree, node, __func__);

    bstnode_t *curr = tree->root;
    int cmp         = 0;

    if (!curr) {
        return NULL;
    }

    if (node == curr) {
        return _bstree_remove_root(tree);
    }

    while (curr) {
        cmp = tree->cmp(node, curr);

        if (cmp < 0) {
            if (curr->left == node) {
                curr = _bstree_remove_left(tree, curr);
                break;
            } else {
                curr = curr->left;
            }
        } else {
            if (curr->right == node) {
                curr = _bstree_remove_right(tree, curr);
                break;
            } else {
                curr = curr->right;
            }
        }
    }

    return curr;
}

bstnode_t *bstree_remove_first(bstree_t *tree) {
    _bstree_short_arg_validate(tree, __func__);

    bstnode_t *curr = tree->root;
    if (curr == NULL) {
        return NULL;
    }

    bstnode_t *res = NULL;

    if (curr->left) {
        while (curr->left->left) {
            curr = curr->left;
        }
        return _bstree_remove_left(tree, curr);
    } else {
        return _bstree_remove_root(tree);
    }
}

bstnode_t *bstree_remove_last(bstree_t *tree) {
    _bstree_short_arg_validate(tree, __func__);

    bstnode_t *curr = tree->root;
    if (curr == NULL) {
        return NULL;
    }

    bstnode_t *res = NULL;

    if (curr->right) {
        while (curr->right->right) {
            curr = curr->right;
        }
        bstnode_t *res = _bstree_remove_right(tree, curr);
        return res;
    } else {
        bstnode_t *res = _bstree_remove_root(tree);
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

    node->right       = new->left;
    new->left->parent = node;

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

    node->left         = new->right;
    new->right->parent = node;

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