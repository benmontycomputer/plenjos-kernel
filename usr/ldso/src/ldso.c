#include "ldso.h"

#include "elf.h"
#include "syscall.h"

extern uint64_t next_dso_base;

static int ld_main(uint64_t stack, uint64_t target_ehdr);

static int _strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) {
            return (unsigned char)*s1 - (unsigned char)*s2;
        }
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

static ELF_addr_t _ldso_self_resolve_symbol(const char *name, ELF_sym_t *symtab, const char *strtab, uint64_t base) {
    // Assume up to 1024 symbols for now
    // TODO: use something like GNU hash map to speed this up and also determine actual symbol count
    for (size_t j = 0; j < 1024; j++) {
        ELF_sym_t *sym = &symtab[j];

        if (sym->st_name == 0) {
            continue; // Empty name
        }

        const char *sym_name = strtab + sym->st_name;

        if (_strcmp(sym_name, name) == 0) {
            // Found the symbol
            ELF_addr_t addr = sym->st_value + base;
            return addr;
        }
    }

    return 0;
}

static uint64_t _prereloc_syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi) {
    uint64_t out;

    asm volatile("mov %[_rax], %%rax\n"
                 "mov %[_rbx], %%rbx\n"
                 "mov %[_rcx], %%rcx\n"
                 "mov %[_rdx], %%rdx\n"
                 "mov %[_rsi], %%rsi\n"
                 "mov %[_rdi], %%rdi\n"
                 "int $0x80\n"
                 "mov %%rax, %[_out]\n"
                 : [_out] "=r"(out)
                 : [_rax] "r"(rax), [_rbx] "r"(rbx), [_rcx] "r"(rcx), [_rdx] "r"(rdx), [_rsi] "r"(rsi), [_rdi] "r"(rdi)
                 : "rax", "rbx", "rcx", "rdx", "rsi", "rdi" // Clobbered registers
    );

    return out;
}

static int _prereloc_syscall_print(const char *str) {
    return (int)_prereloc_syscall(SYSCALL_PRINT, (uint64_t)str, 0, 0, 0, 0);
}

static int _prereloc_syscall_print_ptr(uint64_t val) {
    return (int)_prereloc_syscall(SYSCALL_PRINT_PTR, (uint64_t)val, 0, 0, 0, 0);
}

// This is for both the main executable and shared libraries. This should happen AFTER it's loaded into memory.
static int _apply_relocations(ELF_addr_t base, ELF_sym_t *symtab, const char *strtab, size_t relaent, uint64_t pltgot, ELF_rela_t *rela, size_t rela_sz, int is_jmprel, int use_linker_resolve_symbol) {
    // _prereloc_syscall_print("apply_relocations: applying relocations\n");

    if (!rela || rela_sz == 0) {
        // _prereloc_syscall_print("apply_relocations: no rela table found\n");
        return 0;
    }

    size_t count = rela_sz / relaent;
    for (size_t i = 0; i < count; i++) {
        ELF_rela_t *r     = (ELF_rela_t *)((uint8_t *)rela + i * relaent);
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
            /* _prereloc_syscall_print("apply_relocations: resolving R_X86_64_64 symbol: ");
            _prereloc_syscall_print(name);
            _prereloc_syscall_print("\n"); */

            ELF_addr_t addr = use_linker_resolve_symbol ? _ldso_self_resolve_symbol(name, symtab, strtab, base) : resolve_symbol(name);

            if (addr == 0) {
                _prereloc_syscall_print("ERROR: Couldn't resolve symbol ");
                _prereloc_syscall_print(name);
                _prereloc_syscall_print("!\n");
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
            /* _prereloc_syscall_print("apply_relocations: resolving R_X86_64_GLOB_DAT or R_X86_64_JUMP_SLOT symbol: ");
            _prereloc_syscall_print(name);
            _prereloc_syscall_print("\n"); */

            ELF_addr_t addr = use_linker_resolve_symbol ? _ldso_self_resolve_symbol(name, symtab, strtab, base) : resolve_symbol(name);

            if (addr == 0) {
                _prereloc_syscall_print("ERROR: Couldn't resolve symbol ");
                _prereloc_syscall_print(name);
                _prereloc_syscall_print("!\n");
                return -1;
            }

            *where = addr;

            if (is_jmprel && pltgot && ELF64_R_TYPE(r->r_info) == R_X86_64_JUMP_SLOT) {
                // Also write to the GOT entry
                ELF_addr_t *got_entry
                    = (ELF_addr_t *)(base + r->r_offset); //(obj->pltgot + (r->r_offset - (obj->rela_plt ?
                                                          //(uint64_t)obj->rela_plt : 0)));
                *got_entry = addr;
            } else if (!is_jmprel && pltgot && ELF64_R_TYPE(r->r_info) == R_X86_64_GLOB_DAT) {
                // Also write to the GOT entry
                ELF_addr_t *got_entry
                    = (ELF_addr_t *)(base + r->r_offset); //(obj->pltgot + (r->r_offset - (obj->rela_plt ?
                                                          //(uint64_t)obj->rela_plt : 0)));
                *got_entry = addr;
            }

            break;
        }
        case (R_X86_64_RELATIVE): {
            // B + A
            *where = r->r_addend + base;
            break;
        }
        default: {
            _prereloc_syscall_print("ERROR: unrecognized relocation type ");
            _prereloc_syscall_print_ptr(ELF64_R_TYPE(r->r_info));
            _prereloc_syscall_print(" at index ");
            _prereloc_syscall_print_ptr(i);
            _prereloc_syscall_print(" (relatab =");
            _prereloc_syscall_print_ptr((uint64_t)rela);
            _prereloc_syscall_print(", r =");
            _prereloc_syscall_print_ptr((uint64_t)r);
            _prereloc_syscall_print(", count = ");
            _prereloc_syscall_print_ptr(count);
            _prereloc_syscall_print(", rela_sz = ");
            _prereloc_syscall_print_ptr(rela_sz);
            _prereloc_syscall_print(")\n");

            return -1;
        }
        }
    }

    return 0;
}

int apply_relocations(struct elf_object *obj, ELF_rela_t *rela, size_t rela_sz, int is_jmprel) {
    return _apply_relocations(obj->base, obj->symtab, obj->strtab, obj->relaent, obj->pltgot, rela, rela_sz, is_jmprel, 0);
}

void _start(uint64_t stack, uint64_t linker_ehdr, uint64_t target_ehdr) {
    // First, we must self-relocate. Until we do this, we can only call functions defined statically in this file.

    // Start by parsing the dynamic header
    ELF_header_t *ehdr          = (ELF_header_t *)linker_ehdr;
    ELF_program_header_t *phdrs = (ELF_program_header_t *)((uint64_t)linker_ehdr + ehdr->ph_offset);

    ELF_dyn_t *dynamic = NULL;

    for (uint16_t i = 0; i < ehdr->ph_entries; i++) {
        ELF_program_header_t *phdr = &phdrs[i];

        if (phdr->type == PT_DYNAMIC) {
            // Allow us to map
            dynamic = (ELF_dyn_t *)(linker_ehdr + phdr->vaddr);
            break;
        }
    }

    if (!dynamic) {
        syscall_print("ldso: no PT_DYNAMIC segment found in linker, cannot self-relocate (not needed).\n");
        goto finish_relocations;
    }

    // Parse dynamic entries
    ELF_rela_t *rela     = NULL;
    size_t rela_sz       = 0;
    size_t relaent       = 0;
    ELF_rela_t *rela_plt = NULL;
    size_t rela_plt_sz   = 0;
    uint64_t pltgot      = 0;
    ELF_sym_t *symtab    = NULL;
    const char *strtab   = NULL;

    for (size_t i = 0;; i++) {
        ELF_dyn_t *d = &dynamic[i];

        if (d->d_tag == DT_NULL) {
            break;
        }

        switch (d->d_tag) {
        case DT_RELA: {
            rela = (ELF_rela_t *)(linker_ehdr + d->d_un.d_ptr);
            break;
        }
        case DT_RELASZ: {
            rela_sz = d->d_un.d_val;
            break;
        }
        case DT_JMPREL: {
            rela_plt = (ELF_rela_t *)(linker_ehdr + d->d_un.d_ptr);
            break;
        }
        case DT_PLTRELSZ: {
            rela_plt_sz = d->d_un.d_val;
            break;
        }
        case DT_RELAENT: {
            // Size of each rela entry; should be sizeof(ELF_rela_t) but not always
            relaent = d->d_un.d_val;
            break;
        }
        case DT_PLTGOT: {
            pltgot = linker_ehdr + d->d_un.d_ptr;
            break;
        }
        case DT_SYMTAB: {
            symtab = (ELF_sym_t *)(linker_ehdr + d->d_un.d_ptr);
            break;
        }
        case DT_STRTAB: {
            strtab = (const char *)(linker_ehdr + d->d_un.d_ptr);
            break;
        }
        default:
            break;
        }
    }

    if (relaent == 0) {
        relaent = sizeof(ELF_rela_t);
    }

    _prereloc_syscall_print("\nldso: Starting self-relocations...\n");
    _prereloc_syscall_print("rela = ");
    _prereloc_syscall_print_ptr((uint64_t)rela);
    _prereloc_syscall_print(", rela_sz = ");
    _prereloc_syscall_print_ptr((uint64_t)rela_sz);
    _prereloc_syscall_print(", relaent = ");
    _prereloc_syscall_print_ptr((uint64_t)relaent);
    _prereloc_syscall_print("\nrela_plt = ");
    _prereloc_syscall_print_ptr((uint64_t)rela_plt);
    _prereloc_syscall_print(", rela_plt_sz = ");
    _prereloc_syscall_print_ptr((uint64_t)rela_plt_sz);
    _prereloc_syscall_print("\npltgot = ");
    _prereloc_syscall_print_ptr((uint64_t)pltgot);
    _prereloc_syscall_print("\nsymtab = ");
    _prereloc_syscall_print_ptr((uint64_t)symtab);
    _prereloc_syscall_print(", strtab = ");
    _prereloc_syscall_print_ptr((uint64_t)strtab);
    _prereloc_syscall_print("\n");

    // Apply relocations
    if (rela_sz > 0) {
        _apply_relocations(linker_ehdr, symtab, strtab, relaent, pltgot, rela, rela_sz, 1, 1);
    }

    if (rela_plt_sz > 0) {
        _apply_relocations(linker_ehdr, symtab, strtab, relaent, pltgot, rela_plt, rela_plt_sz, 1, 1);
    }

    _prereloc_syscall_print("ldso: Self-relocations complete.\ninit_dso_base resolves to ");
    _prereloc_syscall_print_ptr((uint64_t)init_dso_base);
    _prereloc_syscall_print("\n");

finish_relocations:
    init_dso_base();
    syscall_print("ldso: Finished relocations, jumping to main executable... (ehdr ");
    syscall_print_ptr(target_ehdr);
    syscall_print(" stack ");
    syscall_print_ptr(stack);
    syscall_print(")\n");
    ld_main(stack, target_ehdr);
}

void jump_to_entry(uint64_t entry, uint64_t stack) {
    // Jump to the entry point
    asm volatile("mov %0, %%rsp\n"
                 "push %1\n"
                 "ret\n"
                 :
                 : "r"(stack), "r"(entry)
                 : "%rsp");
}

/**
 * Stack format:
 *  0x00: argc (uint64_t)
 *  0x08: argv[0] (char *)
 *  0x10: argv[1] (char *)
 *  ...
 *  [argc * 8]: NULL (char *)
 *  [argc * 8 + 0x08]: envp[0] (char *)
 *  ...
 *
 * Both argv and envp are NULL-terminated arrays of pointers.
 *
 * Alternatively, char **argv = (char **)(stack + 8);
 * char **envp = (char **)(stack + 8 + (argc + 1) * 8);
 * * * * *
 * ld_main() details:
 * - Responsible for loading the ELF executable specified by argv[0]
 * - Responsible for applying relocations and loading any necessary shared libraries
 * - Finally, it should transfer control to the entry point of the loaded executable
 * - Note: ld_main() is NOT responsible for setting up the initial stack frame for the executable;
 *   that is handled by the caller of ld_main().
 *
 * The kernel is responsible for making sure we have permissions to execute the main executable and for making sure it
 * is a regular file.
 */

static int ld_main(uint64_t stack, uint64_t target_ehdr) {
    uint64_t argc = *((uint64_t *)stack);
    char **argv   = (char **)((uint8_t *)stack + 8);
    char **envp   = (char **)((uint8_t *)argv + (argc + 1) * 8);

    if (argc == 0) {
        // No executable specified
        syscall_print("ldso: No executable specified in argv[0]\n");
        return -1;
    } else {
        syscall_print("ldso: Loading executable:\n(Stack: ");
        syscall_print_ptr(stack);
        syscall_print("): \n");
        syscall_print(argv[-1] ? argv[-1] : "(null)");
        syscall_print(" ");
        syscall_print(argv[0] ? argv[0] : "(null)");
        syscall_print(" ");
        syscall_print(argv[1] ? argv[1] : "(null)");
        syscall_print("\n");
    }

    struct elf_object main_obj = { 0 };
    // loadelf(argv[0], &main_obj);

    main_obj.base  = 0x400000; // Typical base for executables
    main_obj.ehdr  = (ELF_header_t *)target_ehdr;
    main_obj.phdrs = (ELF_program_header_t *)((uint8_t *)target_ehdr + main_obj.ehdr->ph_offset);

    parse_dynamic_section(&main_obj);

    int res = 0;

    // Handle relocations
    if (main_obj.rela_sz > 0) {
        syscall_print("\nldso: applying ");
        syscall_print_ptr((uint64_t)(main_obj.rela_sz / sizeof(ELF_rela_t)));
        syscall_print(" rela relocations\n");
        res = apply_relocations(&main_obj, main_obj.rela, main_obj.rela_sz, 0);
        if (res < 0) {
            syscall_print("load_library_from_disk: failed to apply relocations\n");
            return res;
        }
    }
    if (main_obj.rela_plt_sz > 0) {
        syscall_print("\nldso: applying ");
        syscall_print_ptr((uint64_t)(main_obj.rela_plt_sz / sizeof(ELF_rela_t)));
        syscall_print(" rela_plt relocations\n");
        res = apply_relocations(&main_obj, main_obj.rela_plt, main_obj.rela_plt_sz, 1);
        if (res < 0) {
            syscall_print("load_library_from_disk: failed to apply PLT relocations\n");
            return res;
        }
    }

    // Resolve symbols (populate GOT and PLT)
    // TODO

    // Jump to the entry point

    uint64_t entry = main_obj.ehdr->entry_offset + main_obj.base;
    syscall_print("ldso: Jumping to entry point at ");
    syscall_print_ptr(entry);
    syscall_print(" with stack ");
    syscall_print_ptr(stack);
    syscall_print("\n");

    jump_to_entry(entry, stack);

    while (1);

    return 0;
}