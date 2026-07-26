#pragma once

#include "bstree.h"
#include "kernel.h"

typedef enum rbcolor {
    BLACK,
    RED
} rbcolor_t;

typedef struct rbnode rbnode_t;

struct rbnode {
    BSTNODE_UNION(rbnode_t);
    rbcolor_t color;
};

typedef struct rbtree {
    BSTREE_UNION(rbnode_t);
} rbtree_t;

void rbtree_init(rbtree_t *tree, int (*cmp)(const rbnode_t *a, const rbnode_t *b));
void rbtree_destroy(rbtree_t *tree, void (*free_data)(void *data), void (*free_node)(rbnode_t *node));
int rbtree_insert(rbtree_t *tree, rbnode_t *data);
rbnode_t *rbtree_find(rbtree_t *tree, void *data);
rbnode_t *rbtree_find_min(rbtree_t *tree);
rbnode_t *rbtree_find_max(rbtree_t *tree);
int rbtree_remove(rbtree_t *tree, rbnode_t *remove);