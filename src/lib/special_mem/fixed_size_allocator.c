#include "fixed_size_allocator.h"

#include "lib/stdio.h"
#include "memory/mm.h"

#define FREE_IND_MASK                0xFFF
#define _KERNEL_CORRUPTION_CHECK_FSA 1 // Temporary

static fixed_size_allocator_t _fsa_root = FSA_DEFAULT(sizeof(fixed_size_allocator_t));

static inline void **_fsa_get_page_prev_slot(void *page) {
    return (void **)((uintptr_t)page + PAGE_LEN - sizeof(void *));
}

static inline void _fsa_set_page_free_ind(void *page, uint16_t free_ind) {
    void **page_prev = _fsa_get_page_prev_slot(page);
    uintptr_t prev   = (uintptr_t)*page_prev & ~FREE_IND_MASK;
    *page_prev       = (void *)(prev | ((uintptr_t)free_ind & FREE_IND_MASK));
}

static inline void _fsa_set_page_prev(void *page, void *prev) {
    void **page_prev   = _fsa_get_page_prev_slot(page);
    uintptr_t free_ind = (uintptr_t)*page_prev & FREE_IND_MASK;
    *page_prev         = (void *)(((uintptr_t)prev & ~FREE_IND_MASK) | free_ind);
}

static _fsa_free_slot_t *_fsa_setup_page(void *page, void *next_page_start, fixed_size_allocator_t *fsa) {
    if (!page || !fsa) {
        kout(KERNEL_SEVERE_FAULT, "_fsa_setup_page(): page or fsa is NULL!");
    }

    uintptr_t upper_bound = ((uintptr_t)page) + (fsa->items_per_page * fsa->item_size);
    void **prev           = NULL;
    for (uintptr_t i = (uintptr_t)page; i < upper_bound; i += fsa->item_size) {
        if (prev) {
            *prev = (void *)i;
        }

        prev = (void **)i;
    }
    if (prev) {
        *prev = (void *)next_page_start;
    }
    return (_fsa_free_slot_t *)prev;
}

static void _fsa_add_page(fixed_size_allocator_t *fsa) {
#ifdef _KERNEL_CORRUPTION_CHECK_FSA
    if (!fsa) {
        kout(KERNEL_SEVERE_FAULT, "_fsa_add_page(): fsa is NULL!");
    }
    if (((fsa->last_free == NULL) ^ (fsa->first_free == NULL)) || (fsa->last_free < fsa->first_free)) {
        kout(KERNEL_SEVERE_FAULT, "_fsa_add_page(): invalid state (last_free and first_free don't agree)");
    }
    if (((fsa->last_page == NULL) ^ (fsa->first_page == NULL)) || (fsa->last_page < fsa->first_page)) {
        kout(KERNEL_SEVERE_FAULT, "_fsa_add_page(): invalid state (last_page and first_page don't agree)");
    }
#endif

    uint64_t phys = find_next_free_frame();

    if (phys == NULL) {
        kout(KERNEL_WARN, "_fsa_add_page(): no free frames!");
        return;
    }

    void *page                  = (void *)phys_to_virt(phys);
    _fsa_free_slot_t *last_free = _fsa_setup_page(page, NULL, fsa);
    _fsa_set_page_free_ind(page, 0);
    _fsa_set_page_prev(page, fsa->last_page);

    if (fsa->last_free) {
        // There were free spots
        *(fsa->last_free) = (void *)page;
    } else {
        // There were no free spots
        fsa->first_free = (_fsa_free_slot_t *)page;
    }
    fsa->last_free = last_free;

    if (!fsa->last_page) {
        fsa->first_page = page;
    }
    fsa->last_page = page;
}

static inline uint16_t _fsa_index_in_page(void *page, void *block, fixed_size_allocator_t *fsa) {
#ifdef _KERNEL_CORRUPTION_CHECK_FSA
    if (!page) {
        kout(KERNEL_SEVERE_FAULT, "_fsa_index_in_page(): page is NULL!");
    }
    if (!fsa) {
        kout(KERNEL_SEVERE_FAULT, "__fsa_index_in_page(): fsa is NULL!");
    }
#endif

    uintptr_t page_end = (uintptr_t)page + (fsa->items_per_page * fsa->item_size);

    if (block == NULL || block < page || (uintptr_t)block >= page_end) {
        return (uint16_t)FREE_IND_MASK;
    }

    uintptr_t offs = (uintptr_t)block - (uintptr_t)page;
    uintptr_t res  = (offs / fsa->item_size);
    if (res >= fsa->items_per_page) {
        return (uint16_t)FREE_IND_MASK;
    }

    // This must come after the out-of-bounds checks; an out-of-bounds block should return the value for no free slots
    // in the page and should NOT error, but out-of-bounds values may cause issues with this offs alignment check.
#ifdef _KERNEL_CORRUPTION_CHECK_FSA
    if (offs % fsa->item_size) {
        kout(KERNEL_SEVERE_FAULT, "_fsa_index_in_page(): offset isn't aligned properly!");
    }
#endif
    return (uint16_t)res;
}

static inline void *_fsa_find_free_predecessor(fixed_size_allocator_t *fsa, void *page, uint16_t block_index) {
    for (void *cur_page = page; cur_page;
         cur_page       = (void *)((uintptr_t)*_fsa_get_page_prev_slot(cur_page) & ~FREE_IND_MASK)) {
        // This can be an int64_t because it is an index, not a pointer (and it must be signed)
        int64_t start = (cur_page == page) ? (int64_t)block_index - 1 : (int64_t)fsa->items_per_page - 1;

        for (int64_t i = start; i >= 0; --i) {
            void *candidate = (void *)((uintptr_t)cur_page + ((uintptr_t)i * fsa->item_size));

            for (void *cur = fsa->first_free; cur; cur = *(void **)cur) {
                if (cur == candidate) {
                    return candidate;
                }
            }
        }
    }

    return NULL;
}

void *fsa_alloc(fixed_size_allocator_t *fsa) {
    if (fsa == NULL) {
        kout(KERNEL_SEVERE_FAULT, "fsa_alloc(): fsa is NULL!");
    }

    mutex_lock(&fsa->mutex);

    if (!fsa->first_free) {
        _fsa_add_page(fsa);
        if (!fsa->first_free) {
            kout(KERNEL_WARN, "fsa_alloc(): _fsa_add_page() failed (OOM?)");
            return NULL;
        }
    }

    void *res       = fsa->first_free;
    fsa->first_free = *(void **)res;
    if (!fsa->first_free) {
        fsa->last_free = NULL;
#ifdef _KERNEL_CORRUPTION_CHECK_FSA
        if (fsa->last_free != res) {
            kout(KERNEL_SEVERE_FAULT, "fsa_alloc(): first_free says next free is NULL, but last_free != first_free!");
        }
#endif
    }

    void *page        = (void *)((uintptr_t)res & ~FREE_IND_MASK);
    void **page_prev  = _fsa_get_page_prev_slot(page);
    uint16_t free_ind = (uint16_t)((uintptr_t)*page_prev & FREE_IND_MASK);

#ifdef _KERNEL_CORRUPTION_CHECK_FSA
    if (_fsa_index_in_page(page, res, fsa) != free_ind) {
        kout(KERNEL_SEVERE_FAULT, "fsa_alloc(): free indices are inconsistent!");
    }
#endif

    if (fsa->first_free && ((uintptr_t)fsa->first_free & ~FREE_IND_MASK) == (uintptr_t)page) {
        _fsa_set_page_free_ind(page, _fsa_index_in_page(page, fsa->first_free, fsa));
    } else {
        _fsa_set_page_free_ind(page, FREE_IND_MASK);
    }

    mutex_unlock(&fsa->mutex);
    return res;
}

void *fsa_free(void *block, fixed_size_allocator_t *fsa) {
    if (!block || !fsa) {
        kout(KERNEL_SEVERE_FAULT, "fsa_free(): block or fsa is NULL!");
        return NULL;
    }

    mutex_lock(&fsa->mutex);

    // Get the page this block belongs to
    void *page = (void *)((uintptr_t)block & ~FREE_IND_MASK);

    // Get the index of this block in the page
    uint16_t block_index = _fsa_index_in_page(page, block, fsa);

    if (block_index == (uint16_t)FREE_IND_MASK) {
        // Block is not properly aligned in its page
        kout(KERNEL_SEVERE_FAULT, "fsa_free(): block is not properly located in its page!");
        mutex_unlock(&fsa->mutex);
        return NULL;
    }

#ifdef _KERNEL_CORRUPTION_CHECK_FSA
    if (!page) {
        kout(KERNEL_SEVERE_FAULT, "fsa_free(): page is NULL!");
        mutex_unlock(&fsa->mutex);
        return NULL;
    }
#endif

    void *prev_free = _fsa_find_free_predecessor(fsa, page, block_index);

    if (prev_free) {
        *(void **)block     = *(void **)prev_free;
        *(void **)prev_free = block;
        if (prev_free == (void *)fsa->last_free) {
            fsa->last_free = (_fsa_free_slot_t *)block;
        }
    } else {
        *(void **)block = (void *)fsa->first_free;
        fsa->first_free = (_fsa_free_slot_t *)block;

        if (!fsa->last_free) {
            fsa->last_free = (_fsa_free_slot_t *)block;
        }
    }

    // Update the page's free index to be the minimum of current and this block's index
    void **page_prev       = _fsa_get_page_prev_slot(page);
    uint16_t page_free_ind = (uint16_t)((uintptr_t)*page_prev & FREE_IND_MASK);
    if (page_free_ind == (uint16_t)FREE_IND_MASK || block_index < page_free_ind) {
        _fsa_set_page_free_ind(page, block_index);
    }

    mutex_unlock(&fsa->mutex);
    return NULL;
}

fixed_size_allocator_t *fixed_size_allocator_new(size_t item_size) {
    if (item_size > (PAGE_LEN - sizeof(void *))) {
        kout(KERNEL_SEVERE_FAULT, "fixed_size_allocator_new(): tried to allocate with item size %p (too big!)",
             item_size);
    }

    fixed_size_allocator_t *fsa = (fixed_size_allocator_t *)fsa_alloc(&_fsa_root);

    if (!fsa) {
        kout(KERNEL_WARN, "fixed_size_allocator_new(): failed to allocate!");
        return NULL;
    }

    memset(fsa, 0, sizeof(fixed_size_allocator_t));
    fsa->item_size      = item_size;
    fsa->items_per_page = ITEMS_PER_PAGE(item_size);
    fsa->mutex          = MUTEX_INIT;

    return fsa;
}