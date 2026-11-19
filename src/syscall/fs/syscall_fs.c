#include "syscall/fs/syscall_fs.h"

#include "devices/storage/drive.h"
#include "devices/sysdrivers.h"
#include "lib/string.h"
#include "plenjos/errno.h"
#include "plenjos/types.h"
#include "stdint.h"

void syscall_handle_fs_call(registers_t *regs, proc_t *proc, pml4_t *current_pml4) {
    uint64_t call = CALL(regs);

    switch (call) {
    case SYSCALL_READ: {
        regs->rax = (uint64_t)syscall_routine_read(ARG1(regs), (void *)ARG2(regs), (size_t)ARG3(regs), proc, current_pml4);
        break;
    }
    case SYSCALL_WRITE: {
        if (ARG3(regs) == 0) {
            regs->rax = 0;
            break;
        }
        void *kbuf = kmalloc_heap((size_t)ARG3(regs));
        if (!kbuf) {
            printf("syscall_routine: failed to allocate kernel buffer for write syscall of %p bytes\n",
                   (size_t)ARG3(regs));
            regs->rax = (uint64_t)-ENOMEM;
            break;
        }

        int check = copy_to_kernel_buf(kbuf, (void *)ARG2(regs), (size_t)ARG3(regs), current_pml4);
        if (check < 0) {
            printf("syscall_routine: failed to copy data from user buffer %p for process %s (pid %p), errno %d\n",
                   (void *)ARG2(regs), proc->name, proc->pid, check);
            kfree_heap(kbuf);
            regs->rax = (uint64_t)check;
            break;
        }

        regs->rax = (uint64_t)syscall_routine_write(ARG1(regs), (const void *)kbuf, (size_t)ARG3(regs), proc);
        kfree_heap(kbuf);
        break;
    }
    case SYSCALL_OPEN: {
        const char *path = NULL;
        regs->rax        = handle_string_arg(current_pml4, ARG1(regs), &path);
        if (path) {
            regs->rax = (uint64_t)syscall_routine_open(path, (syscall_open_flags_t)ARG2(regs), (mode_t)ARG3(regs), proc);
            kfree_heap((void *)path);
        }
        break;
    }
    case SYSCALL_CLOSE: {
        regs->rax = (uint64_t)syscall_routine_close(ARG1(regs), proc);
        break;
    }
    case SYSCALL_STAT: {
        const char *path = NULL;
        regs->rax        = handle_string_arg(current_pml4, ARG1(regs), &path);
        if (path) {
            regs->rax = (uint64_t)syscall_routine_stat(path, ARG2(regs), proc, current_pml4);
            kfree_heap((void *)path);
        }
        break;
    }
    case SYSCALL_FSTAT: {
        regs->rax = (uint64_t)syscall_routine_fstat((int)ARG1(regs), ARG2(regs), proc, current_pml4);
        break;
    }
    case SYSCALL_LSTAT: {
        const char *path = NULL;
        regs->rax        = handle_string_arg(current_pml4, ARG1(regs), &path);
        if (path) {
            regs->rax = (uint64_t)syscall_routine_lstat(path, ARG2(regs), proc, current_pml4);
            kfree_heap((void *)path);
        }
        break;
    }
    // case SYSCALL_POLL:
    case SYSCALL_LSEEK: {
        regs->rax = (uint64_t)syscall_routine_lseek(ARG1(regs), (ssize_t)ARG2(regs), (int)ARG3(regs), proc);
        break;
    }
    case SYSCALL_GETDENTS: {
        regs->rax
            = (uint64_t)syscall_routine_getdents(ARG1(regs), (void *)ARG2(regs), (size_t)ARG3(regs), proc, current_pml4);
        break;
    }
    case SYSCALL_MKDIR: {
        const char *path = NULL;
        regs->rax        = handle_string_arg(current_pml4, ARG1(regs), &path);
        if (path) {
            regs->rax = (uint64_t)syscall_routine_mkdir(path, (mode_t)ARG2(regs), proc);
            kfree_heap((void *)path);
        }
        break;
    }
    /* case SYSCALL_RMDIR:
    case SYSCALL_RENAME:
    case SYSCALL_CHMOD:
    case SYSCALL_FCHMOD:
    case SYSCALL_CHOWN:
    case SYSCALL_FCHOWN:
    case SYSCALL_LCHOWN: */
    // TODO: modify file groups
    case SYSCALL_GETCWD: {
        regs->rax = (uint64_t)syscall_routine_getcwd(ARG1(regs), (size_t)ARG2(regs), proc, current_pml4);
        break;
    }
    case SYSCALL_CHDIR: {
        const char *path = NULL;
        regs->rax        = handle_string_arg(current_pml4, ARG1(regs), &path);
        if (path) {
            regs->rax = (uint64_t)syscall_routine_chdir(path, proc);
            kfree_heap((void *)path);
        }
        break;
    }
    case SYSCALL_MOUNT: {
        if (proc->uid != 0) {
            regs->rax = -EACCES;
            break;
        }

        const char *dev    = NULL;
        const char *target = NULL;
        regs->rax          = handle_string_arg(current_pml4, ARG1(regs), &dev);
        if (dev) {
            regs->rax = handle_string_arg(current_pml4, ARG2(regs), &target);
            if (target) {
                vfs_handle_t *dev_handle = NULL;
                const char *dev_path     = kmalloc_heap(strlen(dev) + 1);
                const char *target_path  = kmalloc_heap(strlen(target) + 1);

                if (dev_path && target_path) {
                    regs->rax = (uint64_t)handle_relative_path(dev, proc, dev_path);
                    if (!regs->rax) {
                        regs->rax = (uint64_t)handle_relative_path(target, proc, target_path);
                    }
                    if (!regs->rax) {
                        regs->rax = vfs_open(dev_path, SYSCALL_OPEN_FLAG_READ, 0, proc->uid, &dev_handle);

                        if (!regs->rax) {
                            regs->rax = drive_mount_from_handle(dev_handle, target_path);
                            vfs_close(dev_handle);
                        }
                    }
                }

                if (dev_path) {
                    kfree_heap(dev_path);
                }

                if (target_path) {
                    kfree_heap(target_path);
                }

                kfree_heap(target);
            }
            kfree_heap(dev);
        }
        break;
    }
    /* case SYSCALL_FCHDIR:
    case SYSCALL_LINK:
    case SYSCALL_UNLINK:
    case SYSCALL_SYMLINK:
    case SYSCALL_READLINK: */
    default: {
        printf("syscall_handle_fs_call: unrecognized filesystem syscall %p\n", call);
        regs->rax = (uint64_t)-ENOSYS;
        break;
    }
    }
}