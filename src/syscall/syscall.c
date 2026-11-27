#include "syscall/syscall.h"

#include "arch/x86_64/cpuid/cpuid.h"
#include "arch/x86_64/gdt/gdt.h"
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
#include "proc/scheduler.h"
#include "proc/task.h"
#include "syscall/fs/syscall_fs.h"
#include "syscall/mem/memmap.h"
#include "syscall/syscall_helpers.h"
#include "timer/pit.h"
#include "timer/timer.h"

#include <stdint.h>

extern void *linker_elf_buf;

/* uint64_t syscall(uint64_t rax, uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t r8,
                                  uint64_t r9) {
    uint64_t out;

    asm volatile("mov %[_rax], %%rax\n"
                 "mov %[_rdi], %%rdi\n"
                 "mov %[_rsi], %%rsi\n"
                 "mov %[_rdx], %%rdx\n"
                 "mov %[_r10], %%r10\n"
                 "mov %[_r8], %%r8\n"
                 "mov %[_r9], %%r9\n"
                 "int $0x80\n"
                 "mov %%rax, %[_out]\n"
                 : [_out] "=r"(out)
                 : [_rax] "r"(rax), [_rdi] "r"(rdi), [_rsi] "r"(rsi), [_rdx] "r"(rdx), [_r10] "r"(r10), [_r8] "r"(r8),
                   [_r9] "r"(r9)
                 : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9" // Clobbered registers
    );

    return out;
} */

extern char *fb;
extern int fb_scanline, fb_width, fb_height, fb_bytes_per_pixel;

// Max number of argv (or envp) entries accepted by SYSCALL_SPAWN
#define SPAWN_MAX_ARGS 256

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

void syscall(registers_t *regs, task_t *task) {
    uint64_t call = CALL(regs);

    mm_t *mm = task->mm_to_use;

    if (call >= SYSCALL_FS_FIRST && call <= SYSCALL_FS_LAST) {
        syscall_handle_fs_call(regs, task, mm);
        goto ret;
    }

    switch (call) {
    case SYSCALL_GET_FB: {
        // arg1: pointer to struct for framebuffer data
        // Map the framebuffer into the currently loaded page table
        // TODO: check if framebuffer is in use
        int res = vmmap(virt_to_phys((uint64_t)fb), (void *)0x4000000000, (uint64_t)fb_scanline * (uint64_t)fb_height, VMA_USER | VMA_RW, NULL, mm);
        if (res) {
            printf("syscall_get_fb: vmmap failed (res %d)\n", res);
            regs->rax = (uint64_t)res;
            break;
        }

        bool valid = true;

        uint64_t arg1 = ARG1(regs);

        if (arg1 & 0xFFF) {
            printf("The fb_info pointer must be page aligned!\n");
            valid = false;
        } else {
            if (!vmlookup((void *)arg1, mm)) {
                printf("Page at %p not mapped, can't write fb value.\n", arg1);
                valid = false;
            }
        }

        if (valid) {
            fb_info_t *out = (fb_info_t *)phys_to_virt(get_physaddr(arg1, mm));

            memset(out, 0, sizeof(fb_info_t));

            out->fb_bytes_per_pixel = fb_bytes_per_pixel;
            out->fb_ptr             = (char *)0x4000000000;
            out->fb_scanline        = fb_scanline;
            out->fb_width           = fb_width;
            out->fb_height          = fb_height;

            regs->rax = arg1;
        } else {
            regs->rax = 0;
        }
        break;
    }
    case SYSCALL_GET_KB: {
        // printf("phys %p %p\n", kernel_pml4, get_physaddr((uint64_t)fb, kernel_pml4));
        // VERY IMPORTANT CRITICAL TODO: don't allow random processes to modify the resulting buffer (some sort of
        // access control is needed)
        /* page_t *page = find_page((uint64_t)&kbd_buffer_state, false, current_pml4);
        if (page && page->present) {
            kout(KERNEL_WARN, "WARN: Something is already mapped at keyboard buffer's address in this process!\n");
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
        } */
        regs->rax = -ENOSYS;
        break;
    }
    case SYSCALL_MEMMAP: {
        // TODO: have some way of tracking what has been mapped so we can unmap it if the process doesn't unmap on exit
        // TODO: we also need a way of unmapping this

        // (syscall) virtaddr size flags
        uint64_t virt                = ARG1(regs);
        uint64_t size                = ARG2(regs);
        syscall_memmap_flags_t flags = ARG3(regs);

        regs->rax = (uint64_t)syscall_routine_memmap((void *)virt, (size_t)size, flags, mm);

        break;
    }
    case SYSCALL_MEMMAP_FROM_BUFFER: {
        // (syscall) virtaddr memsize flags bufptr bufsize
        uint64_t virt                = ARG1(regs);
        uint64_t size                = ARG2(regs);
        syscall_memmap_flags_t flags = ARG3(regs);
        void *buffer                 = (void *)ARG4(regs);
        size_t buffer_size           = (size_t)ARG5(regs);

        void *buf_tmp = kmalloc_heap(buffer_size);
        if (!buf_tmp) {
            printf("syscall_routine: failed to allocate kernel buffer for memmap from buffer syscall of %p bytes\n",
                   buffer_size);
            regs->rax = (uint64_t)-ENOMEM;
        } else {
            regs->rax = (uint64_t)copy_to_kernel_buf(buf_tmp, buffer, buffer_size, mm);

            if (regs->rax != 0) {
                printf("syscall_routine: failed to copy data from user buffer %p for task %s (tid %p), errno %d\n",
                       buffer, task->name, task->tid, (int)regs->rax);
                kfree_heap(buf_tmp);
                break;
            }

            regs->rax = (uint64_t)syscall_routine_memmap_from_buffer((void *)virt, (size_t)size, flags, buf_tmp,
                                                                     buffer_size, mm);
            kfree_heap(buf_tmp);
        }

        break;
    }
    // case SYSCALL_MEMMAP_FILE:
    case SYSCALL_PRINT: {
        const char *str_ptr = NULL;
        regs->rax           = handle_string_arg(ARG1(regs), &str_ptr, mm);

        if (str_ptr) {
            printf("%s", str_ptr);
            kfree_heap(str_ptr);
            regs->rax = 0;
        }
        break;
    }
    case SYSCALL_PRINT_PTR: {
        printf("%p", ARG1(regs));
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
        kthread_sleep_ms(ARG1(regs));
        break;
    }
    case SYSCALL_SPAWN: {
        const char *path = NULL;
        regs->rax        = handle_string_arg(ARG1(regs), &path, mm);

        if (!path) {
            break;
        }

        // libc: syscall_spawn(path, argv, envp, argc, envc)
        const uint64_t uargc = ARG4(regs);
        const uint64_t uenvc = ARG5(regs);
        char **kargv         = NULL;
        char **kenvp         = NULL;

        int args_res = handle_string_array_arg(ARG2(regs), uargc, SPAWN_MAX_ARGS, &kargv, mm);
        if (!args_res) {
            args_res = handle_string_array_arg(ARG3(regs), uenvc, SPAWN_MAX_ARGS, &kenvp, mm);
        }
        if (args_res) {
            free_string_array(kargv, uargc);
            kfree_heap(path);
            regs->rax = (uint64_t)(int64_t)args_res;
            break;
        }

        task_t *spawn_task = create_task("spawn", task);
        if (!spawn_task) {
            free_string_array(kargv, uargc);
            free_string_array(kenvp, uenvc);
            kfree_heap(path);
            regs->rax = (uint64_t)(int64_t)-ENOMEM;
            break;
        }

        int res = vfs_open("/dev/tty0", SYSCALL_OPEN_FLAG_READ, 0, 0, &spawn_task->fds[0]);
        if (!res) {
            res = vfs_open("/dev/tty0", SYSCALL_OPEN_FLAG_WRITE, 0, 0, &spawn_task->fds[1]);
        }
        if (!res) {
            res = vfs_open("/dev/tty0", SYSCALL_OPEN_FLAG_WRITE, 0, 0, &spawn_task->fds[2]);
        }
        if (!res) {
            // The child may run and exit before execute() returns, so don't touch spawn_task afterwards.
            pid_t child_tid = spawn_task->tid;
            // execute() copies argv/envp onto the new stack, so they can be freed as soon as it returns
            res = execute(path, spawn_task, (int)uargc, kargv, (int)uenvc, kenvp);
            free_string_array(kargv, uargc);
            free_string_array(kenvp, uenvc);
            kargv = NULL;
            kenvp = NULL;
            if (!res) {
                kfree_heap(path);
                regs->rax = (uint64_t)child_tid;
                break;
            }
        }

        kout(KERNEL_WARN, "spawn: failed to start %s (%d)\n", path, res);

        task_exit(spawn_task);

        free_string_array(kargv, uargc); // NULL (and a no-op) if execute() already consumed them
        free_string_array(kenvp, uenvc);
        kfree_heap(path);
        regs->rax = (uint64_t)(int64_t)res;

        break;
    }
    case SYSCALL_WAITTID: {
        pid_t tid = (pid_t)ARG1(regs);

        regs->rax = (int)waittid(task, tid);

        break;
    }
    case SYSCALL_EXIT: {
        task_exit(task);
        cpu_scheduler_task();
    }
    case SYSCALL_EXIT_GROUP: {
        panic("SYSCALL_EXIT_GROUP not implemented!\n");
        break;
    }
    default:
        break;
    }

ret:
    asm volatile("cli");
}

#define IA32_EFER  0xC0000080
#define IA32_STAR  0xC0000081
#define IA32_LSTAR 0xC0000082
#define IA32_FMASK 0xC0000084
#define EFER_SCE   (1ULL << 0)

#define SYSRET_BASE 0x10 // only used by sysretq, not iretq

extern void syscall_entry(void);

void syscall_init_cpu() {
    write_msr(IA32_EFER, read_msr(IA32_EFER) | EFER_SCE);

    // STAR[47:32] = kernel CS (SS = CS + 8); STAR[63:48] = sysret base
    write_msr(IA32_STAR, ((uint64_t)SYSRET_BASE << 48) | ((uint64_t)KERNEL_CS << 32));

    write_msr(IA32_LSTAR, (uint64_t)syscall_entry);

    // RFLAGS bits cleared on entry: IF (0x200), DF (0x400), TF (0x100), AC (0x40000)
    write_msr(IA32_FMASK, 0x200 | 0x400 | 0x100 | 0x40000);
}