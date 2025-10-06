#include <stdint.h>
#include <stdbool.h>

#include "lib/stdio.h"
#include "lib/string.h"

#include "devices/input/keyboard/keyboard.h"

#include "shell/shell.h"

#include "kernel.h"

#include "syscall/syscall.h"

#define SHELL_PROMPT "kernel_shell >> "
#define CMD_BUFFER_MAX 256

static char cmdbuffer[CMD_BUFFER_MAX];
static int cmd_buffer_i = 0;

static void process_cmd(const char *cmd) {
    // printf("Received command: %s\n", cmd);

    size_t cmdlen = strlen(cmd);

    if (!strncmp(cmd, "meminfo", cmdlen)) {
        printf("meminfo not implemented yet.\n");
    }
}

static void shell() {
    // char *fb = (char *)syscall(SYSCALL_GET_FB, 0, 0, 0, 0, 0);
    // syscall(SYSCALL_PRINT, (uint64_t)"\ntest!!!\n\n", 0, 0, 0, 0);

    // Shell stub
    setcursor(true);

    char ch;

    printf("\n%s", SHELL_PROMPT);

    while (true) {
        while (kbd_buffer_empty()) {

        }

        while (kbd_buffer_pop(&ch) == 0) {
            if (ch == '\n'){
                printf("\n");

                cmdbuffer[cmd_buffer_i] = 0;

                process_cmd(cmdbuffer);

                printf(SHELL_PROMPT);

                cmd_buffer_i = 0;
            } else {
                if (ch == '\b') {
                    if (cmd_buffer_i > 0) {
                        --cmd_buffer_i;
                        backs();
                    }
                } else {
                    // Leave space for end of string
                    if (cmd_buffer_i < (CMD_BUFFER_MAX - 1)) {
                        printf("%c", ch);
                        cmdbuffer[cmd_buffer_i] = ch;
                        ++cmd_buffer_i;
                    }
                }
            }
        }
    }
}

void start_shell() {
    shell();
}