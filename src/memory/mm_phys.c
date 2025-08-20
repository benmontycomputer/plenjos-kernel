#include <stdint.h>

#include "kernel.h"
#include "lib/stdio.h"

#include "memory/detect.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"

bool phys_mem_lock;

// PMM doesn't need to be locked
uint32_t phys_mem_ref_frame(phys_mem_free_frame_t *frame) {
    uint32_t refcnt = (frame->flags & ~FRAME_FLAG_USABLE);

    if (refcnt >> 23) {
        printf("Somehow the refcnt on frame %p will be too big. Halt!\n", frame);
        hcf();
    }

    refcnt++;

    frame->flags &= FRAME_FLAG_USABLE;
    frame->flags |= refcnt;

    return refcnt;
}

// PMM doesn't need to be locked
uint32_t phys_mem_unref_frame(phys_mem_free_frame_t *frame) {
    uint32_t refcnt = (frame->flags & ~FRAME_FLAG_USABLE);

    if (refcnt == 0) { return 0; }

    return --(frame->flags);
}

void lock_pmm() {
    for (;;) {
        if (!phys_mem_lock) {
            phys_mem_lock = true;
            return;
        }
    }
}

void unlock_pmm() {
    phys_mem_lock = false;
}

/* void alloc_page_frame(page_t *page, int user, int writeable) {
    // idx is now the index of the first free frame.
    uint64_t bit = next_free_frame_bit();

    if (bit == (uint64_t) -1) {
        printf("No free frames for paging. Halt!\n");
        hcf();
    }

    set_frame(bit); // Mark the frame as used by passing the frame index

    page->present = 1;                      // Mark it as present.
    page->rw = writeable;                // Should the page be writeable?
    page->user = user;                      // Should the page be user-mode?
    PHYS_MEM_HEAD &= 0xFFFFFFFFFFFFF000;    // Align the physical memory head to 4KB boundary
    page->frame = (uint64_t) (PHYS_MEM_HEAD + (bit * FRAME_LEN)) >> 12;     // Store physical base address

    PHYS_MEM_HEAD += FRAME_LEN;            // Move the physical memory head to the next frame
} */

phys_mem_free_frame_t *fm2 = NULL;

uint64_t *alloc() {
    printf("alloc called in mm_phys");
    hcf();
    return NULL;
}

uint64_t find_next_free_frame() {
    lock_pmm();

    phys_mem_free_frame_t *frame = phys_mem_frame_map_next_free;

    if ((!frame) || frame >= (phys_mem_frame_map + phys_mem_frame_map_size)) {
        printf("No free frames! Last one: %p.\n", fm2);
        return 0;
        // hcf();
    }
    fm2 = frame;

    if (!(frame->flags & FRAME_FLAG_USABLE)) {
        printf("Unuseable frame (virt address %p) listed in memory map. This is likely due to corruption. Halt!\n",
               frame);
        hcf();
    }

    if (frame->flags & ~FRAME_FLAG_USABLE) {
        printf("The next free frame is already referenced! Halt!\n");
        hcf();
    }

    phys_mem_ref_frame(frame);

    phys_mem_frame_map_next_free = (phys_mem_free_frame_t *)decode_struct_frame_ptr(frame->next_free);
    if (!frame->next_free) { phys_mem_frame_map_next_free = NULL; }

    // printf("%p -> %p -> %p\n", 0xFFFF800000000000, encode_struct_frame_ptr(0xFFFF800000000000),
    // decode_struct_frame_ptr(encode_struct_frame_ptr(0xFFFF800000000000)));
    // printf("frame %p, frame encoded %p, frame next %p, frame next decoded %p\n", frame, encode_struct_frame_ptr((uint64_t)frame), frame->next_free, decode_struct_frame_ptr((uint64_t)frame->next_free));
    if (phys_mem_frame_map_next_free) phys_mem_frame_map_next_free->prev_free = 0;

    if (frame->prev_free) {
        printf("The first free frame has prev_free set. This shouldn't happen, but we'll continue anyways. Frame: %p, "
               "prev: %p, undecoded prev %p, next %p, undecoded next %p\n",
               frame, decode_struct_frame_ptr(frame->prev_free), frame->prev_free,
               decode_struct_frame_ptr(frame->next_free), frame->next_free);
        hcf();
        ((phys_mem_free_frame_t *)decode_struct_frame_ptr(frame->prev_free))->next_free
            = encode_struct_frame_ptr((uint64_t)phys_mem_frame_map_next_free);
        if (phys_mem_frame_map_next_free)
            phys_mem_frame_map_next_free->prev_free = encode_struct_frame_ptr(frame->prev_free);
    }

    // These are kept at zero for non-free frames so we don't have to manage them.
    frame->next_free = 0;
    frame->prev_free = 0;

    unlock_pmm();

    return frame_addr_to_phys_addr((uint64_t)frame);
}

void alloc_page_frame(page_t *page, int user, int writeable) {
    page->present = 1;
    page->rw = writeable;
    page->user = user;
    page->frame = find_next_free_frame();
}