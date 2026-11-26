#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/irq.h"
#include "cpu/cpu.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "memory/detect.h"
#include "memory/mm.h"

#include <stdint.h>

static fixed_size_allocator_t _vma_fsa = FSA_DEFAULT(sizeof(vm_area_t));

static vm_area_t *_vmlookup_nolock(void *virt, mm_t *mm) {
    vm_area_t vma = { 0 };
    vma.start     = (ptr_t)virt;

    vm_area_t *res = (vm_area_t *)rbtree_find_or_closest_predecessor(&mm->tree, &vma.node);
    if (!res || res->start > (uint64_t)virt || res->end <= (uint64_t)virt) {
        res = NULL;
    }
    return res;
}

static bool _vmlookup_range_is_occupied_nolock(ptr_t start, ptr_t end, mm_t *mm) {
    if (end <= start) {
        panic("_vmlookup_range_is_occupied_nolock(): no range (%p to %p)!\n", start, end);
    }

    vm_area_t vma = { 0 };
    vma.start     = end - 1;

    vm_area_t *res = (vm_area_t *)rbtree_find_or_closest_predecessor(&mm->tree, &vma.node);
    if (!res || res->end <= start) {
        return false;
    }
    return true;
}

vm_area_t *vmlookup(void *virt, mm_t *mm) {
    rwsem_acquire_read(&mm->lock);

    vm_area_t *res = _vmlookup_nolock(virt, mm);

    rwsem_release_read(&mm->lock);
    return res;
}

// Does everything except mapping pages and inserting into the tree
static int _vm_prepare_range_nolock(void *virt_start, size_t len, vma_flags_t requested_flags, mm_t *mm,
                                           uint64_t *pt_flags_out, uint64_t *page_flags_out, ptr_t *start_out,
                                           ptr_t *end_out, vm_area_t **out) {
    ptr_t start = (ptr_t)virt_start & ~0xFFF;
    ptr_t end   = (start + len + ((ptr_t)virt_start - start) + PAGE_LEN - 1) & ~0xFFF;
    if (_vmlookup_range_is_occupied_nolock(start, end, mm)) {
        return -EEXIST;
    }

    uint64_t pt_flags = PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;

    if (start <= USER_END_ADDR && (end - 1) <= USER_END_ADDR) {
        // In bounds
        if (requested_flags & VMA_USER) {
            pt_flags |= PAGE_FLAG_USER;
        } else {
            return -EFAULT;
        }
    } else if (start >= KERNEL_START_ADDR && (end - 1) <= KERNEL_END_ADDR) {
        if (requested_flags & VMA_USER) {
            return -EFAULT;
        }
    }

    vm_area_t *vma = fsa_alloc(&_vma_fsa);
    if (!vma) {
        kout(KERNEL_WARN, "_vm_prepare_range_nolock(): OOM: fsa_alloc() failed!\n");
        return -ENOMEM;
    }

    uint64_t page_flags = PAGE_FLAG_PRESENT;
    if (requested_flags & VMA_USER) {
        page_flags |= PAGE_FLAG_USER;
        vma->flags |= VMA_USER;
    }
    if (requested_flags & VMA_RW) {
        page_flags |= PAGE_FLAG_WRITE;
        vma->flags |= VMA_RW;
    }
    if (!(requested_flags & VMA_EXEC)) {
        page_flags |= PAGE_FLAG_NX;
    } else {
        vma->flags |= VMA_EXEC;
    }

    vma->start = start;
    vma->end   = end;
    vma->mm    = mm;

    *pt_flags_out   = pt_flags;
    *page_flags_out = page_flags;
    *start_out      = start;
    *end_out        = end;

    if (out) *out = vma;
    return 0;
}

// pml4 = pointer map level 4, pdpt = page directory pointer table, pd = page directory, pt = page table
static inline void addr_split(uint64_t virt, uint32_t *pml4, uint32_t *pdpt, uint32_t *pd, uint32_t *pt) {
    *pml4 = ((virt) >> 39) & 0x1FF; // 39-47
    *pdpt = ((virt) >> 30) & 0x1FF; // 30-38
    *pd   = ((virt) >> 21) & 0x1FF; // 21-29
    *pt   = ((virt) >> 12) & 0x1FF; // 12-20
}

void cleanup_frames(ptr_t start, ptr_t failed, mm_t *mm) {
    for (ptr_t v = start; v < failed; v += PAGE_LEN) {
        physptr_t phys = get_physaddr((uint64_t)v, mm);
        phys_mem_unref_frame(phys_addr_to_frame_struct(phys));
    }
}

int vmalloc(void *virt_hint, size_t size, vma_flags_t requested_flags, vm_area_t **out, mm_t *mm) {
    rwsem_acquire_write(&mm->lock);

    // TODO: check for user flag and validate that we actually pick a virtual memory address in user/kernel range
    // TODO: once we search for a range, honor requested_flags' FIXED option

    uint64_t pt_flags, page_flags;
    ptr_t start, end;
    vm_area_t *vma = NULL;
    int res = _vm_prepare_range_nolock(virt_hint, size, requested_flags, mm, &pt_flags, &page_flags, &start, &end, &vma);
    if (res) {
        if (out) *out = NULL;
        goto cleanup;
    }

    bool zero = !!(requested_flags & VMALLOC_ZERO);

    uint32_t i_pml4, i_pdpt, i_pd, i_pt;
    uint64_t *pdpt_table, *pd_table, *pt_table;

    // The entire area is clear
    // Map as many pages as needed to fill the range
    for (ptr_t v = start; v < end; v += PAGE_LEN) {
        addr_split((uint64_t)v, &i_pml4, &i_pdpt, &i_pd, &i_pt);

        // Search for a page directory pointer table; create if not found
        if (!(((uint64_t *)mm->pml4)[i_pml4] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page directory pointer table and add it to the pml4 table
            pdpt_table                     = alloc_paging_node();
            ((uint64_t *)mm->pml4)[i_pml4] = (virt_to_phys((uint64_t)pdpt_table) | pt_flags);
        } else {
            // Retrieve the page directory pointer table, making sure to discard unnecessary bits.
            pdpt_table = (uint64_t *)phys_to_virt((uint64_t)((uint64_t *)mm->pml4)[i_pml4] & PHYSADDR_MASK);
        }

        // Search for a page directory table; create if not found
        if (!(pdpt_table[i_pdpt] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page directory table and add it to the page directory pointer table
            pd_table           = alloc_paging_node();
            pdpt_table[i_pdpt] = (virt_to_phys((uint64_t)pd_table) | pt_flags);
        } else {
            // Retrieve the page directory table, making sure to discard unnecessary bits
            pd_table = (uint64_t *)phys_to_virt((uint64_t)pdpt_table[i_pdpt] & PHYSADDR_MASK);
        }

        // Search for a page table; create if not found
        if (!(pd_table[i_pd] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page table and add it to the page directory table
            pt_table       = alloc_paging_node();
            pd_table[i_pd] = (virt_to_phys((uint64_t)pt_table) | pt_flags);
        } else {
            // Retrieve the page table, making sure to discard unnecessary bits
            pt_table = (uint64_t *)phys_to_virt((uint64_t)pd_table[i_pd] & PHYSADDR_MASK);
        }

        // Finally, set the entry in the page table
        // printf("mapping 2 %p\n", phys_current);
        uint64_t phys = find_next_free_frame();
        if (!phys) {
            kout(KERNEL_WARN, "vmalloc(): OOM: couldn't find free frame!\n");
            cleanup_frames(start, v, mm);
            fsa_free(&_vma_fsa, vma);
            if (out) *out = NULL;
            res = -ENOMEM;
            goto cleanup;
        }
        pt_table[i_pt] = ((phys & PHYSADDR_MASK) | page_flags);
        if (zero) {
            memset((void *)phys_to_virt(phys), 0, PAGE_LEN);
        }

        // No TLB flush is needed because we are allocating fresh vmem.
    }

    rbtree_insert(&mm->tree, &vma->node);
    if (out) *out = vma;

cleanup:
    rwsem_release_write(&mm->lock);
    return res;
}

static bool page_table_reap_if_empty(uint64_t *table) {
    for (int i = 0; i < 512; i++) {
        if (table[i] & PAGE_FLAG_PRESENT) return false;
    }

    return true;
}

void vmfree(vm_area_t *vma, mm_t *mm) {
    rwsem_acquire_write(&mm->lock);

    int res = rbtree_remove(&mm->tree, &vma->node);
    if (res) {
        panic("vmfree(): couldn't remove from tree!\n");
    }

    uint32_t i_pml4, i_pdpt, i_pd, i_pt;
    uint64_t *pdpt_table, *pd_table, *pt_table;

    for (ptr_t v = vma->start; v < vma->end; v += PAGE_LEN) {
        uint64_t phys = get_physaddr((uint64_t)v, mm);

        if (!phys) {
            kout(KERNEL_PANIC, "free_virtual_memory(): couldn't get mapping for virt %p (mm %p)\n", v, mm);
        }

        phys_mem_unref_frame(phys_addr_to_frame_struct(phys));

        addr_split((uint64_t)v, &i_pml4, &i_pdpt, &i_pd, &i_pt);

        // Search for a page directory pointer table; create if not found
        if (!(((uint64_t *)mm->pml4)[i_pml4] & PAGE_FLAG_PRESENT)) {
            panic("free_virtual_memory(): missing pml4_table entry!\n");
        } else {
            // Retrieve the page directory pointer table, making sure to discard unnecessary bits.
            pdpt_table = (uint64_t *)phys_to_virt((uint64_t)((uint64_t *)mm->pml4)[i_pml4] & PHYSADDR_MASK);
        }

        // Search for a page directory table; create if not found
        if (!(pdpt_table[i_pdpt] & PAGE_FLAG_PRESENT)) {
            panic("free_virtual_memory(): missing pdpt_table entry!\n");
        } else {
            // Retrieve the page directory table, making sure to discard unnecessary bits
            pd_table = (uint64_t *)phys_to_virt((uint64_t)pdpt_table[i_pdpt] & PHYSADDR_MASK);
        }

        // Search for a page table; create if not found
        if (!(pd_table[i_pd] & PAGE_FLAG_PRESENT)) {
            panic("free_virtual_memory(): missing pd_table entry!\n");
        } else {
            // Retrieve the page table, making sure to discard unnecessary bits
            pt_table = (uint64_t *)phys_to_virt((uint64_t)pd_table[i_pd] & PHYSADDR_MASK);
        }

        pt_table[i_pt] = 0ULL;

        if (page_table_reap_if_empty(pt_table)) {
            pd_table[i_pd] = 0ULL;
        }

        if (page_table_reap_if_empty(pd_table)) {
            pdpt_table[i_pdpt] = 0ULL;
        }

        // Don't reap pml4 entries (pdpt_table), as they might be global kernel ones and there is little benefit
    }

    flush_tlb_range(vma->start, vma->end, mm);

    rwsem_release_write(&mm->lock);
}

static inline void flush_ipi_all() {
    if (!smp_loaded) return;

    uint32_t curr_core = get_curr_core();

    for (uint32_t i = 0; i < get_n_cores(); i++) {
        if (cpu_cores[i].online && i != curr_core) {
            send_ipi(cpu_cores[i].lapic_id, IPI_TLB_SHOOTDOWN_IRQ + 32); // IPI for tlb shootdown
        }
    }
}

// Function to flush the entire TLB (by writing to cr3)
static inline void flush_tlb_all() {
    uint64_t cr3;
    // Get the current value of CR3 (the base of the PML4 table)
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    // Write the value of CR3 back to itself, which will flush the TLB
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

    uint32_t curr_core = get_curr_core();

    flush_ipi_all();
}

// MAKE SURE TO SEND THE IPI AFTER CALLING THIS FUNCTION
static inline void __native_flush_tlb_single(uint64_t addr) {
    asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

void flush_tlb_range(ptr_t start, ptr_t end, mm_t *mm) {
    // TODO: implement
    flush_tlb_all();
}

// TODO: VERY IMPORTANT: handle OOM errors on alloc_paging_node()
// The alloc func should return a kernel-mapped pointer
int vmmap(physptr_t phys_start, void *virt_start, size_t len, vma_flags_t requested_flags, vm_area_t **out, mm_t *mm) {
    rwsem_acquire_write(&mm->lock);

    uint64_t pt_flags, page_flags;
    ptr_t start, end;
    vm_area_t *vma = NULL;
    int res = _vm_prepare_range_nolock(virt_start, len, requested_flags, mm, &pt_flags, &page_flags, &start, &end, &vma);
    if (res) {
        if (out) *out = NULL;
        goto cleanup;
    }

    uint32_t i_pml4, i_pdpt, i_pd, i_pt;
    uint64_t *pdpt_table, *pd_table, *pt_table;

    // The entire area is clear
    // Map as many pages as needed to fill the range
    for (ptr_t v = start; v < end; v += PAGE_LEN) {
        addr_split((uint64_t)v, &i_pml4, &i_pdpt, &i_pd, &i_pt);

        // Search for a page directory pointer table; create if not found
        if (!(((uint64_t *)mm->pml4)[i_pml4] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page directory pointer table and add it to the pml4 table
            pdpt_table                     = alloc_paging_node();
            ((uint64_t *)mm->pml4)[i_pml4] = (virt_to_phys((uint64_t)pdpt_table) | pt_flags);
        } else {
            // Retrieve the page directory pointer table, making sure to discard unnecessary bits.
            pdpt_table = (uint64_t *)phys_to_virt((uint64_t)((uint64_t *)mm->pml4)[i_pml4] & PHYSADDR_MASK);
        }

        // Search for a page directory table; create if not found
        if (!(pdpt_table[i_pdpt] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page directory table and add it to the page directory pointer table
            pd_table           = alloc_paging_node();
            pdpt_table[i_pdpt] = (virt_to_phys((uint64_t)pd_table) | pt_flags);
        } else {
            // Retrieve the page directory table, making sure to discard unnecessary bits
            pd_table = (uint64_t *)phys_to_virt((uint64_t)pdpt_table[i_pdpt] & PHYSADDR_MASK);
        }

        // Search for a page table; create if not found
        if (!(pd_table[i_pd] & PAGE_FLAG_PRESENT)) {
            // Allocate a new page table and add it to the page directory table
            pt_table       = alloc_paging_node();
            pd_table[i_pd] = (virt_to_phys((uint64_t)pt_table) | pt_flags);
        } else {
            // Retrieve the page table, making sure to discard unnecessary bits
            pt_table = (uint64_t *)phys_to_virt((uint64_t)pd_table[i_pd] & PHYSADDR_MASK);
        }

        // Finally, set the entry in the page table
        // printf("mapping 2 %p\n", phys_current);
        uint64_t phys  = phys_start + v - start;
        pt_table[i_pt] = ((phys & PHYSADDR_MASK) | page_flags);

        // No TLB flush is needed because we are allocating fresh vmem.
    }

    rbtree_insert(&mm->tree, &vma->node);
    if (out) *out = vma;

cleanup:
    rwsem_release_write(&mm->lock);
    return res;
}

physptr_t get_physaddr(uint64_t virt, mm_t *mm) {
    uint32_t pml4_index, pdpt_index, pd_index, pt_index;

    addr_split(virt, &pml4_index, &pdpt_index, &pd_index, &pt_index);

    dir_entry_t pml4_entry = mm->pml4->entries[pml4_index];

    if (!pml4_entry.present) return 0;

    pdpt_t *pdpt_table     = (pdpt_t *)phys_to_virt(pml4_entry.base_addr << 12);
    dir_entry_t pdpt_entry = pdpt_table->entries[pdpt_index];

    if (!pdpt_entry.present) return 0;
    if (*(uint64_t *)&pdpt_entry & PS_BIT) // 1 GiB page
        return ((*(uint64_t *)&pdpt_entry) & 0x000FFFFFC0000000ULL) + (virt & 0x3FFFFFFFULL);

    pd_t *pd_table       = (pd_t *)phys_to_virt(pdpt_entry.base_addr << 12);
    dir_entry_t pd_entry = pd_table->entries[pd_index];

    if (!pd_entry.present) return 0;
    if (*(uint64_t *)&pd_entry & PS_BIT) // 2 MiB page
        return ((*(uint64_t *)&pd_entry) & 0x000FFFFFFFE00000ULL) + (virt & 0x1FFFFFULL);

    pt_t *pt_table = (pt_t *)phys_to_virt(pd_entry.base_addr << 12);
    page_t page    = pt_table->pages[pt_index];

    if (!page.present) return 0;

    return (page.frame << 12) + (virt & 0xFFF);
}

uint32_t get_physaddr32(uint64_t virt, mm_t *mm) {
    uint64_t phys = get_physaddr(virt, mm);

    if (phys >> 32) {
        printf("WARNING: get_physaddr32() called, but resulting physaddr is >32bit\n");
        return 0U;
    }

    return phys & 0xFFFFFFFF;
}