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
static uint64_t next_dso_base;
static uint64_t dso_count;
static struct elf_object loaded_dsos[48];

void init_dso_base() {
    next_dso_base  = 0x00007f0000000000ULL;
    dso_count      = 0;
    loaded_dsos[0] = (struct elf_object) { 0 };
}

// TODO: read-only ELF pages, along with .dynsym and .dynstr, should be shared across processes (use file-backed
// mappings w/ memmap)

// TODO: unmap mappings on unload

int load_library_from_disk(const char *path, uint64_t *out_index) {
    // Linker is single-threaded for now; no locking needed. We will increment dso_count when done.

    struct elf_object *obj = &loaded_dsos[dso_count];
    *obj                   = (struct elf_object) { 0 };

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

    obj->ehdr          = ehdr;
    obj->phdrs         = phdr;
    obj->dynamic       = NULL;
    obj->dynamic_count = 0;

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

        // Map with full permissions; we will adjust later if needed
        int res = syscall_memmap_from_buffer((void *)(base + p->vaddr), (size_t)p->memsz,
                                             SYSCALL_MEMMAP_FLAG_WR | SYSCALL_MEMMAP_FLAG_EX,
                                             (void *)(file_buf + p->offset), (size_t)p->filesz);

        if (res < 0) {
            syscall_print("load_library_from_disk: failed to map PT_LOAD segment from buffer\n");
            // TODO: unmap previously mapped segments
            return res;
        }
    }

    obj->base = base;
    syscall_print("load_library_from_disk: mapped library at base ");
    syscall_print_ptr((uint64_t)obj->base);
    syscall_print("\n");

    // Locate dynamic section
    int res = parse_dynamic_section(obj);
    if (res < 0) {
        syscall_print("load_library_from_disk: failed to parse dynamic section\n");
        return res;
    }

    // This is needed so relocations can find symbols within this DSO
    dso_count++;

    // Handle relocations
    if (obj->rela_sz > 0) {
        syscall_print("\nload_library_from_disk: applying ");
        syscall_print_ptr((uint64_t)(obj->rela_sz / sizeof(ELF_rela_t)));
        syscall_print(" rela relocations\n");
        res = apply_relocations(obj, obj->rela, obj->rela_sz, 0);
        if (res < 0) {
            syscall_print("load_library_from_disk: failed to apply relocations\n");
            dso_count--;
            return res;
        }
    }
    if (obj->rela_plt_sz > 0) {
        syscall_print("\nload_library_from_disk: applying ");
        syscall_print_ptr((uint64_t)(obj->rela_plt_sz / sizeof(ELF_rela_t)));
        syscall_print(" rela_plt relocations\n");
        res = apply_relocations(obj, obj->rela_plt, obj->rela_plt_sz, 1);
        if (res < 0) {
            syscall_print("load_library_from_disk: failed to apply PLT relocations\n");
            dso_count--;
            return res;
        }
    }

    // Relocations are done; now we can remap segments with proper permissions
    for (size_t i = 0; i < ehdr->ph_entries; i++) {
        ELF_program_header_t *p = &phdr[i];

        if (p->type != PT_LOAD) continue;

        // Map with full permissions; we will adjust later if needed
        int res = syscall_memprotect((void *)(base + p->vaddr), (size_t)p->memsz,
                                     ((p->flags & ELF_PHDR_W) ? SYSCALL_MEMMAP_FLAG_WR : 0)
                                         | ((p->flags & ELF_PHDR_EX) ? SYSCALL_MEMMAP_FLAG_EX : 0));

        if (res < 0) {
            syscall_print("load_library_from_disk: failed to apply permissions to PT_LOAD segment from buffer\n");
            // TODO: unmap previously mapped segments
            return res;
        }
    }

    // Add to symbol table
    // (already done by being in loaded_dsos array)

    // Run initializers
    // TODO

    if (out_index) {
        *out_index = dso_count;
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
            /* syscall_print("parse_dynamic_section: found PT_DYNAMIC segment at vaddr ");
            syscall_print_ptr((uint64_t)(obj->base + phdr->vaddr));
            syscall_print(" w/ obj base ");
            syscall_print_ptr((uint64_t)obj->base);
            syscall_print("\n"); */

            obj->dynamic       = (ELF_dyn_t *)(obj->base + phdr->vaddr);
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
        /* syscall_print("parse_dynamic_section: Dynamic Entry ");
        syscall_print_ptr((uint64_t)i);
        syscall_print(": d_tag=");
        syscall_print_ptr((uint64_t)d->d_tag);
        syscall_print(", d_un.d_val=");
        syscall_print_ptr((uint64_t)d->d_un.d_val);
        syscall_print(", d_un.d_ptr=");
        syscall_print_ptr((uint64_t)d->d_un.d_ptr);
        syscall_print("\n"); */

        switch (d->d_tag) {
        case DT_NULL: {
            syscall_print("parse_dynamic_section: reached DT_NULL, ending dynamic section parsing\n");
            goto done_parsing;
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
            syscall_print("parse_dynamic_section: unhandled dynamic tag ");
            syscall_print_ptr((uint64_t)d->d_tag);
            syscall_print("\n");
            break;
        }
        }
    }

done_parsing:

    // Go over this again to parse DT_NEEDED
    for (size_t i = 0; i < obj->dynamic_count; i++) {
        ELF_dyn_t *d = &dynamic[i];

        if (d->d_tag == DT_NEEDED) {
            // Handle needed library
            syscall_print("parse_dynamic_section: DT_NEEDED entry found for ");
            syscall_print((const char *)(obj->strtab + d->d_un.d_val));
            syscall_print("\n");

            // TODO: should this be larger?
            char lib_path[512];
            int res = resolve_library_path((const char *)(obj->strtab + d->d_un.d_val), lib_path, sizeof(lib_path));
            if (res < 0) {
                syscall_print("parse_dynamic_section: failed to resolve library path\n");
                return -1;
            }

            syscall_print("parse_dynamic_section: resolved library path: ");
            syscall_print(lib_path);
            syscall_print("\n");

            // TODO: check if already loaded

            uint64_t lib_index = (uint64_t)-1;
            res                = load_library_from_disk(lib_path, &lib_index);
            if (res < 0) {
                syscall_print("parse_dynamic_section: failed to load needed library\n");
                return -1;
            }
        }
    }

    if (obj->relaent == 0) {
        syscall_print("WARN: parse_dynamic_section: DT_RELAENT not found, defaulting to sizeof(ELF_rela_t)\n");
        obj->relaent = sizeof(ELF_rela_t);
    } else {
        syscall_print("parse_dynamic_section: DT_RELAENT = ");
        syscall_print_ptr((uint64_t)obj->relaent);
        syscall_print("\n");
    }

    return 0;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) {
            return (unsigned char)*s1 - (unsigned char)*s2;
        }
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

ELF_addr_t resolve_symbol(const char *name) {
    // Placeholder
    /* syscall_print("resolve_symbol: resolving symbol: ");
    syscall_print(name);
    syscall_print("\n"); */

    for (uint64_t i = 0; i < dso_count; i++) {
        struct elf_object *obj = &loaded_dsos[i];

        /* syscall_print("resolve_symbol: searching in DSO ");
        syscall_print_ptr(i);
        syscall_print(" at base ");
        syscall_print_ptr((uint64_t)obj->base);
        syscall_print("\n"); */

        ELF_sym_t *symtab  = obj->symtab;
        const char *strtab = obj->strtab;

        if (!symtab || !strtab) {
            continue;
        }

        // Assume up to 1024 symbols per object for now
        // TODO: use something like GNU hash map to speed this up and also determine actual symbol count
        for (size_t j = 0; j < 1024; j++) {
            ELF_sym_t *sym = &symtab[j];

            if (sym->st_name == 0) {
                continue; // Empty name
            }

            const char *sym_name = strtab + sym->st_name;

            if (strcmp(sym_name, name) == 0) {
                // Found the symbol
                ELF_addr_t addr = obj->base + sym->st_value;
                /* syscall_print("resolve_symbol: found symbol ");
                syscall_print(sym_name);
                syscall_print(" at address ");
                syscall_print_ptr((uint64_t)addr);
                syscall_print("\n"); */
                return addr;
            }
        }
    }

    return 0;
}