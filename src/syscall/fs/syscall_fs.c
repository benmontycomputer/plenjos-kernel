#include "syscall/fs/syscall_fs.h"

#include <stdint.h>

#include "plenjos/errno.h"

void syscall_handle_fs_call(registers_t *regs, proc_t *proc, pml4_t *current_pml4) {
    uint64_t call = regs->rax;

    switch (call) {
    case SYSCALL_READ: {
        regs->rax = (uint64_t)syscall_routine_read(regs->rbx, (void *)regs->rcx, (size_t)regs->rdx, proc, current_pml4);
        break;
    }
    case SYSCALL_WRITE: {
        void *kbuf = kmalloc_heap((size_t)regs->rdx);
        if (!kbuf) {
            printf("syscall_routine: failed to allocate kernel buffer for write syscall of %p bytes\n",
                   (size_t)regs->rdx);
            regs->rax = (uint64_t)-ENOMEM;
            break;
        }

        int check = copy_to_kernel_buf(kbuf, (void *)regs->rcx, (size_t)regs->rdx, current_pml4);
        if (check < 0) {
            printf("syscall_routine: failed to copy data from user buffer %p for process %s (pid %p), errno %d\n",
                   (void *)regs->rcx, proc->name, proc->pid, check);
            kfree_heap(kbuf);
            regs->rax = (uint64_t)check;
            break;
        }

        regs->rax = (uint64_t)syscall_routine_write(regs->rbx, (const void *)kbuf, (size_t)regs->rdx, proc);
        kfree_heap(kbuf);
        break;
    }
    case SYSCALL_OPEN: {
        const char *path = NULL;
        printf("trying...\n\n");
        regs->rax = handle_string_arg(current_pml4, regs->rbx, &path);
        if (path) {
            regs->rax = (uint64_t)syscall_routine_open(path, (syscall_open_flags_t)regs->rcx, (mode_t)regs->rdx, proc);
            kfree_heap((void *)path);
        }
        break;
    }
    case SYSCALL_CLOSE: {
        regs->rax = (uint64_t)syscall_routine_close(regs->rbx, proc);
        break;
    }
    case SYSCALL_STAT: {
        const char *path = NULL;
        regs->rax = handle_string_arg(current_pml4, regs->rbx, &path);
        if (path) {
            regs->rax = (uint64_t)syscall_routine_stat(path, regs->rcx, proc, current_pml4);
            kfree_heap((void *)path);
        }
        break;
    }
    case SYSCALL_FSTAT: {
        regs->rax = (uint64_t)syscall_routine_fstat(regs->rbx, regs->rcx, proc, current_pml4);
        break;
    }
    case SYSCALL_LSTAT: {
        const char *path = NULL;
        regs->rax = handle_string_arg(current_pml4, regs->rbx, &path);
        if (path) {
            regs->rax = (uint64_t)syscall_routine_lstat(path, regs->rcx, proc, current_pml4);
            kfree_heap((void *)path);
        }
        break;
    }
    case SYSCALL_POLL:
    case SYSCALL_LSEEK: {
        regs->rax = (uint64_t)syscall_routine_lseek(regs->rbx, (ssize_t)regs->rcx, (int)regs->rdx, proc);
        break;
    }
    case SYSCALL_GETDENTS: {
        regs->rax = (uint64_t)syscall_routine_getdents(regs->rbx, (void *)regs->rcx, (size_t)regs->rdx, proc, current_pml4);
        break;
    }
    case SYSCALL_MKDIR:
    case SYSCALL_RMDIR:
    case SYSCALL_RENAME:
    case SYSCALL_CHMOD:
    case SYSCALL_FCHMOD:
    case SYSCALL_CHOWN:
    case SYSCALL_FCHOWN:
    case SYSCALL_LCHOWN:
    // TODO: modify file groups
    case SYSCALL_GETCWD:
    case SYSCALL_CHDIR:
    case SYSCALL_FCHDIR:
    case SYSCALL_LINK:
    case SYSCALL_UNLINK:
    case SYSCALL_SYMLINK:
    case SYSCALL_READLINK:
    default: {
        printf("syscall_handle_fs_call: unrecognized filesystem syscall %p\n", call);
        regs->rax = (uint64_t)-ENOSYS;
        break;
    }
    }
}