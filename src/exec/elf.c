#include "exec/elf.h"

#include "kernel.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "memory/mm_common.h"
#include "plenjos/errno.h"

#include <stdbool.h>
#include <stdint.h>

#define ELF_MAGIC          0x464C457FU // "\x7f" "ELF", read as a little-endian uint32_t
#define ELF_CLASS_64       2
#define ELF_DATA_LSB       1
#define ELF_MACHINE_X86_64 0x3E
#define ELF_TYPE_EXEC      2
#define ELF_TYPE_DYN       3

// Sanity limits, so a corrupt or hostile file can't make us loop for ages or exhaust memory in one segment.
#define ELF_MAX_PHDRS       64
#define ELF_MAX_SEGMENT_MEM (256ULL << 20)

// Everything we map must stay below this (the lower half of the canonical address space).
#define ELF_USER_TOP 0x0000800000000000ULL

// Copies count bytes from src into the (already mapped) user range starting at dest in the given address space.
int elf_copy(void *dest, void *src, size_t count, mm_t *mm) {
    uint64_t offs = 0;

    while (offs < count) {
        uint64_t addr  = (uint64_t)dest + offs;
        uint64_t paddr = get_physaddr(addr, mm);

        if (!paddr) {
            return -EFAULT;
        }

        // Never copy past the end of the page that paddr is in
        size_t chunk = PAGE_LEN - (addr % PAGE_LEN);
        if (chunk > count - offs) {
            chunk = count - offs;
        }

        memcpy((void *)phys_to_virt(paddr), (uint8_t *)src + offs, chunk);
        offs += chunk;
    }

    return 0;
}

// page-aligned virtual range [lo, hi) with flags giving permissions for all PT_LOAD touching it
typedef struct {
    uint64_t lo, hi;
    vma_flags_t flags;
} elf_range_t;

static vma_flags_t elf_vma_flags(uint32_t pf) {
    return VMA_USER | ((pf & ELF_PHDR_W) ? VMA_RW : 0) | ((pf & ELF_PHDR_EX) ? VMA_EXEC : 0);
}

// Validates one PT_LOAD program header against the file and the user address range; returns its absolute start.
static int elf_check_segment(size_t elf_size, const ELF_program_header_t *phdr, uint64_t voffs, uint64_t *start_out) {
    if (phdr->filesz > phdr->memsz || phdr->memsz > ELF_MAX_SEGMENT_MEM) {
        return -ENOEXEC;
    }

    // The data must actually be inside the file
    if (phdr->offset > elf_size || phdr->filesz > elf_size - phdr->offset) {
        return -ENOEXEC;
    }

    // The whole range [vaddr + voffs, vaddr + voffs + memsz) must be in user space, without overflowing
    if (phdr->vaddr >= ELF_USER_TOP - voffs) {
        return -ENOEXEC;
    }

    uint64_t start = phdr->vaddr + voffs;
    if (phdr->memsz > ELF_USER_TOP - start) {
        return -ENOEXEC;
    }

    *start_out = start;
    return 0;
}

// Builds the ranges to map, preventing overlapping mmap calls
static int elf_build_ranges(size_t elf_size, const ELF_program_header_t *phdrs, uint16_t n, uint64_t voffs,
                            elf_range_t *out) {
    int cnt = 0;

    for (uint16_t i = 0; i < n; i++) {
        const ELF_program_header_t *p = &phdrs[i];
        if (p->type != PT_LOAD || p->memsz == 0) {
            continue;
        }

        uint64_t start;
        int res = elf_check_segment(elf_size, p, voffs, &start);
        if (res) {
            return res;
        }

        elf_range_t r = {
            .lo    = start & ~(uint64_t)(PAGE_LEN - 1),
            .hi    = (start + p->memsz + PAGE_LEN - 1) & ~(uint64_t)(PAGE_LEN - 1),
            .flags = elf_vma_flags(p->flags),
        };

        // Insertion sort by start address (n <= ELF_MAX_PHDRS)
        int j = cnt++;
        while (j > 0 && out[j - 1].lo > r.lo) {
            out[j] = out[j - 1];
            j--;
        }
        out[j] = r;
    }

    // Merge anything that overlaps or shares a page
    int merged = 0;
    for (int i = 0; i < cnt; i++) {
        if (merged > 0 && out[i].lo < out[merged - 1].hi) {
            if (out[i].hi > out[merged - 1].hi) {
                out[merged - 1].hi = out[i].hi;
            }
            out[merged - 1].flags |= out[i].flags;
        } else {
            out[merged++] = out[i];
        }
    }

    return merged;
}

// Loads an ELF64 x86-64 executable / shared object into the address space pml4, at voffs_if_pic (which must be page
// aligned; all of our binaries are position independent, so this is added to every address in the file).
// elf_size is the size of the buffer at elf_base; nothing outside it is ever read.
// Returns 0 on success (entry point in *entry_out), or a negative errno (-ENOEXEC for a file that isn't a valid
// executable). On failure, pages that were already mapped stay mapped; the caller should tear the process down.
int loadelf(void *elf_base, size_t elf_size, mm_t *mm, uint64_t *entry_out, uint64_t voffs_if_pic) {
    if (!elf_base || !mm || !entry_out) {
        return -EINVAL;
    }

    if ((voffs_if_pic % PAGE_LEN) || voffs_if_pic >= ELF_USER_TOP) {
        return -EINVAL;
    }

    if (elf_size < sizeof(ELF_header_t)) {
        return -ENOEXEC;
    }

    const uint8_t *elf       = (const uint8_t *)elf_base;
    const ELF_header_t *ehdr = (const ELF_header_t *)elf_base;

    if (ehdr->magic != ELF_MAGIC || ehdr->bits != ELF_CLASS_64 || ehdr->endian != ELF_DATA_LSB
        || ehdr->isa != ELF_MACHINE_X86_64 || (ehdr->type != ELF_TYPE_EXEC && ehdr->type != ELF_TYPE_DYN)) {
        return -ENOEXEC;
    }

    if (ehdr->ph_entry_size != sizeof(ELF_program_header_t) || ehdr->ph_entries == 0
        || ehdr->ph_entries > ELF_MAX_PHDRS) {
        return -ENOEXEC;
    }

    uint64_t ph_bytes = (uint64_t)ehdr->ph_entries * sizeof(ELF_program_header_t);
    if (ehdr->ph_offset > elf_size || ph_bytes > elf_size - ehdr->ph_offset) {
        return -ENOEXEC;
    }

    if (ehdr->entry_offset >= ELF_USER_TOP - voffs_if_pic) {
        return -ENOEXEC;
    }

    printf("Loading ELF executable at base %p, voffs %p with %d program headers (phoffs = %p)\n", elf_base,
           (void *)voffs_if_pic, ehdr->ph_entries, (void *)ehdr->ph_offset);

    const ELF_program_header_t *phdrs = (const ELF_program_header_t *)(elf + ehdr->ph_offset);
    bool ehdr_mapped                  = false;

    // Work out every page range first (merging shared pages), then map each range exactly once.
    elf_range_t *ranges = kmalloc_heap(sizeof(elf_range_t) * ELF_MAX_PHDRS);
    if (!ranges) {
        return -ENOMEM;
    }
    int nranges = elf_build_ranges(elf_size, phdrs, ehdr->ph_entries, voffs_if_pic, ranges);
    if (nranges < 0) {
        kfree_heap(ranges);
        return nranges;
    }

    for (int i = 0; i < nranges; i++) {
        int res = vmalloc((void *)ranges[i].lo, ranges[i].hi - ranges[i].lo,
                          ranges[i].flags | VMALLOC_FIXED | VMALLOC_ZERO, NULL, mm);
        if (res) {
            kout(KERNEL_WARN, "loadelf(): couldn't map user range %p-%p, res %d\n", (void *)ranges[i].lo,
                 (void *)ranges[i].hi, res);
            kfree_heap(ranges);
            return res;
        }
    }

    kfree_heap(ranges);

    // Copy the file contents in. elf_copy() writes through the direct map, so read-only pages are fine.
    for (uint16_t i = 0; i < ehdr->ph_entries; i++) {
        const ELF_program_header_t *phdr = &phdrs[i];

        if (phdr->type == PT_LOAD && phdr->filesz) {
            int res = elf_copy((void *)(phdr->vaddr + voffs_if_pic), (void *)(elf + phdr->offset), phdr->filesz, mm);
            if (res) {
                return res;
            }

            // If this segment holds the start of the file, the ELF header is already mapped as part of it
            if (phdr->vaddr == 0 && phdr->offset == 0 && phdr->filesz >= sizeof(ELF_header_t)) {
                ehdr_mapped = true;
            }
        } else if (phdr->type == PT_DYNAMIC && phdr->memsz) {
            // Must lie inside a PT_LOAD (so it is already mapped); don't map it again.
            uint64_t dyn = phdr->vaddr + voffs_if_pic;
            if (phdr->vaddr >= ELF_USER_TOP - voffs_if_pic || phdr->memsz > ELF_USER_TOP - dyn || !get_physaddr(dyn, mm)
                || !get_physaddr(dyn + phdr->memsz - 1, mm)) {
                return -ENOEXEC;
            }
        }
    }

    if (!ehdr_mapped && !get_physaddr(voffs_if_pic, mm)) {
        // No segment covers the start of the file; the dynamic linker still needs to find the headers there.
        int res;
        if (res = vmalloc((void *)voffs_if_pic, PAGE_LEN, VMA_USER | VMALLOC_FIXED | VMALLOC_ZERO, NULL, mm)) {
            kout(KERNEL_WARN, "loadelf(): failed to allocate ehdr start (res %d)\n", res);
            return res;
        }

        res = elf_copy((void *)voffs_if_pic, (void *)elf, sizeof(ELF_header_t), mm);
        if (res) {
            return res;
        }
    }

    *entry_out = ehdr->entry_offset + voffs_if_pic;
    return 0;
}
