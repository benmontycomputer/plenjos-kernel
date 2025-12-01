#include "stdlib.h"
#include "stdio.h"
#include "common.h"

#include "plenjos/syscall.h"
#include "plenjos/dev/fb.h"
#include "plenjos/dev/kbd.h"

#include "stdlib_internal.h"

__attribute__((aligned(0x1000))) //
fb_info_t fb_info;
kbd_buffer_state_t *kbd_buffer_state;

void initialize_standard_library(int argc, char *argv[], int envc, char *envp[]) {
    syscall(SYSCALL_GET_FB, (uint64_t)&fb_info, 0, 0, 0, 0);
    kbd_buffer_state = (kbd_buffer_state_t *)syscall(SYSCALL_GET_KB, 0, 0, 0, 0, 0);

    init_heap();

    // TODO: setup stdin, stdout, stderr

    // printf("initialize_standard_library called (argc=%d, argv=%p, envc=%d, envp=%p) \n", argc, argv, envc, envp);
}

// TODO: implement this
_Noreturn void exit(int status) {
    // Halt for now
    while (1) {
        asm volatile("hlt");
    }
}