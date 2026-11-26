#include "sys/mman.h"

#include "sys/syscall.h"

// TODO: find a way to handle debug messages without just printing them to the console; this will be necessary once this
// becomes a freestanding userland library
#include "stdio.h"

int mmap_fixed(void *hint, size_t length) {
    void *res = (void *)syscall_memmap(hint, length, SYSCALL_MEMMAP_FLAG_WR | SYSCALL_MEMMAP_FLAG_FIXED);
    if (!res) {
        printf("mmap_fixed: syscall failed for %p (len %lu)\n", hint, length);
        return -1;
    }

    return 0;
}