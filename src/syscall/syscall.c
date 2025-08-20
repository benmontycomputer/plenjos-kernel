#include <stdint.h>

#include "syscall/syscall.h"

#include "memory/mm.h"
#include "memory/mm_common.h"

#include "kernel.h"

#include "lib/stdio.h"
#include "lib/string.h"

#include "arch/x86_64/irq.h"

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi) {
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

extern char *fb;
extern int fb_scanline,fb_width,fb_height,fb_bytes_per_pixel;

registers_t *syscall_routine(registers_t *regs) {
    uint64_t call = regs->rax;

    switch (call) {
    case SYSCALL_GET_FB:
        // Map the framebuffer into the currently loaded page table
        pml4_t *current_pml4 = (pml4_t *)get_cr3_addr();
        // TODO: check if framebuffer is in use
        map_virtual_memory(virt_to_phys((uint64_t)fb), fb_scanline * fb_height, PAGE_FLAG_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITE, current_pml4);

        regs->rax = (uint64_t)fb;
        break;
    case SYSCALL_GET_KB:
        break;
    case SYSCALL_PRINT:
        // map_virtual_memory(virt_to_phys((uint64_t)fb), fb_scanline * fb_height, PAGE_FLAG_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITE, current_pml4);
        uint64_t str_ptr = regs->rbx;

        size_t len = strlen((const char *)str_ptr);

        bool valid = true;

        for (uint64_t i = str_ptr & ~0xFFF; i < str_ptr + len; i += PAGE_LEN) {
            if (!get_physaddr(i)) {
                valid = false;
                break;
            }
        }

        if (valid) {
            uint64_t str_phys = get_physaddr(str_ptr);

            // TODO: switch to kernel pml4

            const char *str = (const char *)phys_to_virt(str_phys);

            printf("%s", str);

            // TODO: back to user pml4
        }

        break;
    case SYSCALL_READ:
        break;
    default:
        break;
    }

    return regs;
}

void syscall_routine_noret(registers_t *regs) {
    syscall_routine(regs);
}

void syscalls_init() {
    irq_register_routine(SYSCALL_IRQ, (void *)&syscall_routine);
}