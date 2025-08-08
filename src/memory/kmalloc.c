#include <stdint.h>

#include "lib/stdio.h"

#include "memory/detect.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"

uint64_t heap_start, heap_end;

typedef struct heap_segment_info heap_segment_info_t;

struct heap_segment_info {
    size_t size;

    heap_segment_info_t *next;
    heap_segment_info_t *prev;

    bool free;
};

heap_segment_info_t *last_segment = NULL;

static inline bool is_page_misaligned(uint64_t ptr) {
    return (bool)(ptr & 0xFFF);
}

// Set to 16 for debug purposes
#define HEAP_INIT_PAGES 16
#define HEAP_INIT_SIZE (PAGE_LEN * HEAP_INIT_PAGES)

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

void init_kernel_heap() {
    lock_kheap();

    heap_start = KERNEL_HEAP_START_ADDR;
    heap_end = KERNEL_HEAP_START_ADDR + HEAP_INIT_SIZE;

    uint64_t pos = KERNEL_HEAP_START_ADDR;

    for (size_t i = 0; i < HEAP_INIT_PAGES; i++) {
        find_page(pos, true, kernel_pml4);
        pos += PAGE_LEN;
    }

    last_segment = (heap_segment_info_t *)heap_start;

    last_segment->free = true;
    last_segment->next = NULL;
    last_segment->prev = NULL;
    last_segment->size = HEAP_INIT_SIZE - HEAP_HEADER_LEN;

    unlock_kheap();
}

static heap_segment_info_t *kheap_add_segment(size_t len) {
    if (len < HEAP_ALLOC_MIN) len = HEAP_ALLOC_MIN;

    size_t pages = len + HEAP_HEADER_LEN;

    // Page align len, rounding up
    if (pages % PAGE_LEN) { pages += PAGE_LEN; }

    pages = pages / PAGE_LEN;

    uint64_t pg_addr = heap_end & ~(uint64_t)0xFFF;

    // Map the pages
    for (size_t i = 0; i <= pages; i++) {
        find_page(pg_addr + (i * PAGE_LEN), true, kernel_pml4);
    }

    heap_segment_info_t *new_segment = (heap_segment_info_t *)heap_end;

    last_segment->next = new_segment;

    new_segment->free = true;
    new_segment->prev = last_segment;
    new_segment->next = NULL;
    new_segment->size = len;

    last_segment = new_segment;

    heap_end += (len + HEAP_HEADER_LEN);

    return new_segment;
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

    if (last_segment == segment) last_segment = new_segment;

    return new_segment;
}

void *kmalloc_heap(uint64_t size) {
    if (size == 0) return NULL;

    lock_kheap();

    heap_segment_info_t *cur_seg = (heap_segment_info_t *)heap_start;

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