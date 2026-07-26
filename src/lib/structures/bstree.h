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
    rw_lock_t lock;
};

typedef struct bstree {
    bstnode_t *root;
    rw_lock_t root_lock; // Entry point lock

    int (*cmp)(const bstnode_t *a, const bstnode_t *b);

    size_t size;
} bstree_t;

/**
 * These return read-locked nodes
 */
bstnode_t *bstree_find_comparable(bstree_t *tree, bstnode_t *comparable_node);
bstnode_t *bstree_find_closest_predecessor(bstree_t *tree, bstnode_t *comparable_node);

/**
 * Any locking on the inserted node is the caller's responsibilty (lock it before calling this function if you need
 * that)
 */
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
 * This function executes a LEFT rotation on the given subtree and returns the new root of the subtree. The node AND ITS
 * PARENT must be write-locked, and the write-lock is NOT released. If the node is the root node, the tree's root lock
 * must be write-locked.
 *
 * @param node the root of the subtree to rotate
 * @return the new root of the subtree
 */
bstnode_t *bstree_left_rotate(bstnode_t *node);

/**
 * This function executes a RIGHT rotation on the given subtree and returns the new root of the subtree. The node AND
 * ITS PARENT must be write-locked, and the write-lock is NOT released. If the node is the root node, the tree's root
 * lock must be write-locked.
 *
 * @param node the root of the subtree to rotate
 * @return the new root of the subtree
 */
bstnode_t *bstree_right_rotate(bstnode_t *node);