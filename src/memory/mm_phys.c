#include "kernel.h"
#include "lib/lock.h"
#include "lib/stdio.h"
#include "memory/detect.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"

#include <stdatomic.h>
#include <stdint.h>

// TODO: can we make parts of these frames atomic instead, allowing us to avoid locking the entire PMM?
static mutex phys_mem_lock = MUTEX_INIT;

// PMM doesn't need to be locked
uint32_t phys_mem_ref_frame(phys_mem_free_frame_t *frame) {
    uint32_t refcnt = (frame->flags & ~FRAME_FLAG_USABLE);

    if (++refcnt >> 23) {
        printf("Somehow the refcnt on frame %p will be too big. Panic!\n", frame);
        panic("phys_mem_ref_frame: refcnt out of range!\n");
    }

    frame->flags &= FRAME_FLAG_USABLE;
    frame->flags |= refcnt;

    return refcnt;
}

// PMM doesn't need to be locked
uint32_t phys_mem_unref_frame(phys_mem_free_frame_t *frame) {
    uint32_t refcnt = (frame->flags & ~FRAME_FLAG_USABLE);

    if (refcnt == 0) {
        return 0;
    }

    return --(frame->flags);
}

phys_mem_free_frame_t *fm2 = NULL;

uint64_t *alloc() {
    printf("alloc called in mm_phys");
    hcf();
    return NULL;
}

uint64_t find_next_free_frame() {
    mutex_lock(&phys_mem_lock);

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
    if (!frame->next_free) {
        phys_mem_frame_map_next_free = NULL;
    }

    // printf("%p -> %p -> %p\n", 0xFFFF800000000000, encode_struct_frame_ptr(0xFFFF800000000000),
    // decode_struct_frame_ptr(encode_struct_frame_ptr(0xFFFF800000000000)));
    // printf("frame %p, frame encoded %p, frame next %p, frame next decoded %p\n", frame,
    // encode_struct_frame_ptr((uint64_t)frame), frame->next_free, decode_struct_frame_ptr((uint64_t)frame->next_free));
    if (phys_mem_frame_map_next_free) phys_mem_frame_map_next_free->prev_free = 0;

    if (frame->prev_free) {
        printf("The first free frame has prev_free set. This shouldn't happen! Frame: %p, "
               "prev: %p, undecoded prev %p, next %p, undecoded next %p\n",
               frame, decode_struct_frame_ptr(frame->prev_free), frame->prev_free,
               decode_struct_frame_ptr(frame->next_free), frame->next_free);
        panic("The first free frame has prev_free set!");
        ((phys_mem_free_frame_t *)decode_struct_frame_ptr(frame->prev_free))->next_free
            = encode_struct_frame_ptr((uint64_t)phys_mem_frame_map_next_free);
        if (phys_mem_frame_map_next_free)
            phys_mem_frame_map_next_free->prev_free = encode_struct_frame_ptr(frame->prev_free);
    }

    // These are kept at zero for non-free frames so we don't have to manage them.
    frame->next_free = 0;
    frame->prev_free = 0;

    mutex_unlock(&phys_mem_lock);

    return frame_addr_to_phys_addr((uint64_t)frame);
}

void alloc_page_frame(page_t *page, int user, int writeable) {
    page->present = 1;
    page->rw      = writeable;
    page->user    = user;
    page->frame   = find_next_free_frame() >> 12;
}