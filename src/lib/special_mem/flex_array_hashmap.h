#pragma once

#include "kernel.h"
#include "stdint.h"
#include "flex_array.h"
#include "fixed_size_allocator.h"

typedef struct flex_array_hashmap {
    size_t item_size;
    void *fa;
    size_t fa_size;
    size_t item_count;
    hash64_func_t hash_func;
    key_func_t key_func;
} flex_array_hashmap_t;

flex_array_hashmap_t *fah_new(size_t item_size, key_func_t key_func, hash64_func_t hash_func);

int fah_insert(flex_array_hashmap_t *fah, void *data, void **out_old);