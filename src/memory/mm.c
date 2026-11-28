// partially adapted from KeblaOS

#include "memory/mm.h"

#include "kernel.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "memory/detect.h"

#include <limine.h>

kernel_memory_map_t memmap;

static fixed_size_allocator_t _mm_fsa = FSA_DEFAULT(sizeof(mm_t));
mm_t _kernel_mm                       = { 0 };
mm_t *kernel_mm                       = &_kernel_mm;

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_memmap_request memmap_request = { .id = LIMINE_MEMMAP_REQUEST, .revision = 3 };

static int vma_cmp(const rbnode_t *a, const rbnode_t *b) {
    vm_area_t *va = (vm_area_t *)a;
    vm_area_t *vb = (vm_area_t *)b;

    return (va->start > vb->start) - (va->start < vb->start);
}

void init_memory_manager() {
    if (!memmap_request.response) {
        printf("\nNo memmap detected. Halt!\n");
        hcf();
    }

    detect_memory(memmap_request.response);

    init_paging();

    kernel_mm->cr3     = kernel_pml4_phys;
    kernel_mm->pml4    = kernel_pml4;
    kernel_mm->ref_cnt = 1;

    rbtree_init(&kernel_mm->tree, vma_cmp);

    parse_memmap_limine(memmap_request.response);
    populate_kernel_half_pml4();

    // This will crash if paging isn't properly set up.
    // *((char *)(KERNEL_START_ADDR)) = 0;
}

// Function to allocate a new page table / page directory / page directory pointer table
uint64_t *alloc_paging_node() {
    uint64_t *pt = (uint64_t *)find_next_free_frame();
    if (!pt) {
        kout(KERNEL_WARN, "alloc_paging_node(): OOM: failed to allocate paging node\n");
        return NULL; // Allocation failed
    }
    pt = (uint64_t *)phys_to_virt((uint64_t)pt);
    memset(pt, 0, PAGE_LEN); // Zero out the page table
    return pt;
}

mm_t *mm_new() {
    mm_t *mm = fsa_alloc(&_mm_fsa);
    if (!mm) {
        kout(KERNEL_WARN, "mm_new(): OOM: fsa_alloc() failed!\n");
        return NULL;
    }

    mm->lock = RWSEM_INIT;

    mm->pml4 = (pml4_t *)alloc_paging_node();
    if (!mm->pml4) {
        kout(KERNEL_WARN, "mm_new(): OOM: couldn't allocate PML4\n");
        fsa_free(&_mm_fsa, mm);
        return NULL;
    }

    for (int i = 256; i < 512; i++) {
        mm->pml4->entries[i] = kernel_pml4->entries[i];
    }

    mm->cr3 = virt_to_phys((uint64_t)mm->pml4);

    mm_ref(mm);

    rbtree_init(&mm->tree, vma_cmp);

    return mm;
}

int mm_ref(mm_t *mm) {
    return atomic_fetch_add(&mm->ref_cnt, 1);
}

static void mm_free_node(rbnode_t *node) {
    vm_destroy_area((vm_area_t *)node);
}

int mm_unref(mm_t *mm) {
    int prev = atomic_fetch_sub(&mm->ref_cnt, 1);

    if (prev == 1) {
        // Free
        rbtree_destroy(&mm->tree, mm_free_node);

        free_user_page_table(mm->pml4);

        fsa_free(&_mm_fsa, mm);
    }

    return prev;
}