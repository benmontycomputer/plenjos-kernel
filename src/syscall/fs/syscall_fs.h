#pragma once

#include <stdint.h>
#include <stddef.h>
#include "kernel.h"

#include "proc/proc.h"

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

void syscall_handle_fs_call(registers_t *regs, proc_t *proc, pml4_t *current_pml4);

ssize_t syscall_fs_helper_get_handle_and_check_backing_node(size_t fd, proc_t *proc, vfs_handle_t **out_handle);
ssize_t syscall_fs_helper_get_dir_handle(size_t fd, proc_t *proc, vfs_handle_t **out_handle);
ssize_t syscall_fs_helper_get_not_dir_handle(size_t fd, proc_t *proc, vfs_handle_t **out_handle);

/**
 * Basic architecture for io-related syscalls:
 * 1. Any and all output buffers are handled by the syscall routine itself; the routine is passed a user-space pointer.
 * 2. Any input pointers, however, are copied from user space to kernel space before being passed to the routine.
 *    This allows us to use const char *restrict for all pointer arguments. This means that routines are passed a
 * kernel-space pointer.
 */

// Because we copy any string arguments from user space to kernel space before any routines happen, we can use const
// char *restrict for everything.
// (*restrict indicates that the pointer is the only reference to that data in this scope, allowing for certain compiler
// optimizations.)

// count must be <= SSIZE_MAX
// These two functions don't work on directories; use getdents in place of read for directories.
ssize_t syscall_routine_read(size_t fd, void *buf, size_t count, proc_t *proc, pml4_t *current_pml4);
ssize_t syscall_routine_write(size_t fd, const void *buf, size_t count, proc_t *proc);

// If mode_if_create is used, the only bits that are honored are the permission bits (lower 9) and the special bits
// (next 3 lowest: setuid, setgid, sticky). The create and directory flags can't be specified simultaneously; if the
// create flag is set, only regular files can be created.
ssize_t syscall_routine_open(const char *restrict path, syscall_open_flags_t flags, mode_t mode, proc_t *proc);
int syscall_routine_close(size_t fd, proc_t *proc);

// These functions fill out_stat, which must be a user-space pointer to a struct stat
ssize_t syscall_routine_stat(const char *restrict path, uint64_t out_stat_ptr, proc_t *proc, pml4_t *current_pml4);
ssize_t syscall_routine_fstat(size_t fd, uint64_t out_stat_ptr, proc_t *proc, pml4_t *current_pml4);
ssize_t syscall_routine_lstat(const char *restrict path, uint64_t out_stat_ptr, proc_t *proc, pml4_t *current_pml4);

// TODO: implement
void syscall_routine_poll();

// Unlike read and write, lseek also works on directories
ssize_t syscall_routine_lseek(size_t fd, ssize_t offset, int whence, proc_t *proc);

// This works only on directory file descriptors; count = size of output buffer, NOT number of entries to read
ssize_t syscall_routine_getdents(size_t fd, void *buf, size_t count, proc_t *proc, pml4_t *current_pml4);

ssize_t syscall_routine_mkdir(const char *restrict path, mode_t mode, proc_t *proc);
ssize_t syscall_routine_rmdir(const char *restrict path, proc_t *proc);
void syscall_routine_rename();
void syscall_routine_chmod();
void syscall_routine_fchmod();
void syscall_routine_chown();
void syscall_routine_fchown();
void syscall_routine_lchown();
// TODO: modify file groups
void syscall_routine_getcwd();
void syscall_routine_chdir();
void syscall_routine_fchdir();
void syscall_routine_link();
void syscall_routine_unlink();
void syscall_routine_symlink();
void syscall_routine_readlink();