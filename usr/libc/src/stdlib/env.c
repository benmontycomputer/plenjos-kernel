#include "stdlib.h"
#include "stdio.h"
#include "errno.h"

#include "sys/syscall.h"
#include "plenjos/dev/fb.h"
#include "plenjos/dev/kbd.h"

#include "stdlib_internal.h"

FILE _stdin = {
    .fd = 0,
};

FILE _stdout = {
    .fd = 1,
};

FILE _stderr = {
    .fd = 2,
};

int errno = 0;

__attribute__((aligned(0x1000))) //
fb_info_t fb_info;

void initialize_standard_library(int argc, char *argv[], int envc, char *envp[]) {
    syscall_get_fb(&fb_info);

    init_heap();

    stdin = &_stdin;
    stdout = &_stdout;
    stderr = &_stderr;

    // printf("initialize_standard_library called (argc=%d, argv=%p, envc=%d, envp=%p) \n", argc, argv, envc, envp);
}

// TODO: implement this
_Noreturn void exit(int status) {
    // Halt for now
    while (1) {
        asm volatile("hlt");
    }
}