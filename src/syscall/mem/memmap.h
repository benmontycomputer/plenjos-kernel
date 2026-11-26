#pragma once

#include "memory/mm_common.h"
#include "plenjos/syscall.h"

#include <stddef.h>
#include <stdint.h>

void *syscall_routine_memmap(void *addr, size_t length, syscall_memmap_flags_t flags, mm_t *mm);
void *syscall_routine_memmap_file(void *addr, size_t length, syscall_memmap_flags_t flags, int fd, size_t file_offset,
                                  mm_t *mm);

// NOTE: the addr does not need to be page-aligned. The buffer will be copied starting from addr, not from the first
// page of the mapping. The parts of the mapping outside of the buffer are guaranteed to be zeroed.
void *syscall_routine_memmap_from_buffer(void *addr, size_t length, syscall_memmap_flags_t flags, void *buffer,
                                         size_t buffer_length, mm_t *mm);

// Currently, this only supports removing permissions, not adding them.
int syscall_routine_memprotect(void *addr, size_t length, syscall_memmap_flags_t flags, mm_t *mm);