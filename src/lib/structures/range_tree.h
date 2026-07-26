#pragma once

/**
 * This is a custom tree, vaguely similar to Linux's Maple Trees. It's specifically designed for the memory manager.
 */

#include "kernel.h"
#include "stddef.h"
#include "stdint.h"
#include "lib/lock.h"

#define RANGE_TREE_NODE_SEGMENTS 15

typedef struct range_item {
    ptr_t start;
    ptr_t end;
} range_item_t;

typedef struct range_node range_node_t;

/**
 * The architecture of a range_node:
 * | nodes[0] | nodes[1] | nodes[2] | ... | nodes[RANGE_TREE_NODE_SEGMENTS - 1] |
 *
 * Where each '|' represents splits[0], splits[1], etc.
 *
 * In other words, nodes[i] spans the area in the range [splits[i], splits[i + 1]).
 * NULL nodes represent free memory space.
 *
 * NOTE: these ranges are always page-aligned.
 */
struct range_node {
    rw_lock_t lock;
    size_t largest_gap; // Among this node and its children; bit 0 set means children are leaves
    ptr_t splits[RANGE_TREE_NODE_SEGMENTS + 1];
    void *children[RANGE_TREE_NODE_SEGMENTS]; // If leaf, these are data; otherwise, they're more range nodes.
};