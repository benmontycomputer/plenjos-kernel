#pragma once

#include "kernel.h"

/**
 * These trees don't handle allocating or freeing nodes; that is left to the caller.
 *
 * By convention, nodes with the same value go to the right branch (left < parent; right >= parent). However, the equal
 * children are NOT guaranteed to be in a line; other nodes can come between.
 * 
 * This tree replaces with the in-order successor by default when removing a node.
 */

typedef struct bstnode {
    struct bstnode *left;
    struct bstnode *right;

    void *data;
} bstnode_t;

typedef struct bstree {
    bstnode_t *root;

    int (*cmp)(const void *a, const void *b);

    size_t size;
} bstree_t;

int bstree_insert_node(bstree_t *tree, bstnode_t *node);

// This removes the node with the same data POINTER; not just any node where cmp() returns 0.
bstnode_t *bstree_remove_data(bstree_t *tree, void *data);

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
 * This function executes a LEFT rotation on the given subtree and returns the new root of the subtree.
 *
 * @param node the root of the subtree to rotate
 * @return the new root of the subtree
 */
bstnode_t *bstree_left_rotate(bstnode_t *node);

/**
 * This function executes a RIGHT rotation on the given subtree and returns the new root of the subtree.
 *
 * @param node the root of the subtree to rotate
 * @return the new root of the subtree
 */
bstnode_t *bstree_right_rotate(bstnode_t *node);