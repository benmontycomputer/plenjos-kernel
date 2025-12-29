#include "elf.h"

#include "plenjos/errno.h"
#include "plenjos/stat.h"
#include "syscall.h"

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest      = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

// TODO: figure this out
#define PAGE_LEN 4096

// TODO: let the kernel keep track of mapped memory sections and assist with picking addresses
static uint64_t next_dso_base = 0x00007f0000000000;

// TODO: read-only ELF pages, along with .dynsym and .dynstr, should be shared across processes (use file-backed
// mappings w/ memmap)

// TODO: unmap mappings on unload

int load_library_from_disk(const char *path, struct elf_object *out_obj) {
    struct elf_object obj = { 0 };

    // First, load the ELF file from the given path.
    int fd = syscall_open(path, SYSCALL_OPEN_FLAG_READ, 0);

    struct kstat stat;
    syscall_fstat(fd, &stat);

    if (stat.size <= 0) {
        // Handle error: file size is invalid
        // For simplicity, we return an empty elf_object here.
        syscall_print("loadelf: invalid file size\n");
        return -EIO;
    }

    size_t file_size = (size_t)stat.size;

    // TODO: use ulimit to determine this
    // TODO: use mmap or something when this is to large for the stack
    if (file_size >= 0x80000 /* 512 KiB */) {
        syscall_print("loadelf: ELF file too large\n");
        return -EIO;
    }

    uint8_t file_buf[file_size]; // 512 KiB buffer on stack

    ssize_t read = syscall_read(fd, file_buf, file_size);
    if (read < 0 || (size_t)read != file_size) {
        syscall_print("loadelf: failed to read entire ELF file\n");
        return -EIO;
    } else {
        syscall_print("loadelf: read ");
        syscall_print_ptr((uint64_t)file_size);
        syscall_print(" bytes from ELF file\n");
    }

    ELF_header_t *ehdr         = (ELF_header_t *)file_buf;
    ELF_program_header_t *phdr = (ELF_program_header_t *)(file_buf + ehdr->ph_offset);

    obj.ehdr          = ehdr;
    obj.phdrs         = phdr;
    obj.dynamic       = NULL; // For simplicity, we skip dynamic section parsing
    obj.dynamic_count = 0;

    int res = parse_dynamic_section(&obj);
    if (res < 0) {
        syscall_print("apply_library_relocations: failed to parse dynamic section\n");
        return -1;
    }

    // Map pt_load segments

    // Compute required contiguous virtual memory:
    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;

    for (size_t i = 0; i < ehdr->ph_entries; i++) {
        ELF_program_header_t *p = &phdr[i];

        if (p->type != PT_LOAD) continue;

        uint64_t min_vaddr_new = p->vaddr - (p->vaddr % PAGE_LEN);
        uint64_t max_vaddr_new = p->vaddr + p->memsz;

        if (max_vaddr_new % PAGE_LEN) {
            // Round up to the next page
            max_vaddr_new -= (max_vaddr_new % PAGE_LEN);
            max_vaddr_new += PAGE_LEN;
        }

        if (min_vaddr_new < min_vaddr) {
            min_vaddr = min_vaddr_new;
        }

        if (max_vaddr_new > max_vaddr) {
            max_vaddr = max_vaddr_new;
        }
    }

    uint64_t span_size = max_vaddr - min_vaddr;

    uint64_t base  = next_dso_base;
    next_dso_base += span_size + PAGE_LEN; // Leave a page gap between library mappings

    for (size_t i = 0; i < ehdr->ph_entries; i++) {
        ELF_program_header_t *p = &phdr[i];

        if (p->type != PT_LOAD) continue;

        int res = syscall_memmap_from_buffer((void *)(base + p->vaddr), (size_t)p->memsz,
                                             ((p->flags & ELF_PHDR_W) ? SYSCALL_MEMMAP_FLAG_WR : 0)
                                                 | ((p->flags & ELF_PHDR_EX) ? SYSCALL_MEMMAP_FLAG_EX : 0),
                                             (void *)(file_buf + p->offset), (size_t)p->filesz);

        if (res < 0) {
            syscall_print("load_library_from_disk: failed to map PT_LOAD segment from buffer\n");
            // TODO: unmap previously mapped segments
            return res;
        }
    }

    obj.base = base;

    // Locate dynamic section
    parse_dynamic_section(&obj);

    // Handle relocations
    if (obj.rela_sz > 0) {
        res = apply_relocations(&obj, obj.rela, obj.rela_sz, 0);
        if (res < 0) {
            syscall_print("load_library_from_disk: failed to apply relocations\n");
            return res;
        }
    }
    if (obj.rela_plt_sz > 0) {
        res = apply_relocations(&obj, obj.rela_plt, obj.rela_plt_sz, 1);
        if (res < 0) {
            syscall_print("load_library_from_disk: failed to apply PLT relocations\n");
            return res;
        }
    }

    // Add to symbol table
    // TODO

    // Run initializers
    // TODO

    return 0;

    if (out_obj) {
        *out_obj = obj;
    }

    return 0;
}

int resolve_library_path(const char *lib_name, char *out_path, size_t out_path_size) {
    // TODO: implement proper resolution (don't just search this fixed path)
    const char *lib_prefix = "/iso9660/lib/";
    size_t prefix_len      = 13; // strlen("/iso9660/lib/")

    size_t lib_name_len = 0;
    while (lib_name[lib_name_len] != '\0') {
        lib_name_len++;
    }

    if (prefix_len + lib_name_len + 1 > out_path_size) {
        return -ENAMETOOLONG;
    }

    // Construct the full path
    for (size_t i = 0; i < prefix_len; i++) {
        out_path[i] = lib_prefix[i];
    }
    for (size_t i = 0; i <= lib_name_len; i++) {
        out_path[prefix_len + i] = lib_name[i];
    }

    return 0;
}

int parse_dynamic_section(struct elf_object *obj) {
    ELF_header_t *ehdr          = obj->ehdr;
    ELF_program_header_t *phdrs = obj->phdrs;

    // Find PT_DYNAMIC segment
    for (size_t i = 0; i < ehdr->ph_entries; i++) {
        ELF_program_header_t *phdr = &phdrs[i];

        if (phdr->type == PT_DYNAMIC) {
            syscall_print("parse_dynamic_section: found PT_DYNAMIC segment at vaddr ");
            syscall_print_ptr((uint64_t)phdr->vaddr);
            syscall_print("\n");

            obj->dynamic       = (ELF_dyn_t *)(phdr->vaddr);
            obj->dynamic_count = phdr->memsz / sizeof(ELF_dyn_t);
            break;
        }
    }

    if (!obj->dynamic) {
        syscall_print("parse_dynamic_section: no PT_DYNAMIC segment found, skipping relocations\n");
        return 0;
    }

    ELF_dyn_t *dynamic = obj->dynamic;
    for (size_t i = 0; i < obj->dynamic_count; i++) {
        ELF_dyn_t *d = &dynamic[i];

        // For simplicity, we just print out the dynamic entries here.
        syscall_print("apply_relocations: Dynamic Entry ");
        syscall_print_ptr((uint64_t)i);
        syscall_print(": d_tag=");
        syscall_print_ptr((uint64_t)d->d_tag);
        syscall_print(", d_un.d_val=");
        syscall_print_ptr((uint64_t)d->d_un.d_val);
        syscall_print(", d_un.d_ptr=");
        syscall_print_ptr((uint64_t)d->d_un.d_ptr);
        syscall_print("\n");

        switch (d->d_tag) {
        case DT_NEEDED: {
            // Handle needed library
            syscall_print("parse_dynamic_section: DT_NEEDED entry found\n");

            // TODO: should this be larger?
            char lib_path[512];
            int res = resolve_library_path((const char *)d->d_un.d_ptr, lib_path, sizeof(lib_path));
            if (res < 0) {
                syscall_print("parse_dynamic_section: failed to resolve library path\n");
                return -1;
            }

            syscall_print("parse_dynamic_section: resolved library path: ");
            syscall_print(lib_path);
            syscall_print("\n");

            struct elf_object lib_obj;
            res = load_library_from_disk(lib_path, &lib_obj);
            if (res < 0) {
                syscall_print("parse_dynamic_section: failed to load needed library\n");
                return -1;
            }

            break;
        }
        case DT_NULL: {
            syscall_print("parse_dynamic_section: reached DT_NULL, ending dynamic section parsing\n");
            return 0;
        }
        case DT_REL: {
            syscall_print("parse_dynamic_section: DT_REL not supported, only DT_RELA\n");
            return -1;
        }
        case DT_RELA: {
            obj->rela = (ELF_rela_t *)(obj->base + d->d_un.d_ptr);
            break;
        }
        case DT_RELASZ: {
            obj->rela_sz = d->d_un.d_val;
            break;
        }
        case DT_JMPREL: {
            obj->rela_plt = (ELF_rela_t *)(obj->base + d->d_un.d_ptr);
            break;
        }
        case DT_PLTREL: {
            // Whether the PLT relocations are RELA or REL
            if (d->d_un.d_val != DT_RELA) {
                syscall_print("parse_dynamic_section: DT_PLTREL indicates unsupported relocation type\n");
                return -1;
            }
            break;
        }
        case DT_PLTRELSZ: {
            obj->rela_plt_sz = d->d_un.d_val;
            break;
        }
        case DT_RELAENT: {
            // Size of each rela entry; should be sizeof(ELF_rela_t) but not always
            obj->relaent = d->d_un.d_val;
            break;
        }
        case DT_PLTGOT: {
            obj->pltgot = obj->base + d->d_un.d_ptr;
            break;
        }
        case DT_SYMTAB: {
            obj->symtab = (ELF_sym_t *)(obj->base + d->d_un.d_ptr);
            break;
        }
        // TODO: check for DT_STRSZ and DT_SYMENT?
        case DT_STRTAB: {
            obj->strtab = (const char *)(obj->base + d->d_un.d_ptr);
            break;
        }
        // DT_INIT, DT_INIT_ARRAY, DT_FINI, DT_FINI_ARRAY could be handled here for initializers/finalizers
        default: {
            syscall_print("apply_relocations: unhandled dynamic tag ");
            syscall_print_ptr((uint64_t)d->d_tag);
            syscall_print("\n");
            break;
        }
        }
    }

    if (obj->relaent == 0) {
        syscall_print("parse_dynamic_section: DT_RELAENT not found, defaulting to sizeof(ELF_rela_t)\n");
        obj->relaent = sizeof(ELF_rela_t);
    }

    return 0;
}

ELF_addr_t resolve_symbol(const char *name) {
    // Placeholder
    syscall_print("resolve_symbol: resolving symbol: ");
    syscall_print(name);
    syscall_print("\n");
    return 0;
}

// This is for both the main executable and shared libraries. This should happen AFTER it's loaded into memory.
int apply_relocations(struct elf_object *obj, ELF_rela_t *rela, size_t rela_sz, int write_got) {
    syscall_print("apply_relocations: applying relocations\n");

    ELF_addr_t base      = obj->base;
    ELF_sym_t *symtab    = obj->symtab;
    const char *strtab   = obj->strtab;
    size_t relaent       = obj->relaent;

    if (!rela || rela_sz == 0) {
        syscall_print("apply_relocations: no rela table found\n");
        return 0;
    }

    size_t count = rela_sz / relaent;
    for (size_t i = 0; i < count; i++) {
        ELF_rela_t *r     = &rela[i];
        ELF_addr_t *where = (ELF_addr_t *)(base + r->r_offset);

        // https://docs.oracle.com/cd/E19120-01/open.solaris/819-0690/chapter7-2/index.html
        // https://docs.oracle.com/cd/E19120-01/open.solaris/819-0690/chapter6-54839/index.html
        // S = value of symbol whose index resides in relocation entry
        // A = addend
        // B = base address of shared object
        switch (ELF64_R_TYPE(r->r_info)) {
        case (R_X86_64_64): {
            // S + A
            uint32_t sym_index = ELF64_R_SYM(r->r_info);
            ELF_sym_t *sym     = &symtab[sym_index];

            const char *name = strtab + sym->st_name;
            syscall_print("apply_relocations: resolving symbol: ");
            syscall_print(name);
            syscall_print("\n");

            ELF_addr_t addr = resolve_symbol(name);

            if (addr == 0) {
                syscall_print("ERROR: Couldn't resolve symbol ");
                syscall_print(name);
                syscall_print("!\n");
                return -1;
            }

            *where = addr + r->r_addend;

            break;
        }
        // These next two cases resolve to the same calculation
        case (R_X86_64_GLOB_DAT):
        case (R_X86_64_JUMP_SLOT): {
            // S
            uint32_t sym_index = ELF64_R_SYM(r->r_info);
            ELF_sym_t *sym     = &symtab[sym_index];

            const char *name = strtab + sym->st_name;
            syscall_print("apply_relocations: resolving symbol: ");
            syscall_print(name);
            syscall_print("\n");

            ELF_addr_t addr = resolve_symbol(name);

            if (addr == 0) {
                syscall_print("ERROR: Couldn't resolve symbol ");
                syscall_print(name);
                syscall_print("!\n");
                return -1;
            }

            *where = addr;

            if (write_got && obj->pltgot && ELF64_R_TYPE(r->r_info) == R_X86_64_JUMP_SLOT) {
                // Also write to the GOT entry
                ELF_addr_t *got_entry = (ELF_addr_t *)(base + r->r_offset); //(obj->pltgot + (r->r_offset - (obj->rela_plt ? (uint64_t)obj->rela_plt : 0)));
                *got_entry            = addr;
            }

            break;
        }
        case (R_X86_64_RELATIVE): {
            // B + A
            *where = r->r_addend + base;
            break;
        }
        default: {
            syscall_print("WARN: unrecognized relocation type ");
            syscall_print_ptr(ELF64_R_TYPE(r->r_info));
            syscall_print("\n");

            return -1;
        }
        }
    }

    return 0;
}