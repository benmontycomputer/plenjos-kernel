#pragma once

#include "kernel.h"
#include "lib/lock.h"
#include "stdint.h"

typedef struct slnode slnode_t;
typedef struct dlnode dlnode_t;

extern const slnode_t _slnode_DEL; // For hashmaps, etc.
extern const dlnode_t _dlnode_DEL; // For hashmaps, etc.

#define slnode_DEL &_slnode_DEL
#define dlnode_DEL &_dlnode_DEL

struct slnode {
    void *data;
    slnode_t *next;
};

struct dlnode {
    void *data;
    dlnode_t *prev;
    dlnode_t *next;
};

typedef struct ll_data {
    slnode_t *head;
    slnode_t *tail;
    ssize_t length;
    mutex lock;
} ll_data_t;

typedef struct list_ops_block list_ops_block_t;

typedef struct list {
    const list_ops_block_t *ops;
    char data[64];
} list_t;
_Static_assert(sizeof(ll_data_t) <= sizeof(((list_t *)0)->data), "ll_data_t too large!");

typedef int (*list_get_func)(list_t *list, ssize_t idx, void **out);
typedef int (*list_set_func)(list_t *list, ssize_t idx, void *new, void **out_old);
typedef int (*list_remove_func)(list_t *list, ssize_t idx, void **out);
typedef int (*list_insert_func)(list_t *list, ssize_t idx, void *new);
typedef int (*list_append_func)(list_t *list, void *new);
typedef int (*list_append_or_replace_func)(list_t *list, void *new, compare_func_t cmp, const void *cmp_data, void **out_old);
typedef int (*list_get_by_key_func)(list_t *list, const char *key, key_func_t key_func, void **out);
typedef ssize_t (*list_count_func)(list_t *list);

struct list_ops_block {
    list_get_func get;
    list_set_func set;
    list_remove_func remove;
    list_insert_func insert;
    list_append_func append;
    list_append_or_replace_func append_or_replace;
    list_get_by_key_func get_by_key;
    list_count_func count;
};

inline int list_get(list_t *list, ssize_t idx, void **out) {
    return list->ops->get(list, idx, out);
}

inline int list_set(list_t *list, ssize_t idx, void *new, void **out_old) {
    return list->ops->set(list, idx, new, out_old);
}

inline int list_remove(list_t *list, ssize_t idx, void **out) {
    return list->ops->remove(list, idx, out);
}

// Note: an index of -1 inserts before the last element, not after the last element
inline int list_insert(list_t *list, ssize_t idx, void *new) {
    return list->ops->insert(list, idx, new);
}

inline int list_append(list_t *list, void *new) {
    return list->ops->append(list, new);
}

inline int list_append_or_replace(list_t *list, void *new, compare_func_t cmp, const void *cmp_data, void **out_old) {
    return list->ops->append_or_replace(list, new, cmp, cmp_data, out_old);
}

inline int list_get_by_key(list_t *list, const char *key, key_func_t key_func, void **out) {
    return list->ops->get_by_key(list, key, key_func, out);
}

// Can't be negative; this is just an ssize_t to be consistent with indexing
inline ssize_t list_count(list_t *list) {
    return list->ops->count(list);
}

list_t *singly_linked_list_new(void);
list_t *doubly_linked_list_new(void);

slnode_t *slnode_alloc(void *data);
dlnode_t *dlnode_alloc(void *data);
void slnode_free(slnode_t *node);
void dlnode_free(dlnode_t *node);