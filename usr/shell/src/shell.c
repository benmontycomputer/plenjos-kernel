#include "graphics/draw.h"
#include "keyboard.h"
#include "libshell/libshell.h"
#include "plenjos/dev/fb.h"
#include "plenjos/dev/kbd.h"
#include "stddef.h"
#include "stdio.h"
#include "string.h"
#include "sys/syscall.h"
#include "uconsole.h"

#include <stdint.h>

// __attribute__((section(".data")))
const char teststr_shell[] = "\ntest!!!\n\n\0";
// char buffer[3];

static char cmdbuffer[CMD_BUFFER_MAX];
static char toks[CMD_TOKS_MAX][CMD_BUFFER_MAX] = { "" };
static int cmd_buffer_i                        = 0;
static int toks_count                          = 0;

static char cmd_history[40][CMD_BUFFER_MAX];
static int cmd_history_count = 0;
static int cmd_history_i     = -1;

// TODO: handle quotes
static void split_cmd(const char *cmd) {
    memset(toks[0], 0, CMD_BUFFER_MAX);

    int tok   = 0;
    int i_tok = 0;

    for (size_t i = 0; *cmd; cmd++ /* i is incremented within the loop */) {
        if (*cmd == ' ') {
            if (i == 0 || *(cmd - 1) == '\\') {
                toks[tok][i_tok] = *cmd;
            } else if (*(cmd - 1) == ' ') {
                // Skip multiple spaces
                --i_tok;
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

    // Make sure the last token isn't blank (e.g., trailing spaces)
    if (toks[tok - 1][0] == 0) {
        toks_count--;
    }
}

void draw_terminal_window() {
    // Window position and size
    int win_x = 40, win_y = 40;
    int win_w = 640, win_h = 320;
    int radius = 8;

    // Colors
    uint32_t border_color   = 0xCCCCCC;
    uint32_t fill_color     = 0x222222;
    uint32_t titlebar_color = 0x333333;
    uint32_t close_color    = 0xFF5F57; // Red
    uint32_t min_color      = 0xFEBC2E; // Yellow
    uint32_t zoom_color     = 0x28C940; // Green

    // Draw window background with rounded corners
    draw_filled_rounded_rect(win_x, win_y, win_w, win_h, radius, border_color, fill_color);

    // Draw title bar
    int titlebar_h = 32;
    draw_filled_rounded_rect(win_x + 1, win_y + 1, win_w - 2, titlebar_h, radius - 2, titlebar_color, titlebar_color);

    // Draw window control buttons
    int btn_radius  = 7;
    int btn_border  = 0;
    int btn_y       = win_y + 16;
    int btn_x_start = win_x + 22;
    draw_filled_circle(btn_x_start, btn_y, btn_radius, btn_border, border_color, close_color);     // Close
    draw_filled_circle(btn_x_start + 24, btn_y, btn_radius, btn_border, border_color, min_color);  // Minimize
    draw_filled_circle(btn_x_start + 48, btn_y, btn_radius, btn_border, border_color, zoom_color); // Zoom

    // Optionally, draw window title
    // kputs("Terminal", win_x + 80, win_y + 8);
}

extern void tetris_main();

static bool process_cmd(const char *cmd) {
    // printf("Received command: %s\n", cmd);

    split_cmd(cmd);

    if (!strcmp(toks[0], "echo")) {
        for (int i = 1; i < toks_count && toks[i][0] != 0; i++) {
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
        printf("  print [string] - Print a string to the kernel debug console using syscall_print\n");
        printf("  help           - Show this help message\n");
        printf("  exit           - Exit the shell\n");
    } else if (!strcmp(toks[0], "tetris")) {
        tetris_main();
        clear();
    } else if (!strcmp(toks[0], "exit")) {
        printf("Exiting shell...\n");
        setcursor(false);
        return true;
    } else if (!strcmp(toks[0], "print")) {
        if (toks_count < 2) {
            printf("Usage: print [string]\n");
            return false;
        }
        printf("[not implemented]\n");
        // syscall_print(toks[1]);
    } else if (!strcmp(toks[0], "ls")) {
        ls_cmd(toks_count, toks);
    } else if (!strcmp(toks[0], "mkdir")) {
        if (toks_count < 2) {
            printf("Usage: mkdir [directory]\n");
            return false;
        }
        mkdir_cmd(toks_count, toks);
    } else if (!strcmp(toks[0], "rm")) {
        if (toks_count < 2) {
            printf("Usage: rm [file/directory]\n");
            return false;
        }
        printf("rm command not implemented yet.\n");
        // rm_cmd(toks_count, toks);
    } else if (!strcmp(toks[0], "readfile")) {
        if (toks_count < 2) {
            printf("Usage: readfile [file]\n");
            return false;
        }
        readfile_cmd(toks_count, toks);
    } else if (!strcmp(toks[0], "cd")) {
        if (toks_count < 2) {
            printf("Usage: cd [directory]\n");
            return false;
        }
        cd_cmd(toks_count, toks);
    } else if (!strcmp(toks[0], "pwd")) {
        pwd_cmd(toks_count, toks);
    } else if (!strcmp(toks[0], "lspci")) {
        lspci_cmd(toks_count, toks);
    } else if (toks[0][0] != 0) {
        printf("Unknown command: %s\n", toks[0]);
    }

    // Update cmd history
    if (cmd_history_count < 40) {
        strcpy(cmd_history[cmd_history_count++], cmd);
    } else {
        // Shift history up and add new command at the end
        for (int i = 1; i < 40; i++) {
            strcpy(cmd_history[i - 1], cmd_history[i]);
        }
        strcpy(cmd_history[39], cmd);
    }

    cmd_history_i = -1;

    return false;
}

// __attribute__((section(".text")))
void main(int argc, char **argv) {
    setcursor(true);

    printf("%s", SHELL_PROMPT);

    for (;;) {
        while (kbd_buffer_empty()) {
            // TODO: let the kernel know we're idle
            syscall_sleep(10);
        }

        kbd_event_t event;
        kbd_buffer_pop(&event);

        if (event.state != KEY_PRESSED) {
            continue;
        }
        char ch = _kbdev_get_ch(event.code, event.mods);

        if (ch == '\n') {
            printf("\n");

            cmdbuffer[cmd_buffer_i] = 0;

            if (process_cmd(cmdbuffer)) return;

            printf(SHELL_PROMPT);

            cmd_buffer_i = 0;

            continue;
        } else if (ch == '\b') {
            if (cmd_buffer_i > 0) {
                --cmd_buffer_i;
                backs();
            }

            continue;
        } else if (_kbdev_is_printable(event.code)) {
            // Leave space for end of string
            if (cmd_buffer_i < (CMD_BUFFER_MAX - 1)) {
                printf("%c", ch);
                cmdbuffer[cmd_buffer_i] = ch;
                ++cmd_buffer_i;
            }

            continue;
        }

        switch (event.code) {
        case KEY_UP: {
            if (cmd_history_count > 0 && cmd_history_i < cmd_history_count - 1) {
                ++cmd_history_i;
                // Clear current line
                while (cmd_buffer_i > 0) {
                    --cmd_buffer_i;
                    backs();
                }
                // Copy history command to cmdbuffer
                strcpy(cmdbuffer, cmd_history[cmd_history_count - 1 - cmd_history_i]);
                cmd_buffer_i = (int)strlen(cmdbuffer);
                // Print command
                printf("%s", cmdbuffer);
            }
            break;
        }
        case KEY_DOWN: {
            if (cmd_history_i >= 0) {
                --cmd_history_i;
                // Clear current line
                while (cmd_buffer_i > 0) {
                    --cmd_buffer_i;
                    backs();
                }
                if (cmd_history_i >= 0) {
                    // Copy history command to cmdbuffer
                    strcpy(cmdbuffer, cmd_history[cmd_history_count - 1 - cmd_history_i]);
                    cmd_buffer_i = (int)strlen(cmdbuffer);
                    // Print command
                    printf("%s", cmdbuffer);
                } else {
                    // Reset cmdbuffer
                    memset(cmdbuffer, 0, CMD_BUFFER_MAX);
                }
            }
            break;
        }
        }
    }
}