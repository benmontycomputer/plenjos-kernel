#pragma once

#include "fixed_size_allocator.h"
#include "flex_array.h"
#include "kernel.h"
#include "lib/structures/list.h"
#include "stdint.h"

// TODO: thread safety
typedef struct flex_array_hashmap {
    void *fa;
    size_t fa_size;
    size_t item_count;
    hash64_func_t key_hash_func;
    key_func_t key_func;
    list_t *keys_list;
} flex_array_hashmap_t;

flex_array_hashmap_t *fah_new(key_func_t key_func, hash64_func_t key_hash_func);

int fah_insert(flex_array_hashmap_t *fah, void *data, void **out_old);
void *fah_get(flex_array_hashmap_t *fah, const char *key);
int fah_each(flex_array_hashmap_t *fah, size_t start, ssize_t len, void (each_func)(void *item, void *func_args),
             void *func_args);