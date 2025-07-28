#include <stdint.h>

#include "lib/stdio.h"
#include "kernel.h"

#include "memory/mm.h"
#include "memory/detect.h"
#include "memory/kmalloc.h"

uint64_t *frames; // start of bitset frames
uint64_t nframes; // Total numbers of frames

void init_pmm() {
    nframes = (uint64_t)PHYS_MEM_USEABLE_LENGTH / (uint64_t)FRAME_LEN;         // Total number of frames in the memory

    frames = (uint64_t*)kmalloc_a(sizeof(uint64_t) * (nframes + 1) / BITMAP_LEN, 1); // Allocate memory for the bitmap array
    if(frames == NULL){
        // printf("[Error] PMM: Failed to allocate memory for frames\n");
        printf("Couldn't alloc memory for frames.\n");
        return;
    }
    // clear the memory of frames array
    memset(frames, 0, sizeof(uint64_t) * (nframes + 1) / BITMAP_LEN);

    // printf(" [-] Successfully initialized PMM!\n");
}

// set the value of frames array by using bit no
void set_frame(uint64_t bit_no) {
    if (bit_no < nframes) {
        return;
    } // check either bit_no is less than total nframes i.e. 0 to nframes-1

    uint64_t bitmap_idx = INDEX_FROM_BIT_NO(bit_no);
    uint64_t bitmap_off = OFFSET_FROM_BIT_NO(bit_no);

    frames[bitmap_idx] |= (0x1 << bitmap_off);       // Set the bit
}

// Static function to find the first free frame.
// The below function will return a valid bit number or invalid bit no -1
uint64_t next_free_frame_bit()
{
    uint64_t free_bit = (uint64_t)-1;
    bool found = false;

    for (uint64_t bitmap_idx = 0; (bitmap_idx < INDEX_FROM_BIT_NO(nframes)) && !found; bitmap_idx++)
    {
        if (frames[bitmap_idx] != 0xFFFFFFFFFFFFFFFF) // if all bits not set, i.e. there has at least one bit is clear
        {    
            for (uint64_t bitmap_off = 0; bitmap_off < BITMAP_LEN; bitmap_off++)
            {
                uint64_t toTest = (uint64_t) 0x1ULL << bitmap_off; // Ensure the shift is handled as a 64-bit value.ULL means Unsigned Long Long 

                if ( !(frames[bitmap_idx] & toTest) ) // if corresponding bit is zero
                {
                    free_bit = CONVERT_BIT_NO(bitmap_idx, bitmap_off); // return corresponding bit number i.e frame index
                    found = true;
                    break;
                }
                continue; // If the current bit is set, continue to the next bit in the bitmap.
            }
        }
        continue;   // If all bits in the current bitmap are set, continue to the next bitmap index.
   }
   return free_bit; // Return an invalid frame index to indicate failure.
}

void alloc_page_frame(page_t *page, int user, int writeable) {
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
}