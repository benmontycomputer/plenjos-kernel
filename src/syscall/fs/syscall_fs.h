#pragma once

#include <stdint.h>
#include <stddef.h>
#include "kernel.h"

#include "proc/task.h"

#include "lib/stdio.h"

#include "memory/kmalloc.h"
#include "memory/mm_common.h"

#include "plenjos/syscall.h"
#include "plenjos/types.h"
#include "plenjos/stat.h"
#include "plenjos/errno.h"

#include "syscall/syscall_helpers.h"

#include "vfs/fscache.h"
#include "vfs/vfs.h"

// TODO: these functions have a ton of repeated code; refactor common parts into helper functions

void syscall_handle_fs_call(registers_t *regs, task_t *task, mm_t *mm);

int syscall_fs_helper_get_handle_and_check_backing_node(int fd, task_t *task, vfs_handle_t **out_handle);
int syscall_fs_helper_get_dir_handle(int fd, task_t *task, vfs_handle_t **out_handle);
int syscall_fs_helper_get_not_dir_handle(int fd, task_t *task, vfs_handle_t **out_handle);

/**
 * Basic architecture for io-related syscalls:
 * 1. Any and all output buffers are handled by the syscall routine itself; the routine is passed a user-space pointer.
 * 2. Any input pointers, however, are copied from user space to kernel space before being passed to the routine.
 *    This allows us to use const char *restrict for all pointer arguments. This means that routines are passed a
 * kernel-space pointer. Finally, this means that routines don't have to worry about being passed NULL pointers.
 * 
 * TODO: similar to how char restrict *ch is used, we can use char __user *buf to indicate user-space pointers.
 */

// Because we copy any string arguments from user space to kernel space before any routines happen, we can use const
// char *restrict for everything.
// (*restrict indicates that the pointer is the only reference to that data in this scope, allowing for certain compiler
// optimizations.)

// count must be <= SSIZE_MAX
// These two functions don't work on directories; use getdents in place of read for directories.
ssize_t syscall_routine_read(int fd, void *buf, size_t count, task_t *task, mm_t *mm);
ssize_t syscall_routine_write(int fd, const void *buf, size_t count, task_t *task);

// If mode_if_create is used, the only bits that are honored are the permission bits (lower 9) and the special bits
// (next 3 lowest: setuid, setgid, sticky). The create and directory flags can't be specified simultaneously; if the
// create flag is set, only regular files can be created.
int syscall_routine_open(const char *restrict path, syscall_open_flags_t flags, mode_t mode, task_t *task);
int syscall_routine_close(int fd, task_t *task);

// These functions fill out_stat, which must be a user-space pointer to a struct kstat
int syscall_routine_stat(const char *restrict path, uint64_t out_stat_ptr, task_t *task, mm_t *mm);
int syscall_routine_fstat(int fd, uint64_t out_stat_ptr, task_t *task, mm_t *mm);
int syscall_routine_lstat(const char *restrict path, uint64_t out_stat_ptr, task_t *task, mm_t *mm);

// TODO: implement
void syscall_routine_poll(void);

// Unlike read and write, lseek also works on directories
off_t syscall_routine_lseek(int fd, off_t offset, int whence, task_t *task);

// This works only on directory file descriptors; count = size of output buffer, NOT number of entries to read
// This syscall is the equivalent of Linux's getdents64, NOT getdents.
ssize_t syscall_routine_getdents(int fd, void *buf, size_t count, task_t *task, mm_t *mm);

int syscall_routine_mkdir(const char *restrict path, mode_t mode, task_t *task);
int syscall_routine_rmdir(const char *restrict path, task_t *task);
void syscall_routine_rename(void);
void syscall_routine_chmod(void);
void syscall_routine_fchmod(void);
void syscall_routine_chown(void);
void syscall_routine_fchown(void);
void syscall_routine_lchown(void);
// TODO: modify file groups
ssize_t syscall_routine_getcwd(uint64_t out_buf_ptr, ssize_t size, task_t *task, mm_t *mm);
int syscall_routine_chdir(const char *restrict path, task_t *task);
void syscall_routine_fchdir(void);
// Hard links CANNOT point to directories
void syscall_routine_link(void);
// This handles deletion of non-directories
void syscall_routine_unlink(void);
// TODO: if unlinkat() is implemented, it should have a flag to allow deletion of directories (but unlink() should not)

void syscall_routine_symlink(void);
void syscall_routine_readlink(void);