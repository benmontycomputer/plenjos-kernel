#include "ldso.h"

#include "elf.h"
#include "syscall.h"

void _start(uint64_t stack) {
    // Get the stack pointer
    // uint64_t rsp;
    // asm volatile("mov %%rsp, %0" : "=r"(rsp));

    /* uint64_t base = (uint64_t)0x7fff80000000ULL;

    // Syscall print ptr on base
    asm volatile("mov %0, %%rbx\n"
                 "mov $68, %%rax\n"
                 "int $0x80\n"
                 :
                 : "r"(base)
                 : "%rax", "%rdi");

    ELF_header_t *ehdr         = (ELF_header_t *)base;
    ELF_program_header_t *phdr = (ELF_program_header_t *)(base + ehdr->ph_offset);

    asm volatile("mov %0, %%rbx\n"
                 "mov $68, %%rax\n"
                 "int $0x80\n"
                 :
                 : "r"((uint64_t)ehdr->ph_entries)
                 : "%rax", "%rdi");
    asm volatile("mov %0, %%rbx\n"
                 "mov $68, %%rax\n"
                 "int $0x80\n"
                 :
                 : "r"(ehdr->ph_offset)
                 : "%rax", "%rdi");

    ELF_dyn_t *dyn = 0;

    // Find PT_DYNAMIC segment
    for (int i = 0; i < ehdr->ph_entries; i++) {
        asm volatile("mov %0, %%rbx\n"
                 "mov $68, %%rax\n"
                 "int $0x80\n"
                 :
                 : "r"(base)
                 : "%rax", "%rdi");
        if (phdr[i].type == PT_DYNAMIC) {
            dyn = (ELF_dyn_t *)(base + phdr[i].vaddr);
            break;
        }
    }

    if (!dyn) goto finish_relocations;

    ELF_rela_t *rela = 0;
    uint64_t rela_sz = 0;

    // Locate relocation table
    for (ELF_dyn_t *d = dyn; d->d_tag != DT_NULL; d++) {
        if (d->d_tag == DT_RELA) rela = (ELF_rela_t *)(base + d->d_un.d_ptr);
        else if (d->d_tag == DT_RELASZ) rela_sz = d->d_un.d_val;
    }

    if (!rela || rela_sz == 0) goto finish_relocations;

    // Apply RELATIVE relocations
    uint64_t count = rela_sz / sizeof(ELF_rela_t);
    for (uint64_t i = 0; i < count; i++) {
        if (ELF64_R_TYPE(rela[i].r_info) == R_X86_64_RELATIVE) {
            *(uintptr_t *)(base + rela[i].r_offset) = base + rela[i].r_addend;
        }
    } */

finish_relocations:
    syscall_print("ldso: Finished relocations, jumping to main executable...\nargv0: ");
    ld_main(stack);
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

int ld_main(uint64_t stack) {
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

    struct elf_object main_obj;
    loadelf(argv[0], &main_obj);

    return 0;
}