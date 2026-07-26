#include "flex_array.h"

void *_flex_array_alloc(size_t n, size_t item_size) {
    size_t cap = FLEX_ARRAY_PAGE_CAPACITY(item_size);
    size_t n_pg = (n + cap - 1) / cap; // n/cap, rounded up

    void *start = NULL;
    void *end = NULL;

    for (size_t i = 0; i < n_pg; i++) {
        uint64_t frame = find_next_free_frame();
        if (!frame) {
            while (start) {
                frame = virt_to_phys((uint64_t)start);
                start = *FLEX_ARRAY_NEXT_PTR(start);
                phys_mem_unref_frame(phys_addr_to_frame_struct(frame));
            }
            return NULL;
        }
        void *page = (void *)phys_to_virt(frame);
        memset(page, 0, PAGE_LEN);
        if (end) {
            *FLEX_ARRAY_NEXT_PTR(end) = (void *)page;
        } else {
            start = page;
        }
        end = page;
    }

    return start;
}