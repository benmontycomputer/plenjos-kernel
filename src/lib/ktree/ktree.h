#pragma once

/**
 * Contains everything necessary to create fully-featured hierarchical trees, including custom memory allocation.
 */

struct ktree {
    
}

struct knode {
    struct knode *parent;
    struct knode *prev;
    struct knode *next;
    struct knode *first_child;

    char data[];
}