#pragma once

#include "kernel.h"
#include "lib/lock.h"
#include "stdint.h"

#define ITEMS_PER_PAGE(item_size) ((PAGE_LEN - sizeof(void *)) / (item_size))

typedef struct fixed_size_allocator fixed_size_allocator_t;
#define FSA_DEFAULT(contents_item_size)                         \
    ((fixed_size_allocator_t) {                                 \
        .item_size      = (contents_item_size),                 \
        .items_per_page = ITEMS_PER_PAGE((contents_item_size)), \
        .first_page     = NULL,                                 \
        .last_page      = NULL,                                 \
        .first_free     = NULL,                                 \
        .last_free      = NULL,                                 \
        .mutex          = MUTEX_INIT,                           \
    })

typedef void *_fsa_free_slot_t;

// Note: item_size will be rounded up to sizeof(void *) if it is less than that
// Note: pages must be at least 4096 bytes, so the least-significant 12 bits of each page's stored previous
// address are reserved for the index within the page of the first free item in said page.
struct fixed_size_allocator {
    uint32_t item_size; // Must be <= PAGE_LEN - sizeof(void *); keep much lower than this for optimal performance
    uint32_t items_per_page;

    void *first_page;
    void *last_page;
    _fsa_free_slot_t *first_free;
    _fsa_free_slot_t *last_free;

    mutex mutex;
};

void *fsa_alloc(fixed_size_allocator_t *fsa);
void *fsa_free(void *block, fixed_size_allocator_t *fsa);