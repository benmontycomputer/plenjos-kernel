#include "rbtree.h"

void rbtree_init(rbtree_t *tree, rbnode_t *NIL, int (*cmp)(const void *a, const void *b)) {
    memset(tree, 0, sizeof(rbtree_t));
    memset(NIL, 0, sizeof(rbnode_t));

    NIL->color      = BLACK;
    NIL->bstn.data  = NULL;
    NIL->bstn.left  = (bstnode_t *)NIL;
    NIL->bstn.right = (bstnode_t *)NIL;
    tree->NIL       = NIL;

    tree->bst.cmp  = cmp;
    tree->bst.root = (bstnode_t *)NIL;
    tree->bst.size = 0;
}

void rbtree_destroy(rbtree_t *tree, void (*free_data)(void *data), void (*free_node)(rbnode_t *node)) {
    // TODO
}

int rbtree_insert(rbtree_t *tree, rbnode_t *data) {
    
}