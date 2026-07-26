#pragma once

#include "kernel.h"
#include "lib/lock.h"

/**
 * These trees don't handle allocating or freeing nodes; that is left to the caller.
 *
 * This tree replaces with the in-order successor by default when removing a node.
 */

typedef struct bstnode bstnode_t;

#define BSTNODE_UNION(newnodetype) \
    union {                        \
        bstnode_t _bstn;           \
        struct {                   \
            newnodetype *left;     \
            newnodetype *right;    \
            newnodetype *parent;   \
        };                         \
    }

#define BSTREE_UNION(newnodetype)                                   \
    union {                                                         \
        bstree_t _bst;                                              \
        struct {                                                    \
            newnodetype *root;                                      \
            int (*cmp)(const newnodetype *a, const newnodetype *b); \
            size_t size;                                            \
        };                                                          \
    }

/**
 * This is meant to be embedded in other structs using the BSTNODE_UNION macro for easier access
 */
struct bstnode {
    bstnode_t *left;
    bstnode_t *right;
    bstnode_t *parent;
};

typedef struct bstree {
    bstnode_t *root;

    int (*cmp)(const bstnode_t *a, const bstnode_t *b);

    size_t size;
} bstree_t;

bstnode_t *bstree_find_comparable(bstree_t *tree, bstnode_t *comparable_node);
bstnode_t *bstree_find_closest_predecessor(bstree_t *tree, bstnode_t *comparable_node);

int bstree_insert_node(bstree_t *tree, bstnode_t *node);

/**
 * Removes the exact given node, NOT a comparable node
 *
 * @param tree the tree to operate on
 * @param replaced_by_out (optional) output for the node that took the removed node's place
 * @param replacement_parent (optional) output for the parent of the physically removed node (which is either
 * in-order successor or the removed node itself
 * @return 0 if replacement was a left child; 1 if right; negative on error
 */
int bstree_remove_node(bstree_t *tree, bstnode_t *node, bstnode_t **replaced_by_out,
                              bstnode_t **replacement_parent);

/**
 * This function removes the LEFTMOST node in the given tree and returns the removed node.
 *
 * @param tree the tree to operate on
 * @param replaced_by_out (optional) output for the node that took the removed node's place
 * @param replacement_parent (optional) output for the parent of the physically removed node (which is either
 * in-order successor or the removed node itself
 * @return 0 if replacement was a left child; 1 if right; negative on error
 */
int bstree_remove_first(bstree_t *tree, bstnode_t **replaced_by_out, bstnode_t **replacement_parent);

/**
 * This function removes the RIGHTMOST node in the given tree and returns the removed node.
 *
 * @param tree the tree to operate on
 * @param replaced_by_out (optional) output for the node that took the removed node's place
 * @param replacement_parent (optional) output for the parent of the physically removed node (which is either
 * in-order successor or the removed node itself)
 * @return 0 if replacement was a left child; 1 if right; negative on error
 */
int bstree_remove_last(bstree_t *tree, bstnode_t **replaced_by_out, bstnode_t **replacement_parent);

/**
 * This function executes a LEFT rotation on the given subtree, updating the parent of the subtree (if any).
 *
 * @param tree the tree that the subtree is a part of (so root node can be updated if necessary)
 * @param node the root of the subtree to rotate
 */
void bstree_left_rotate(bstree_t *tree, bstnode_t *node);

/**
 * This function executes a RIGHT rotation on the given subtree, updating the parent of the subtree (if any).
 *
 * @param tree the tree that the subtree is a part of (so root node can be updated if necessary)
 * @param node the root of the subtree to rotate
 */
void bstree_right_rotate(bstree_t *tree, bstnode_t *node);