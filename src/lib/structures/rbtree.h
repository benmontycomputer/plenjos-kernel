#pragma once

#include "bstree.h"
#include "kernel.h"

typedef enum rbcolor {
    BLACK,
    RED
} rbcolor_t;

typedef struct rbnode {
    bstnode_t bstn;
    rbcolor_t color;
} rbnode_t;

typedef struct rbtree {
    bstree_t bst;
    rbnode_t *NIL;
} rbtree_t;

void rbtree_init(rbtree_t *tree, rbnode_t *NIL, int (*cmp)(const void *a, const void *b));
void rbtree_destroy(rbtree_t *tree, void (*free_data)(void *data), void (*free_node)(rbnode_t *node));
int rbtree_insert(rbtree_t *tree, rbnode_t *data);
rbnode_t *rbtree_find(rbtree_t *tree, void *data);
rbnode_t *rbtree_find_min(rbtree_t *tree);
rbnode_t *rbtree_find_max(rbtree_t *tree);
rbnode_t *rbtree_remove(rbtree_t *tree, void *data);