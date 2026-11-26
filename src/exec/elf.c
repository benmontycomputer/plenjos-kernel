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

// Validates one PT_LOAD / PT_DYNAMIC program header against the file and the user address range, maps the pages it
// covers and copies its file contents in.
static int elf_load_segment(const uint8_t *elf, size_t elf_size, const ELF_program_header_t *phdr, mm_t *mm,
                            uint64_t voffs, bool *ehdr_mapped) {
    if (phdr->memsz == 0) {
        return 0;
    }

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

    uint64_t real_start = start & ~(uint64_t)(PAGE_LEN - 1);
    uint64_t real_len   = (phdr->memsz + PAGE_LEN - 1) & ~(uint64_t)(PAGE_LEN - 1);

    vma_flags_t flags = ((phdr->flags & ELF_PHDR_W) ? VMA_RW : 0) | ((phdr->flags & ELF_PHDR_EX) ? VMA_EXEC : 0)
                        | VMA_USER | VMALLOC_FIXED;
    int res;
    if (res = vmalloc((void *)real_start, real_len, flags, NULL, mm)) {
        kout(KERNEL_WARN, "loadelf(): couldn't map user page at %p, res %d\n", (void *)real_start, res);
        return res;
    }

    if (phdr->filesz) {
        int res = elf_copy((void *)start, (void *)(elf + phdr->offset), phdr->filesz, mm);
        if (res) {
            return res;
        }
    }

    // If this segment holds the start of the file, the ELF header is already mapped as part of it
    if (phdr->vaddr == 0 && phdr->offset == 0 && phdr->filesz >= sizeof(ELF_header_t)) {
        *ehdr_mapped = true;
    }

    return 0;
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

    for (uint16_t i = 0; i < ehdr->ph_entries; i++) {
        const ELF_program_header_t *phdr = &phdrs[i];

        // PT_DYNAMIC normally lies inside a PT_LOAD segment that was already loaded; elf_map_page() and elf_copy()
        // are idempotent, so handling it here too is harmless, and still covers a dynamic section outside any PT_LOAD.
        if (phdr->type != PT_LOAD && phdr->type != PT_DYNAMIC) {
            continue;
        }

        int res = elf_load_segment(elf, elf_size, phdr, mm, voffs_if_pic, &ehdr_mapped);
        if (res) {
            return res;
        }
    }

    if (!ehdr_mapped && !get_physaddr(voffs_if_pic, mm)) {
        // No segment covers the start of the file; the dynamic linker still needs to find the headers there.
        int res;
        if (res = vmalloc((void *)voffs_if_pic, PAGE_LEN, VMA_USER, NULL, mm)) {
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
