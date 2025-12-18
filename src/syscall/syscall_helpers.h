#pragma once

#include <stdint.h>
#include <stddef.h>

#include "memory/mm_common.h"

int copy_to_user_buf(void *dest, void *src, size_t count, pml4_t *current_pml4);
int copy_to_kernel_buf(void *dest, void *src, size_t count, pml4_t *current_pml4);

// This must be freed!
int handle_string_arg(pml4_t *current_pml4, uint64_t user_ptr, const char **out);