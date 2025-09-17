#include <stddef.h>
#include <stdint.h>

#include "../../src/syscall/syscall.h"
#include "../../src/devices/input/keyboard/keyboard.h"

#include "lib/common.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "uconsole.h"

// Uses PSF1 format
extern void kputchar(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32_t fg, uint32_t bg);

extern void kputs(const char *str, int cx, int cy);
extern void kputhex(uint64_t hex, int cx, int cy);

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi) {
    uint64_t out;

    asm volatile("mov %[_rax], %%rax\n"
                 "mov %[_rbx], %%rbx\n"
                 "mov %[_rcx], %%rcx\n"
                 "mov %[_rdx], %%rdx\n"
                 "mov %[_rsi], %%rsi\n"
                 "mov %[_rdi], %%rdi\n"
                 "int $0x80\n"
                 "mov %%rax, %[_out]\n"
                 : [_out] "=r"(out)
                 : [_rax] "r"(rax), [_rbx] "r"(rbx), [_rcx] "r"(rcx), [_rdx] "r"(rdx), [_rsi] "r"(rsi), [_rdi] "r"(rdi)
                 : "rax", "rbx", "rcx", "rdx", "rsi", "rdi" // Clobbered registers
    );

    return out;
}

// __attribute__((section(".data")))
const char teststr_shell[] = "\ntest!!!\n\n\0";
// char buffer[3];

__attribute__((aligned(0x1000))) //
fb_info_t fb_info;
kbd_buffer_state_t *kbd_buffer_state;

#define SHELL_PROMPT "kernel_shell >> "
#define CMD_BUFFER_MAX 256
#define CMD_TOKS_MAX 64

static char cmdbuffer[CMD_BUFFER_MAX];
static char toks[CMD_TOKS_MAX][CMD_BUFFER_MAX] = { "" };
static int cmd_buffer_i = 0;
static int toks_count = 0;

static void split_cmd(const char *cmd) {
    memset(toks[0], 0, CMD_BUFFER_MAX);

    int tok = 0;
    int i_tok = 0;

    size_t cmdlen = strlen(cmd);

    for (size_t i = 0; *cmd; cmd++ /* i is incremented within the loop */) {
        if (*cmd == ' ') {
            if (i == 0 || *(cmd - 1) == '\\' || *(cmd - 1) == ' ') {
                toks[tok][i_tok] = *cmd;
            } else {
                memset(toks[++tok], 0, CMD_BUFFER_MAX);
                i_tok = -1;
            }
        } else {
            toks[tok][i_tok] = *cmd;
        }

        ++i_tok;
        ++i;
    }

    toks_count = ++tok;
}

extern void tetris_main();

static void process_cmd(const char *cmd) {
    // printf("Received command: %s\n", cmd);

    split_cmd(cmd);

    if (!strcmp(toks[0], "echo")) {
        for (int i = 1; i < CMD_TOKS_MAX && toks[i][0] != 0; i++) {
            printf("%s ", toks[i]);
        }
        printf("\n");
    } else if (!strcmp(toks[0], "clear")) {
        clear();
    } else if (!strcmp(toks[0], "help")) {
        printf("Available commands:\n");
        printf("  echo [args...] - Print arguments to the console\n");
        printf("  clear          - Clear the console screen\n");
        printf("  tetris         - Start the Tetris game\n");
        printf("  help           - Show this help message\n");
    } else if (!strcmp(toks[0], "tetris")) {
        tetris_main();
        clear();
    } else if (toks[0][0] != 0) {
        printf("Unknown command: %s\n", toks[0]);
    }
}

// __attribute__((section(".text")))
void _start() {
    // TODO: handle errors in these syscalls

    syscall(SYSCALL_PRINT, (uint64_t)teststr_shell, 0, 0, 0, 0);

    syscall(SYSCALL_GET_FB, (uint64_t)&fb_info, 0, 0, 0, 0);
    kbd_buffer_state = (kbd_buffer_state_t *)syscall(SYSCALL_GET_KB, 0, 0, 0, 0, 0);

    clear();
    setcursor(true);

    printf("%s", SHELL_PROMPT);

    for (;;) {
        while (kbd_buffer_empty()) {
            
        }

        char ch;
        kbd_buffer_pop(&ch);

        if (ch == '\n') {
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