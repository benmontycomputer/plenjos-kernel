#include "mman.h"

#include "sys/syscall.h"

#include "common.h"

// TODO: find a way to handle debug messages without just printing them to the console; this will be necessary once this
// becomes a freestanding userland library
#include "stdio.h"

int mmap(void *addr, size_t length) {
    if (addr == NULL) {
        printf("mmap: addr cannot be NULL\n");
        return -1;
    }

    int res = (int)syscall_memmap(addr, length);
    if (res != 0) {
        printf("mmap: syscall failed with res %d\n", res);
        return -1;
    }

    return 0;
}