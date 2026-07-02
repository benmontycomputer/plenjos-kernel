#pragma once

#include "stdint.h"
#include "kernel.h"

#include "memory/mm.h"

#include "lib/stdio.h"

#define FLEX_ARRAY_PAGE_CAPACITY(item_size) ((PAGE_LEN - sizeof(void *)) / (item_size))
#define FLEX_ARRAY_NEXT_PTR(page) ((void **)(PAGE_LEN + (uint64_t)(page) - sizeof(void *)))

#define ptr_flex_array_get_slot(arr, slot) _flex_array_get_slot((arr), (slot), FLEX_ARRAY_PAGE_CAPACITY(sizeof(void *)), sizeof(void *))
#define ptr_flex_array_alloc(n) _flex_array_alloc((n), sizeof(void *))

inline void *_flex_array_get_slot(void *arr, size_t slot, size_t page_capacity, size_t item_size) {
    size_t pg = slot / page_capacity;
    size_t offset = slot % page_capacity;

    while (pg) {
        arr = *FLEX_ARRAY_NEXT_PTR(arr);
        pg--;
    }

    return arr + (offset * item_size);
}

inline void *_flex_array_set(void *arr, size_t idx, size_t page_capacity, size_t item_size) {
    return *(void **)_flex_array_get_slot(arr, idx, page_capacity, item_size);
}

// The array is zeroed out
void *_flex_array_alloc(size_t n, size_t item_size);