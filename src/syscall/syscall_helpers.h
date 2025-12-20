#pragma once

#include <stdint.h>
#include <stddef.h>

#include "plenjos/limits.h"
#include "proc/proc.h"

#include "memory/mm_common.h"

int copy_to_user_buf(void *dest, void *src, size_t count, pml4_t *current_pml4);
int copy_to_kernel_buf(void *dest, void *src, size_t count, pml4_t *current_pml4);

// This must be freed!
int handle_string_arg(pml4_t *current_pml4, uint64_t user_ptr, const char **out);

// Out must be at least PATH_MAX bytes
int handle_relative_path(const char *rel, proc_t *proc, char out[PATH_MAX]);