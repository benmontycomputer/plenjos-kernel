#pragma once

#include <stdint.h>
#include <stddef.h>

#include "plenjos/limits.h"
#include "proc/proc.h"

#include "kernel.h"

#include "memory/mm_common.h"

#define CALL(regs) (((registers_t *)regs)->rax)
#define ARG1(regs) (((registers_t *)regs)->rdi)
#define ARG2(regs) (((registers_t *)regs)->rsi)
#define ARG3(regs) (((registers_t *)regs)->rdx)
#define ARG4(regs) (((registers_t *)regs)->r10)
#define ARG5(regs) (((registers_t *)regs)->r8)
#define ARG6(regs) (((registers_t *)regs)->r9)

int copy_to_user_buf(void *dest, void *src, size_t count, bool override_write_check, pml4_t *current_pml4);
int copy_to_kernel_buf(void *dest, void *src, size_t count, pml4_t *current_pml4);

// This must be freed!
int handle_string_arg(pml4_t *current_pml4, uint64_t user_ptr, const char **out);

// Out must be at least PATH_MAX bytes
int handle_relative_path(const char *rel, proc_t *proc, char *out);

// Out must be at least PATH_MAX bytes
// Simplifies the path by removing ., .., and extra / characters
// It is safe to pass the same buffer for path_in and path_out
int collapse_path(const char *path_in, char *path_out);