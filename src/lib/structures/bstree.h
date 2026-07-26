#pragma once

#include "kernel.h"
#include "lib/lock.h"

/**
 * These trees don't handle allocating or freeing nodes; that is left to the caller.
 *
 * By convention, nodes with the same value go to the right branch (left < parent; right >= parent). However, the equal
 * children are NOT guaranteed to be in a line; other nodes can come between.
 *
 * This tree replaces with the in-order successor by default when removing a node.
 */

typedef struct bstnode bstnode_t;

/**
 * This is meant to be embedded in other structs.
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
 */
bstnode_t *bstree_remove_node(bstree_t *tree, bstnode_t *node);

/**
 * This function removes the LEFTMOST node in the given tree and returns the removed node.
 *
 * @param tree the tree to operate on
 * @return the former leftmost node of the tree
 */
bstnode_t *bstree_remove_first(bstree_t *tree);

/**
 * This function removes the RIGHTMOST node in the given tree and returns the removed node.
 *
 * @param tree the tree to operate on
 * @return the former rightmost node of the tree
 */
bstnode_t *bstree_remove_last(bstree_t *tree);

/**
 * This function executes a LEFT rotation on the given subtree, updating the parent of the subtree (if any) and
 * returning the new root of the subtree.
 *
 * @param tree the tree that the subtree is a part of (so root node can be updated if necessary)
 * @param node the root of the subtree to rotate
 * @return the new root of the subtree
 */
bstree_t *bstree_left_rotate(bstree_t *tree, bstnode_t *node);

/**
 * This function executes a RIGHT rotation on the given subtree, updating the parent of the subtree (if any) and
 * returning the new root of the subtree.
 *
 * @param tree the tree that the subtree is a part of (so root node can be updated if necessary)
 * @param node the root of the subtree to rotate
 * @return the new root of the subtree
 */
bstnode_t *bstree_right_rotate(bstree_t *tree, bstnode_t *node);