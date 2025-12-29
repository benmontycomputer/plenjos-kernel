#include "syscall/syscall.h"

#include "arch/x86_64/irq.h"
#include "devices/input/keyboard/keyboard.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "memory/mm_common.h"
#include "plenjos/dev/fb.h"
#include "plenjos/errno.h"
#include "plenjos/syscall.h"
#include "proc/proc.h"
#include "syscall/fs/syscall_fs.h"
#include "syscall/mem/memmap.h"
#include "syscall/syscall_helpers.h"
#include "timer/pit.h"

#include <stdint.h>

extern kbd_buffer_state_t kbd_buffer_state;

static uint8_t fb_mapped = 0;

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
extern int fb_scanline, fb_width, fb_height, fb_bytes_per_pixel;

// TODO: how do we handle the string being a non-contiguous set of pages?
/* bool _syscall_helper_check_str_ptr_perms(pml4_t *current_pml4, uint64_t user_ptr, char **out) {
    uint64_t str_ptr = phys_to_virt(get_physaddr(user_ptr, current_pml4));
    uint64_t end = user_ptr + strlen((char *)str_ptr) + 1;

    for (uint64_t i = user_ptr; i < end; i += PAGE_LEN) {
        // TODO: check that the addr is user-accessible (idk if the autocreate option on find_page works rn)
        page_t *page = find_page(i, false, current_pml4);
        if (!page || !(page->present)) {
            printf("Failed at %p: page not mapped\n", i);
            if (out) *out = NULL;
            return false;
        }
        if (!page->user) {
            printf("Failed at %p: permission denied (not user accessible)\n", i);
            if (out) *out = NULL;
            return false;
        }
    }

    if (out) *out = (char *)str_ptr;

    return true;
} */

registers_t *syscall_routine(registers_t *regs) {
    uint64_t call = regs->rax;
    // printf("syscall %d\n", (int)call);

    // TODO: would we rather use this or the one from the proc struct? They should ALWAYS be the same, right?
    pml4_t *current_pml4 = (pml4_t *)phys_to_virt(regs->cr3 & ~0xFFF);

    proc_t *proc = _get_proc_kernel();

    if (!proc) {
        printf("syscall: failed to get current process!\n");

        // TODO: handle this more gracefully (we might actually want to panic; this indicates a serious kernel issue)
        // TODO: is this the correct errno?
        regs->rax = (uint64_t)-1;
        return regs;
    }

    if (call >= SYSCALL_FS_FIRST && call <= SYSCALL_FS_LAST) {
        syscall_handle_fs_call(regs, proc, current_pml4);
        return regs;
    }

    switch (call) {
    case SYSCALL_GET_FB: {
        // rbx: pointer to struct for framebuffer data
        // Map the framebuffer into the currently loaded page table
        // current_pml4 = (pml4_t *)get_cr3_addr();
        // TODO: check if framebuffer is in use
        // printf("phys %p %p\n", kernel_pml4, get_physaddr((uint64_t)fb, kernel_pml4));
        map_virtual_memory_using_alloc(
            virt_to_phys((uint64_t)fb), (uint64_t)fb, (uint64_t)fb_scanline * (uint64_t)fb_height,
            PAGE_FLAG_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITE, alloc_paging_node, current_pml4);

        bool valid = true;

        uint64_t rbx = regs->rbx;

        if (rbx & 0xFFF) {
            printf("The fb_info pointer must be page aligned!\n");
            valid = false;
        } else {
            page_t *page = find_page(rbx, false, current_pml4);
            if (!page) {
                printf("Page at %p not mapped, can't write fb value.\n", rbx);
                valid = false;
            } else if (!page->user) {
                printf("Page at %p not user accessible, can't write fb value.\n", rbx);
                valid = false;
            } else if (!page->rw) {
                printf("Page at %p not writeable, can't write fb value.\n", rbx);
                valid = false;
            }
        }

        if (valid) {
            fb_info_t *out = (fb_info_t *)phys_to_virt(get_physaddr(rbx, current_pml4));

            memset(out, 0, sizeof(fb_info_t));

            out->fb_bytes_per_pixel = fb_bytes_per_pixel;
            // out->fb_height = fb_height / 4;
            // out->fb_ptr = fb + (fb_mapped % 4) * (fb_height / 4) * fb_scanline;
            out->fb_ptr             = fb;
            out->fb_scanline        = fb_scanline;
            out->fb_width           = fb_width;
            out->fb_height          = fb_height;

            // fb_mapped++;

            regs->rax = rbx;
        } else {
            regs->rax = 0;
        }
        break;
    }
    case SYSCALL_GET_KB: {
        // printf("phys %p %p\n", kernel_pml4, get_physaddr((uint64_t)fb, kernel_pml4));
        // VERY IMPORTANT CRITICAL TODO: don't allow random processes to modify the resulting buffer (some sort of
        // access control is needed)
        page_t *page = find_page((uint64_t)&kbd_buffer_state, false, current_pml4);
        if (page && page->present) {
            printf("WARN: Something is already mapped at keyboard buffer's address in this process!\n");
            if (get_physaddr((uint64_t)&kbd_buffer_state, current_pml4)
                != get_physaddr((uint64_t)&kbd_buffer_state, kernel_pml4)) {
                printf("And it's not the keyboard buffer! %p %p\n",
                       get_physaddr((uint64_t)&kbd_buffer_state, current_pml4),
                       get_physaddr((uint64_t)&kbd_buffer_state, kernel_pml4));
                regs->rax = 0;
            } else {
                // Re-map to set proper permissions
                map_virtual_memory_using_alloc(get_physaddr((uint64_t)&kbd_buffer_state, kernel_pml4),
                                               (uint64_t)&kbd_buffer_state, sizeof(kbd_buffer_state_t),
                                               PAGE_FLAG_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITE, alloc_paging_node,
                                               current_pml4);
                regs->rax = (uint64_t)&kbd_buffer_state;
            }
        } else {
            map_virtual_memory_using_alloc(get_physaddr((uint64_t)&kbd_buffer_state, kernel_pml4),
                                           (uint64_t)&kbd_buffer_state, sizeof(kbd_buffer_state_t),
                                           PAGE_FLAG_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITE, alloc_paging_node,
                                           current_pml4);
            regs->rax = (uint64_t)&kbd_buffer_state;
        }
        break;
    }
    case SYSCALL_MEMMAP: {
        // TODO: have some way of tracking what has been mapped so we can unmap it if the process doesn't unmap on exit
        // TODO: we also need a way of unmapping this

        // (syscall) virtaddr size flags
        uint64_t virt                = regs->rbx;
        uint64_t size                = regs->rcx;
        syscall_memmap_flags_t flags = regs->rdx;

        regs->rax = (uint64_t)syscall_routine_memmap((void *)virt, (size_t)size, flags, current_pml4);

        break;
    }
    case SYSCALL_MEMMAP_FROM_BUFFER: {
        // (syscall) virtaddr memsize flags bufptr bufsize
        uint64_t virt                = regs->rbx;
        uint64_t size                = regs->rcx;
        syscall_memmap_flags_t flags = regs->rdx;
        void *buffer                 = (void *)regs->rsi;
        size_t buffer_size           = (size_t)regs->rdi;

        void *buf_tmp = kmalloc_heap(buffer_size);
        if (!buf_tmp) {
            printf("syscall_routine: failed to allocate kernel buffer for memmap from buffer syscall of %p bytes\n",
                   buffer_size);
            regs->rax = (uint64_t)-ENOMEM;
        } else {
            regs->rax = (uint64_t)copy_to_kernel_buf(buf_tmp, buffer, buffer_size, current_pml4);

            if (regs->rax != 0) {
                printf("syscall_routine: failed to copy data from user buffer %p for process %s (pid %p), errno %d\n",
                       buffer, proc->name, proc->pid, (int)regs->rax);
                kfree_heap(buf_tmp);
                break;
            }

            regs->rax = (uint64_t)syscall_routine_memmap_from_buffer((void *)virt, (size_t)size, flags, buf_tmp,
                                                                     buffer_size, current_pml4);
            kfree_heap(buf_tmp);
        }

        break;
    }
    // case SYSCALL_MEMMAP_FILE:
    case SYSCALL_PRINT: {
        const char *str_ptr = NULL;
        regs->rax           = handle_string_arg(current_pml4, regs->rbx, &str_ptr);

        if (str_ptr) {
            printf("%s", str_ptr);
            kfree_heap((void *)str_ptr);
            regs->rax = 0;
        }
        break;
    }
    case SYSCALL_PRINT_PTR: {
        printf("%p", regs->rbx);
        break;
    }
    case SYSCALL_KB_READ: {
        /* while (kbd_buffer_empty()) {
            asm volatile("sti");
            // printf("empty\n");
        } */

        /* uint64_t event;
        kbd_buffer_pop(&event);

        // printf("%c", ch);

        regs->rax = *(uint64_t *)&event; */

        regs->rax = ENOSYS;

        break;
    }
    case SYSCALL_SLEEP: {
        asm volatile("sti");
        pit_sleep((uint32_t)regs->rbx);
        break;
    }
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