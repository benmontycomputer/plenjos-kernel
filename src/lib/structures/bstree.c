#include "bstree.h"

#include "lib/stdio.h"

int bstree_insert_node(bstree_t *tree, bstnode_t *node) {
    bstnode_t *curr = tree->root;
    int cmp         = 0;

    while (true) {
        cmp = tree->cmp(node->data, curr->data);

        if (cmp < 0) {
            if (curr->left) {
                curr = curr->left;
                continue;
            } else {
                curr->left = node;
                // node->parent = curr;
                break;
            }
        } else {
            if (curr->right) {
                curr = curr->right;
                continue;
            } else {
                curr->right = node;
                // node->parent = curr;
            }
        }
    }

    tree->size++;
    return 0;
}

bstnode_t *bstree_remove_node(bstree_t *tree, bstnode_t *node) {
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
                newparent->left = new->right;
                new->right      = right;
            }

            new->left = left;
        }
    } else {
        if (left) {
            new = left;
        } else {
            new = NULL;
        }
    }

    tree->size--;
    return new;
}

bstnode_t *bstree_remove_exact_data(bstree_t *tree, void *data) {
    bstnode_t *curr   = tree->root;
    bstnode_t *parent = NULL;

    int cmp = 0;

    while (curr) {
        if (curr->data == data) {
            bstnode_t *new = bstree_remove_node(tree, curr);

            if (parent) {
                if (parent->left == curr) {
                    parent->left = new;
                } else {
                    parent->right = new;
                }
            } else {
                tree->root = new;
            }

            break;
        }

        cmp = tree->cmp(data, curr->data);

        parent = curr;

        if (cmp < 0) {
            curr = curr->left;
        } else {
            curr = curr->right;
        }
    }

    return curr;
}

bstnode_t *bstree_remove_first(bstree_t *tree) {
    if (tree == NULL) {
        return NULL;
    }

    if (tree->root == NULL) {
        return NULL;
    }

    bstnode_t *parent = NULL;
    bstnode_t *curr   = tree->root;

    while (curr->left) {
        parent = curr;
        curr   = curr->left;
    }

    if (parent) {
        parent->left = bstree_remove_node(tree, curr);
    } else {
        tree->root = bstree_remove_node(tree, curr);
    }

    return curr;
}

bstnode_t *bstree_remove_last(bstree_t *tree) {
    if (tree == NULL) {
        return NULL;
    }

    if (tree->root == NULL) {
        return NULL;
    }

    bstnode_t *parent = NULL;
    bstnode_t *curr   = tree->root;

    while (curr->right) {
        parent = curr;
        curr   = curr->right;
    }

    if (parent) {
        parent->right = bstree_remove_node(tree, curr);
    } else {
        tree->root = bstree_remove_node(tree, curr);
    }

    return curr;
}

bstnode_t *bstree_left_rotate(bstnode_t *node) {
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
        return node;
    }

    node->right = new->left;
    new->left = node;

    return new;
}

bstnode_t *bstree_right_rotate(bstnode_t *node) {
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
        return node;
    }

    node->left = new->right;
    new->right = node;

    return new;
}