#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "kernel.h"

#include "lib/stdio.h"
#include "lib/string.h"

#include "memory/detect.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"

// TODO: make sure all sizes are less than uint64_t max?
// Shouldn't be a huge security issue since this is kernel memory allocation

uint64_t kheap_start, kheap_end;

typedef struct heap_segment_info heap_segment_info_t;

struct heap_segment_info {
    size_t size;

    heap_segment_info_t *next;
    heap_segment_info_t *prev;

    bool free;
};

heap_segment_info_t *kheap_last_segment = NULL;

static inline bool is_page_misaligned(uint64_t ptr) {
    return (ptr & (PAGE_LEN - 1)) != 0;
}

// Set to 16 for debug purposes
#define KHEAP_INIT_PAGES 160
#define KHEAP_INIT_SIZE (PAGE_LEN * KHEAP_INIT_PAGES)

#define HEAP_ALLOC_MIN 0x10

#define HEAP_HEADER_LEN sizeof(heap_segment_info_t)

// bool kheap_locked = false;
static atomic_bool kheap_locked_atomic = ATOMIC_VAR_INIT(false);

void lock_kheap() {
    /* for (;;) {
        if (!kheap_locked) {
            kheap_locked = true;
            return;
        }
    } */
    while (atomic_flag_test_and_set_explicit(&kheap_locked_atomic, __ATOMIC_ACQUIRE)) {
        // Wait
        __builtin_ia32_pause();
    }
}

void unlock_kheap() {
    // kheap_locked = false;
    atomic_flag_clear_explicit(&kheap_locked_atomic, __ATOMIC_RELEASE);
}

uint64_t *alloc_before_kheap() {
    uint64_t *frame = (uint64_t *)phys_to_virt(find_next_free_frame());

    memset(frame, 0, PAGE_LEN);

    return frame;
}

static heap_segment_info_t *kheap_add_segment(size_t len);

void init_kernel_heap() {
    uint64_t pos = KERNEL_HEAP_START_ADDR;

    for (size_t i = 0; i < 4; i++) {
        find_page_using_alloc(pos, true, alloc_before_kheap, 0, kernel_pml4);
        // find_page(pos, true, kernel_pml4);
        pos += PAGE_LEN;
    }

    kheap_start = KERNEL_HEAP_START_ADDR;
    kheap_end = KERNEL_HEAP_START_ADDR + (PAGE_LEN * 4);

    kheap_last_segment = (heap_segment_info_t *)kheap_start;

    kheap_last_segment->free = true;
    kheap_last_segment->next = NULL;
    kheap_last_segment->prev = NULL;
    kheap_last_segment->size = (PAGE_LEN * 4) - HEAP_HEADER_LEN;

    kheap_add_segment(KHEAP_INIT_SIZE - (PAGE_LEN * 4));
}

static heap_segment_info_t *kheap_add_segment(size_t len) {
    if (len < HEAP_ALLOC_MIN) len = HEAP_ALLOC_MIN;

    /* Calculate number of pages required (round up). */
    size_t pages = (len + HEAP_HEADER_LEN + PAGE_LEN - 1) / PAGE_LEN;

    /* Align the next mapping address up to a page boundary. */
    uint64_t pg_addr = kheap_end;
    if (pg_addr & (PAGE_LEN - 1)) {
        pg_addr = (pg_addr + PAGE_LEN - 1) & ~((uint64_t)(PAGE_LEN - 1));
    }

    /* Map the pages */
    for (size_t i = 0; i < pages; i++) {
        find_page(pg_addr + (i * PAGE_LEN), true, kernel_pml4);
    }

    heap_segment_info_t *new_segment = (heap_segment_info_t *)pg_addr;

    if (kheap_last_segment) kheap_last_segment->next = new_segment;

    new_segment->free = true;
    new_segment->prev = kheap_last_segment;
    new_segment->next = NULL;
    new_segment->size = len;

    kheap_last_segment = new_segment;

    kheap_end = pg_addr + (pages * PAGE_LEN);

    return new_segment;
}

static heap_segment_info_t *kheap_segment_split(heap_segment_info_t *segment, size_t keep_size) {
    if (!segment) return NULL;
    if (keep_size < HEAP_ALLOC_MIN) keep_size = HEAP_ALLOC_MIN;
    // This next line is needed because we use signed integers instead of unsigned
    if (keep_size + HEAP_HEADER_LEN >= segment->size) return NULL;
    if ((segment->size - keep_size - HEAP_HEADER_LEN) < HEAP_ALLOC_MIN) return NULL;

    heap_segment_info_t *new_segment = (heap_segment_info_t *)((uint64_t)segment + keep_size + HEAP_HEADER_LEN);

    new_segment->free = true;
    new_segment->next = segment->next;
    new_segment->prev = segment;
    new_segment->size = (segment->size - keep_size - HEAP_HEADER_LEN);

    segment->size = keep_size;

    if (segment->next) segment->next->prev = new_segment;

    segment->next = new_segment;

    if (kheap_last_segment == segment) kheap_last_segment = new_segment;

    return new_segment;
}

// TODO: is it safe to return this memory without clearing it?
void *kmalloc_heap(uint64_t size) {
    if (size == 0) return NULL;

    lock_kheap();

    heap_segment_info_t *cur_seg = (heap_segment_info_t *)kheap_start;

    for (;;) {
        if (cur_seg->free) {
            if (cur_seg->size > size) {
                // Split the segment
                kheap_segment_split(cur_seg, size);

                cur_seg->free = false;

                unlock_kheap();
                return (void *)((uint64_t)cur_seg + HEAP_HEADER_LEN);
            } else if (cur_seg->size == size) {
                cur_seg->free = false;

                unlock_kheap();
                return (void *)((uint64_t)cur_seg + HEAP_HEADER_LEN);
            }
        }

        if (!cur_seg->next) break;
        cur_seg = cur_seg->next;
    }

    heap_segment_info_t *seg = kheap_add_segment(size);

    seg->free = false;

    unlock_kheap();
    return (void *)((uint64_t)seg + HEAP_HEADER_LEN);

    // return kmalloc_heap(size);
}

void kfree_heap(void *ptr) {
    if (ptr == NULL) {
        printf("WARNING: tried to free a NULL ptr\n");
        return;
    }

    heap_segment_info_t *cur_seg = (heap_segment_info_t *)((uint64_t)ptr - HEAP_HEADER_LEN);

    lock_kheap();

    if (cur_seg->free) {
        printf("ERROR: double free or corruption\n");

        unlock_kheap();
        return;
    }

    cur_seg->free = true;

    unlock_kheap();
}

void *kmalloc(uint64_t size) {
    if (PHYS_MEM_HEAD + size >= PHYS_MEM_USEABLE_END) {
        printf("kmalloc: out of physical memory.\n");
        return NULL;
    }

    uint64_t ret = PHYS_MEM_HEAD;
    PHYS_MEM_HEAD += size;

    return (void *)phys_to_virt(ret);
}

// page aligned (4KiB)
void *kmalloc_a(uint64_t size, bool align) {
    if (align && is_page_misaligned(PHYS_MEM_HEAD)) {
        /* Align the physical memory head up to the next page. */
        PHYS_MEM_HEAD = (PHYS_MEM_HEAD + PAGE_LEN - 1) & ~((uint64_t)(PAGE_LEN - 1));
    }

    if (PHYS_MEM_HEAD + size >= PHYS_MEM_USEABLE_END) {
        printf("kmalloc_a: out of physical memory.\n");
        return NULL;
    }

    uint64_t ret = PHYS_MEM_HEAD;
    PHYS_MEM_HEAD += size;

    return (void *)phys_to_virt(ret);
}