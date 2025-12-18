// TODO: find a way to handle debug messages without just printing them to the console; this will be necessary once this
// becomes a freestanding userland library

#include "common.h"
#include "sys/mman.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include <stdatomic.h>
#include <stdbool.h>
#include "stddef.h"
#include <stdint.h>

uint64_t heap_start, heap_end;

typedef struct heap_segment_info heap_segment_info_t;

struct heap_segment_info {
    size_t size;

    heap_segment_info_t *next;
    heap_segment_info_t *prev;

    bool free;
};

heap_segment_info_t *heap_last_segment = NULL;

// Set to 16 for debug purposes
#define HEAP_INIT_PAGES 160
#define HEAP_INIT_SIZE (PAGE_LEN * HEAP_INIT_PAGES)

#define HEAP_ALLOC_MIN 0x10

#define HEAP_HEADER_LEN sizeof(heap_segment_info_t)

// TODO: make this correct
#define HEAP_START_ADDR 0x2000000000

// TODO: should this be defined elsewhere?
#define PAGE_LEN 4096

// TODO: i'm pretty sure this is still needed, as different threads could share a heap
static atomic_flag heap_locked_atomic = ATOMIC_FLAG_INIT;

void lock_heap() {
    while (atomic_flag_test_and_set_explicit(&heap_locked_atomic, __ATOMIC_ACQUIRE)) {
        // Wait
        __builtin_ia32_pause();
    }
}

void unlock_heap() {
    atomic_flag_clear_explicit(&heap_locked_atomic, __ATOMIC_RELEASE);
}

static heap_segment_info_t *heap_add_segment(size_t len);

void init_heap() {
    heap_start = HEAP_START_ADDR;
    heap_end = HEAP_START_ADDR;

    heap_last_segment = NULL;

    heap_add_segment(HEAP_INIT_SIZE);
}

static heap_segment_info_t *heap_add_segment(size_t len) {
    if (len < HEAP_ALLOC_MIN) len = HEAP_ALLOC_MIN;

    /* Calculate number of pages required (round up). */
    size_t pages = (len + HEAP_HEADER_LEN + PAGE_LEN - 1) / PAGE_LEN;

    /* Align the next mapping address up to a page boundary. */
    uint64_t pg_addr = heap_end;
    if (pg_addr & (PAGE_LEN - 1)) { pg_addr = (pg_addr + PAGE_LEN - 1) & ~((uint64_t)(PAGE_LEN - 1)); }

    /* Map the pages */
    mmap((void *)pg_addr, pages * PAGE_LEN);

    heap_segment_info_t *new_segment = (heap_segment_info_t *)pg_addr;

    if (heap_last_segment) heap_last_segment->next = new_segment;

    new_segment->free = true;
    new_segment->prev = heap_last_segment;
    new_segment->next = NULL;
    new_segment->size = len;

    heap_last_segment = new_segment;
    heap_end = pg_addr + (pages * PAGE_LEN);

    return new_segment;
}

static heap_segment_info_t *heap_segment_split(heap_segment_info_t *segment, size_t keep_size) {
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

    if (heap_last_segment == segment) heap_last_segment = new_segment;

    return new_segment;
}

// TODO: is it safe to return this memory without clearing it? should be; it should automatically be cleared in kernel
// TODO: do we want to allocate the returned block aligned to 8 or 16 bytes? this might help performance; google it for details
void *malloc(size_t size) {
    if (!heap_last_segment) { return NULL; }

    if (size == 0) return NULL;

    lock_heap();

    heap_segment_info_t *cur_seg = (heap_segment_info_t *)heap_start;

    for (;;) {
        if (cur_seg->free) {
            if (cur_seg->size > size) {
                // Split the segment
                heap_segment_split(cur_seg, size);

                cur_seg->free = false;

                unlock_heap();
                return (void *)((uint64_t)cur_seg + HEAP_HEADER_LEN);
            } else if (cur_seg->size == size) {
                cur_seg->free = false;

                unlock_heap();
                return (void *)((uint64_t)cur_seg + HEAP_HEADER_LEN);
            }
        }

        if (!cur_seg->next) break;
        cur_seg = cur_seg->next;
    }

    heap_segment_info_t *seg = heap_add_segment(size);
    seg->free = false;

    unlock_heap();
    return (void *)((uint64_t)seg + HEAP_HEADER_LEN);
}

void *calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;

    if (nmemb && total_size / nmemb != size) {
        // nmemb * size overflowed the max for size_t
        return NULL;
    }

    void *ptr = malloc(total_size);
    if (ptr == NULL) return NULL;

    memset(ptr, 0, total_size);

    return ptr;
}

// Currently, realloc doesn't shrink the allocation or reduce memory usage if the new size is smaller than the current size
void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    heap_segment_info_t *cur_seg = (heap_segment_info_t *)((uint64_t)ptr - HEAP_HEADER_LEN);

    if (cur_seg->size >= size) {
        // Current segment is already large enough
        return ptr;
    }

    void *new_ptr = malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    memcpy(new_ptr, ptr, cur_seg->size);

    free(ptr);

    return new_ptr;
}

void free(void *ptr) {
    if (ptr == NULL) {
        printf("WARNING: tried to free a NULL ptr\n");
        return;
    }

    heap_segment_info_t *cur_seg = (heap_segment_info_t *)((uint64_t)ptr - HEAP_HEADER_LEN);

    lock_heap();

    if (cur_seg->free) {
        printf("ERROR: double free or corruption\n");

        unlock_heap();
        return;
    }

    cur_seg->free = true;

    unlock_heap();
}