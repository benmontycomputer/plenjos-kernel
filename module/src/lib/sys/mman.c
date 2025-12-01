#include "mman.h"

#include "plenjos/syscall.h"

#include "lib/common.h"

// TODO: find a way to handle debug messages without just printing them to the console; this will be necessary once this
// becomes a freestanding userland library
#include "lib/stdio.h"

int mmap(void *addr, size_t length) {
    if (addr == NULL) {
        printf("mmap: addr cannot be NULL\n");
        return -1;
    }

    int res = (int)syscall(SYSCALL_MEMMAP, (uint64_t)addr, length, 0, 0, 0);
    if (res != 0) {
        printf("mmap: syscall failed with res %d\n", res);
        return -1;
    }

    return 0;
}