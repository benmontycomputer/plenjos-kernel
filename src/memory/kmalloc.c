#include <stdint.h>

#include "kernel.h"

#include "lib/stdio.h"

#include "memory/detect.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"

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
    return (bool)(ptr & 0xFFF);
}

// Set to 16 for debug purposes
#define KHEAP_INIT_PAGES 160
#define KHEAP_INIT_SIZE (PAGE_LEN * KHEAP_INIT_PAGES)

#define HEAP_ALLOC_MIN 0x10

#define HEAP_HEADER_LEN sizeof(heap_segment_info_t)

bool kheap_locked = false;

void lock_kheap() {
    for (;;) {
        if (!kheap_locked) {
            kheap_locked = true;
            return;
        }
    }
}

void unlock_kheap() {
    kheap_locked = false;
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
        find_page_using_alloc(pos, true, alloc_before_kheap, kernel_pml4);
        // find_page(pos, true, kernel_pml4);
        pos += PAGE_LEN;
    }

    kheap_start = KERNEL_HEAP_START_ADDR;
    kheap_end = KERNEL_HEAP_START_ADDR + PAGE_LEN;

    kheap_last_segment = (heap_segment_info_t *)kheap_start;

    kheap_last_segment->free = true;
    kheap_last_segment->next = NULL;
    kheap_last_segment->prev = NULL;
    kheap_last_segment->size = PAGE_LEN - HEAP_HEADER_LEN;

    kheap_add_segment(KHEAP_INIT_SIZE - (PAGE_LEN * 4));
}

static heap_segment_info_t *heap_add_segment(size_t len) {
    if (len < HEAP_ALLOC_MIN) len = HEAP_ALLOC_MIN;

    size_t pages = len + HEAP_HEADER_LEN;

    // Page align len, rounding up
    if (pages % PAGE_LEN) { pages += PAGE_LEN; }

    pages = pages / PAGE_LEN;

    uint64_t pg_addr = kheap_end & ~(uint64_t)0xFFF;

    // Map the pages
    for (size_t i = 0; i <= pages; i++) {
        find_page(pg_addr + (i * PAGE_LEN), true, kernel_pml4);
    }

    heap_segment_info_t *new_segment = (heap_segment_info_t *)kheap_end;

    kheap_last_segment->next = new_segment;

    new_segment->free = true;
    new_segment->prev = kheap_last_segment;
    new_segment->next = NULL;
    new_segment->size = len;

    kheap_last_segment = new_segment;

    kheap_end += (len + HEAP_HEADER_LEN);

    return new_segment;
}

static heap_segment_info_t *kheap_add_segment(size_t len) {
    heap_add_segment(len, kheap_start, kheap_end, kheap_last_segment);
}

static heap_segment_info_t *uheap_add_segment(size_t len) {
    heap_add_segment(len, uheap_start, uheap_end, uheap_last_segment);
}

static heap_segment_info_t *kheap_segment_split(heap_segment_info_t *segment, size_t keep_size) {
    if (!segment) return NULL;
    if (keep_size < HEAP_ALLOC_MIN) keep_size = HEAP_ALLOC_MIN;
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

// these can't be freed currently
void *kmalloc_heap_aligned(uint64_t size) {
    if (size == 0) return NULL;

    lock_kheap();

    heap_segment_info_t *cur_seg = (heap_segment_info_t *)kheap_start;

    uint64_t addr, needed_size, offset;

    for (;;) {
        addr = (uint64_t)cur_seg + HEAP_HEADER_LEN;
        offset = (addr & 0xFFF) ? (PAGE_LEN - (addr & 0xFFF)) : 0;
        needed_size = size + offset;

        if (cur_seg->free) {
            if (cur_seg->size > needed_size) {
                // Split the segment
                kheap_segment_split(cur_seg, needed_size);

                cur_seg->free = false;

                unlock_kheap();
                return (void *)((uint64_t)cur_seg + HEAP_HEADER_LEN + offset);
            } else if (cur_seg->size == needed_size) {
                cur_seg->free = false;

                unlock_kheap();
                return (void *)((uint64_t)cur_seg + HEAP_HEADER_LEN + offset);
            }
        }

        if (!cur_seg->next) break;
        cur_seg = cur_seg->next;
    }

    addr = kheap_end;
    offset = (addr & 0xFFF) ? (PAGE_LEN - (addr & 0xFFF)) : 0;
    needed_size = size + offset;

    heap_segment_info_t *seg = kheap_add_segment(needed_size);

    seg->free = false;

    unlock_kheap();
    return (void *)((uint64_t)seg + HEAP_HEADER_LEN + offset);
}

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

    heap_segment_info_t *cur_seg = (heap_segment_info_t *)(ptr - HEAP_HEADER_LEN);

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
    if (align && (PHYS_MEM_HEAD & is_page_misaligned(PHYS_MEM_HEAD))) {
        // Align the physical memory head to page size

        PHYS_MEM_HEAD &= 0xFFFFFFFFFFFFF000;
        PHYS_MEM_HEAD += PAGE_LEN;
    }

    if (PHYS_MEM_HEAD + size >= PHYS_MEM_USEABLE_END) {
        printf("kmalloc_a: out of physical memory.\n");
        return NULL;
    }

    uint64_t ret = PHYS_MEM_HEAD;
    PHYS_MEM_HEAD += size;

    return (void *)phys_to_virt(ret);
}