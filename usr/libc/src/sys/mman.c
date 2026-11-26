#include "sys/mman.h"

#include "sys/syscall.h"

// TODO: find a way to handle debug messages without just printing them to the console; this will be necessary once this
// becomes a freestanding userland library
#include "stdio.h"

int mmap_fixed(void *hint, size_t length) {
    void *res = (void *)syscall_memmap(hint, length, SYSCALL_MEMMAP_FLAG_WR | SYSCALL_MEMMAP_FLAG_FIXED);
    if (res != 0) {
        printf("mmap_fixed: syscall failed with res %d\n", (int)res);
        return -1;
    }

    return 0;
}