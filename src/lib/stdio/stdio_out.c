#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "devices/kconsole.h"
#include "lib/stdio.h"
#include "lib/lock.h"

#include "memory/mm.h"

#include "kernel.h"
#include "lib/serial.h"

static mutex console_access = MUTEX_INIT;

extern char *fb;
extern int fb_scanline, fb_width, fb_height, fb_bytes_per_pixel;

#define CONSOLE_W() (fb_width / FONT_W)
#define CONSOLE_H() (fb_height / FONT_H)

static uint64_t console_pos;

#define GET_CONSOLE_X(pos) (pos % CONSOLE_W())
#define GET_CONSOLE_Y(pos) (pos / CONSOLE_W())

static bool cursor;

void request_console() {
    mutex_lock(&console_access);
}

void release_console() {
    mutex_unlock(&console_access);
}

void scroll_console() {
    // Scroll

    uint64_t offset = fb_scanline * FONT_H;
    uint64_t i;

    for (i = 0; i < ((uint64_t)fb_scanline * (uint64_t)fb_height) - offset; i++) {
        fb[i] = fb[i + offset];
    }

    for (; i < (uint64_t)fb_scanline * (uint64_t)fb_height; i++) {
        fb[i] = 0;
    }

    console_pos -= CONSOLE_W();
}

void printf_ch(char ch) {
    if (debug_serial) {
        write_serial(ch);
        return;
    }

    if ((int)GET_CONSOLE_Y(console_pos) >= CONSOLE_H()) scroll_console();

    if (ch == '\n') {
        // Remove the cursor (if any)
        if (cursor) kputchar(' ', GET_CONSOLE_X(console_pos), GET_CONSOLE_Y(console_pos), 0xFFFFFF, 0x000000);
        console_pos += (CONSOLE_W() - GET_CONSOLE_X(console_pos));
        if ((int)GET_CONSOLE_Y(console_pos) >= CONSOLE_H()) scroll_console();
    } else {
        kputchar(ch, GET_CONSOLE_X(console_pos), GET_CONSOLE_Y(console_pos), 0xFFFFFF, 0x000000);
        console_pos++;
    }

    if (cursor) kputchar(' ', GET_CONSOLE_X(console_pos), GET_CONSOLE_Y(console_pos), 0x000000, 0xFFFFFF);
}

void printf_str(const char *str) {
    while (*str) {
        printf_ch(*str);

        str++;
    }
}

static char hex_indices[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

void printf_binary(uint64_t binary) {
    char vals[64];

    int i = 0;

    while (binary || !i) {
        vals[i++] = hex_indices[binary & 0b1];
        binary = binary >> 1;
    }

    while (i) {
        printf_ch(vals[--i]);
    }
}

void printf_int(int num) {
    uint32_t adj = 0;

    char vals[((8 * sizeof(int)) + 9) / 10] = { 0 };

    if (num < 0) {
        adj = -num;
        printf_ch('-');
    } else {
        adj = num;
    }

    int i = 0;

    while (adj || !i) {
        vals[i++] = hex_indices[adj % 10];
        adj /= 10;
    }

    while (i) {
        printf_ch(vals[--i]);
    }
}

void printf_uint(uint32_t num) {
    uint32_t adj = num;

    char vals[((8 * sizeof(uint32_t)) + 9) / 10] = { 0 };

    int i = 0;

    while (adj || !i) {
        vals[i++] = hex_indices[adj % 10];
        adj /= 10;
    }

    while (i) {
        printf_ch(vals[--i]);
    }
}

void printf_hex(uint32_t hex) {
    uint64_t adj = (uint64_t)hex;

    printf_str("0x");

    for (int i = 0; i < 8; i++) {
        printf_ch(hex_indices[(adj & 0xf0000000) >> 28]);

        adj = ((adj & 0x0fffffff) << 4);
    }
}

void printf_hex_long(uint64_t hex) {
    uint64_t adj = hex;

    printf_str("0x");

    for (int i = 0; i < 16; i++) {
        printf_ch(hex_indices[(adj & 0xf000000000000000) >> 60]);

        adj = ((adj & 0x0fffffffffffffff) << 4);
    }
}

// %d or %i for integers, %f for floating-point, %s for string, %c for char, %x for hex, %p for pointer, %ld for signed
// long, %lu for unsigned long
int printf(const char *format, ...) {
    request_console();

    va_list list;
    va_start(list, format);

    for (int i = 0; format[i]; i++) {
        if (format[i] == '%') {
            i++;

            switch (format[i]) {
            case 'd': {
                printf_int(va_arg(list, int));
                break;
            }
            case 'i': {
                printf_int(va_arg(list, int));
                break;
            }
            case 'u': {
                printf_uint((uint32_t)va_arg(list, uint32_t));
                break;
            }
            case 'b': {
                printf_binary(va_arg(list, uint64_t));
                break;
            }
            case 'B': {
                printf_str("0b");
                printf_binary(va_arg(list, uint64_t));
                break;
            }
            case 'f': {
                break;
            }
            case 'c': {
                printf_ch(va_arg(list, int));
                break;
            }
            case 's': {
                printf_str(va_arg(list, const char *));
                break;
            }
            case 'x': {
                printf_hex((uint32_t)va_arg(list, uint32_t));
                break;
            }
            case 'p': {
                printf_hex_long((uint64_t)va_arg(list, void *));
                break;
            }
            case 'l': {
                i++;
                switch (format[i]) {
                case 'd':
                    break;
                case 'u':
                    break;
                }
            }
            }
        } else {
            printf_ch(format[i]);
        }
    }

    release_console();

    return 0;
}

int backs() {
    request_console();

    kputchar(' ', GET_CONSOLE_X(console_pos), GET_CONSOLE_Y(console_pos), 0xFFFFFF, 0x000000);
    console_pos--;
    kputchar(' ', GET_CONSOLE_X(console_pos), GET_CONSOLE_Y(console_pos), 0xFFFFFF, 0x000000);
    if (cursor) kputchar(' ', GET_CONSOLE_X(console_pos), GET_CONSOLE_Y(console_pos), 0x000000, 0xFFFFFF);

    release_console();

    return 0;
}

int setcursor(bool curs) {
    request_console();

    cursor = curs;

    kputchar(' ', GET_CONSOLE_X(console_pos), GET_CONSOLE_Y(console_pos), 0x000000, 0xFFFFFF);

    release_console();

    return 0;
}

int clear() {
    request_console();

    uint8_t *p = (uint8_t *)fb;
    const size_t n = fb_height * fb_scanline;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)0;
    }

    console_pos = 0;

    release_console();

    return 0;
}