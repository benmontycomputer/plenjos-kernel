#include "rbtree.h"

#define color(node)    (node ? node->color : BLACK)
#define is_red(node)   (node && (node->color == RED)) // This also serves as a NULL check, as NULL nodes are BLACK
#define is_black(node) (!node || (node->color == BLACK))

void rbtree_init(rbtree_t *tree, int (*cmp)(const rbnode_t *a, const rbnode_t *b)) {
    memset(tree, 0, sizeof(rbtree_t));

    tree->cmp  = cmp;
    tree->root = NULL;
    tree->size = 0;
}

void rbtree_destroy(rbtree_t *tree, void (*free_data)(void *data), void (*free_node)(rbnode_t *node)) {
    // TODO
}

int rbtree_insert(rbtree_t *tree, rbnode_t *node) {
    int res = bstree_insert_node((bstree_t *)tree, (bstnode_t *)node);
    if (res) {
        return res;
    }

    node->color = RED;

    rbnode_t *parent;
    while ((parent = node->parent) && is_red(parent)) {
        rbnode_t *grandparent = parent->parent;
        if (parent == grandparent->left) {
            // Parent is left child
            rbnode_t *uncle = grandparent->right;

            if (is_red(uncle)) {
                // Case 1: make parent and uncle black and grandparent red (if grandparent is ROOT, it will get colored
                // back later)
                parent->color      = BLACK;
                uncle->color       = BLACK;
                grandparent->color = RED;

                node = grandparent;
                continue;
            }

            if (node == parent->right) {
                // Node is right child; case 2 (fix triangle)
                node = parent;
                bstree_left_rotate((bstree_t *)tree, (bstnode_t *)node);

                parent      = node->parent;
                grandparent = parent->parent;
            }

            // Case 3 (triggers even when case 2 triggers)
            parent->color      = BLACK;
            grandparent->color = RED;
            bstree_right_rotate((bstree_t *)tree, (bstnode_t *)grandparent);
        } else {
            // Parent is right child
            rbnode_t *uncle = grandparent->left;

            if (is_red(uncle)) {
                // Case 1: make parent and uncle black and grandparent red (if grandparent is ROOT, it will get colored
                // back later)
                parent->color      = BLACK;
                uncle->color       = BLACK;
                grandparent->color = RED;

                node = grandparent;
                continue;
            }

            if (node == parent->left) {
                // Node is left child; case 2 (fix triangle)
                node = parent;
                bstree_right_rotate((bstree_t *)tree, (bstnode_t *)node);

                parent      = node->parent;
                grandparent = parent->parent;
            }

            // Case 3 (triggers even when case 2 triggers)
            parent->color      = BLACK;
            grandparent->color = RED;
            bstree_left_rotate((bstree_t *)tree, (bstnode_t *)grandparent);
        }
    }
    tree->root->color = BLACK;

    return 0;
}

int rbtree_remove(rbtree_t *tree, rbnode_t *remove) {
    rbnode_t *node               = NULL;
    rbnode_t *replacement_parent = NULL;

    bool two_children = remove->left && remove->right;

    int res = bstree_remove_node((bstree_t *)tree, (bstnode_t *)remove, (bstnode_t **)&node,
                                                   (bstnode_t **)&replacement_parent);

    if (res < 0) {
        return res;
    }

    rbnode_t *parent = node ? node->parent : replacement_parent;

    if (two_children ? is_black(node) : is_black(remove)) {
        while (parent && is_black(node)) {
            if (node == NULL ? (res == 0) : (node == parent->left)) {
                rbnode_t *sibling = parent->right;

                if (!sibling) {
                    node   = parent;
                    parent = node->parent;
                    continue;
                }

                if (is_red(sibling)) {
                    // Case 1

                    sibling->color = BLACK;
                    parent->color  = RED;

                    bstree_left_rotate((bstree_t *)tree, (bstnode_t *)parent);

                    sibling = parent->right;
                }

                if (!sibling) {
                    node   = parent;
                    parent = node->parent;
                    continue;
                }

                if (is_black(sibling->left) && is_black(sibling->right)) {
                    // Case 2

                    sibling->color = RED;

                    node   = parent;
                    parent = node->parent;
                    continue;
                }

                if (is_black(sibling->right)) {
                    // Case 3

                    if (sibling->left) sibling->left->color = BLACK;
                    sibling->color = RED;

                    bstree_right_rotate((bstree_t *)tree, (bstnode_t *)sibling);

                    sibling = parent->right;
                }

                if (!sibling) {
                    node   = parent;
                    parent = node->parent;
                    continue;
                }

                // Case 4

                sibling->color = parent->color;
                parent->color  = BLACK;
                if (sibling->right) sibling->right->color = BLACK;

                bstree_left_rotate((bstree_t *)tree, (bstnode_t *)parent);

                break;
            } else {
                rbnode_t *sibling = parent->left;

                if (!sibling) {
                    node   = parent;
                    parent = node->parent;
                    continue;
                }

                if (is_red(sibling)) {
                    // Case 1

                    sibling->color = BLACK;
                    parent->color  = RED;

                    bstree_right_rotate((bstree_t *)tree, (bstnode_t *)parent);

                    sibling = parent->left;
                }

                if (!sibling) {
                    node   = parent;
                    parent = node->parent;
                    continue;
                }

                if (is_black(sibling->left) && is_black(sibling->right)) {
                    // Case 2

                    sibling->color = RED;

                    node   = parent;
                    parent = node->parent;
                    continue;
                }

                if (is_black(sibling->left)) {
                    // Case 3

                    if (sibling->right) sibling->right->color = BLACK;
                    sibling->color = RED;

                    bstree_left_rotate((bstree_t *)tree, (bstnode_t *)sibling);

                    sibling = parent->left;
                }

                if (!sibling) {
                    node   = parent;
                    parent = node->parent;
                    continue;
                }

                // Case 4

                sibling->color = parent->color;
                parent->color  = BLACK;
                if (sibling->left) sibling->left->color = BLACK;

                bstree_right_rotate((bstree_t *)tree, (bstnode_t *)parent);

                break;
            }
        }
    }

    if (node) node->color = BLACK;

    return 0;
}