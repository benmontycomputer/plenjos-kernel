#pragma once

#include "plenjos/stat.h"
#include "plenjos/syscall.h"
#include "plenjos/types.h"

#include <stddef.h>
#include <stdint.h>

int syscall_open(const char *path, syscall_open_flags_t flags, mode_t mode_if_create);
ssize_t syscall_read(int fd, void *buf, size_t count);
int syscall_fstat(int fd, struct kstat *out_stat_ptr);
int syscall_print(const char *str);
int syscall_print_ptr(uint64_t val);
int syscall_close(int fd);