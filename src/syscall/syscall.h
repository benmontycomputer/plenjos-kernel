#pragma once

#include <stdint.h>

typedef enum {
    SYSCALL_READ,
} syscalls_call;

void syscalls_init();