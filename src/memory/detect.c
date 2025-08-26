#define LIMINE_API_REVISION 3

#include <limine.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory/detect.h"
#include "memory/mm.h"

#include "kernel.h"

#include "lib/stdio.h"

__attribute__((used, section(".limine_requests"))) static volatile struct limine_paging_mode_request paging_mode_request
    = { .id = LIMINE_PAGING_MODE_REQUEST, .revision = 3 };

__attribute__((used, section(".limine_requests"))) static volatile struct limine_hhdm_request hhdm_request
    = { .id = LIMINE_HHDM_REQUEST, .revision = 3 };

uint64_t PAGING_MODE;
uint64_t HHDM_OFFSET;
uint64_t PHYS_MEM_HEAD;

uint64_t TSS_STACK_ADDR;
uint64_t KERNEL_HEAP_START_ADDR;

uint64_t PHYS_MEM_USEABLE_LENGTH;
uint64_t PHYS_MEM_USEABLE_START;
uint64_t PHYS_MEM_USEABLE_END;

phys_mem_free_frame_t *phys_mem_frame_map;
phys_mem_free_frame_t *phys_mem_frame_map_next_free;
uint64_t phys_mem_frame_map_size;

uint64_t *test_alloc() {
    printf("alloc called");
    hcf();
}

uint64_t phys_mem_total_frames = 0;
uint64_t last_useable_phys_frame = 0;

void parse_memmap_limine(struct limine_memmap_response *memmap_response) {
    uint64_t phys_mem_useable_frames = 0;

    memmap.mem_entries = memmap_response->entries;
    memmap.mem_entry_count = memmap_response->entry_count;

    for (size_t i = 0; i < memmap.mem_entry_count; i++) {
        if (memmap.mem_entries[i]->type == LIMINE_MEMMAP_USABLE) {
            phys_mem_useable_frames += memmap.mem_entries[i]->length;
            last_useable_phys_frame = memmap.mem_entries[i]->base + memmap.mem_entries[i]->length - PAGE_LEN;
        }
        
        phys_mem_total_frames += memmap.mem_entries[i]->length;
    }

    phys_mem_frame_map_size = ((phys_mem_total_frames + PAGE_LEN - 1) / PAGE_LEN) * sizeof(phys_mem_free_frame_t);

    printf("Total useable physical memory: %dMiB\nTotal physical memory: %dMiB\nLast useable physical frame: %p\nNeed to allocate %dMiB for table.\n", (int)(phys_mem_useable_frames >> 20), (int)(phys_mem_total_frames >> 20), last_useable_phys_frame, (int)(phys_mem_frame_map_size >> 20));

    // uint64_t page_table_mem_needed = (PAGE_LEN * (phys_mem_frame_map_size + PAGE_LEN - 1) / PAGE_LEN)

    for (size_t i = 0; i < memmap.mem_entry_count; i++) {
        uint64_t base = memmap.mem_entries[i]->base;
        uint64_t length = memmap.mem_entries[i]->length;
        uint64_t type = memmap.mem_entries[i]->type;

        if (type == LIMINE_MEMMAP_USABLE) {
            if (length >= (phys_mem_frame_map_size + PAGE_LEN)) {
                map_virtual_memory_using_alloc(base, phys_to_virt(base), length, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, test_alloc, kernel_pml4);

                phys_mem_frame_map = (phys_mem_free_frame_t *)phys_to_virt(base);
                memset(phys_mem_frame_map, 0, phys_mem_frame_map_size);

                for (uint64_t j = phys_addr_to_frame_addr(base); j < phys_addr_to_frame_addr(base + phys_mem_frame_map_size); j += sizeof(phys_mem_free_frame_t)) {
                    ((phys_mem_free_frame_t *)j)->flags = FRAME_FLAG_USABLE | 0x1;
                }

                break;
            }
        }
    }

    if (!phys_mem_frame_map) {
        printf("No suitable memory block found. Halt!\n");
        hcf();
    }

    phys_mem_free_frame_t *prev_free_frame = NULL;
    phys_mem_free_frame_t *frame = NULL;

    for (size_t i = 0; i < memmap.mem_entry_count; i++) {
        uint64_t base = memmap.mem_entries[i]->base;
        uint64_t length = memmap.mem_entries[i]->length;
        uint64_t type = memmap.mem_entries[i]->type;

        if (type == LIMINE_MEMMAP_USABLE) {
            // if (base != virt_to_phys((uint64_t)phys_mem_frame_map))
            map_virtual_memory_using_alloc(base, phys_to_virt(base), length, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, test_alloc, kernel_pml4);

            // TODO: i think the code might break if frame 0 is actually valid.
            for (uint64_t j = phys_addr_to_frame_addr(base); j < phys_addr_to_frame_addr(base + length) - sizeof(phys_mem_free_frame_t); j += sizeof(phys_mem_free_frame_t)) {
                frame = (phys_mem_free_frame_t *)j;

                if (frame->flags == 0) {
                    frame->flags = FRAME_FLAG_USABLE;

                    if (prev_free_frame) {
                        frame->prev_free = encode_struct_frame_ptr((uint64_t)prev_free_frame);
                        prev_free_frame->next_free = encode_struct_frame_ptr((uint64_t)frame);
                    } else {
                        printf("Next frame %p, prev: %p\n", frame, frame->prev_free);
                        if (!phys_mem_frame_map_next_free)
                            phys_mem_frame_map_next_free = frame;
                    }

                    prev_free_frame = frame;
                }
            }
        }
    }

    // phys_mem_frame_map_next_free = (phys_mem_free_frame_t *)decode_struct_frame_ptr(phys_mem_frame_map_next_free->next_free);
    
    // map_virtual_memory_using_alloc(frame_addr_to_phys_addr((uint64_t)phys_mem_frame_map_next_free), PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, test_alloc, kernel_pml4);

    printf("Physical memory map setup complete. Last frame addr: %p; next free frame: %p, next free frame prev/next: %p %p\n", prev_free_frame, phys_mem_frame_map_next_free, phys_mem_frame_map_next_free->prev_free, phys_mem_frame_map_next_free->next_free);
}

void detect_memory(struct limine_memmap_response *memmap_response) {
    if (!hhdm_request.response) {
        // Error!
        printf("Couldn't get HHDM offset.\n");
    } else {
        HHDM_OFFSET = hhdm_request.response->offset;
    }

    if (!paging_mode_request.response) {
        // Error!
        printf("Couldn't get paging mode.\n");
    } else {
        PAGING_MODE = paging_mode_request.response->mode;
    }

    TSS_STACK_ADDR = 0xFFFF8E0000000000;
    KERNEL_HEAP_START_ADDR = 0xFFFF900000000000;
}