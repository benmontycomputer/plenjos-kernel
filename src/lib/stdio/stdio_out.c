#include "devices/kconsole.h"
#include "kernel.h"
#include "lib/lock.h"
#include "lib/serial.h"
#include "lib/stdio.h"
#include "memory/mm.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

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

void printf_str(const char *str, uint16_t max_chars) {
    while (*str && max_chars--) {
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
        binary    = binary >> 1;
    }

    while (i) {
        printf_ch(vals[--i]);
    }
}

// precision is minimum number of digits to print (pads with leading zeros)
void printf_int(int num, uint16_t min_digits) {
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
        vals[i++]  = hex_indices[adj % 10];
        adj       /= 10;
    }

    if (min_digits > i) {
        while (min_digits > i) {
            vals[i++] = '0';
        }
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
        vals[i++]  = hex_indices[adj % 10];
        adj       /= 10;
    }

    while (i) {
        printf_ch(vals[--i]);
    }
}

void printf_hex(uint32_t hex, uint16_t pad_to) {
    uint64_t adj = (uint64_t)hex;

    printf_str("0x", 2);

    for (int i = 0; i < 8; i++) {
        if (pad_to && (8 - i) > pad_to && ((adj & 0xf0000000) == 0)) {
            adj = (adj << 4);
            continue;
        }

        printf_ch(hex_indices[(adj & 0xf0000000) >> 28]);

        adj = ((adj & 0x0fffffff) << 4);
    }
}

void printf_hex_long(uint64_t hex, uint16_t pad_to) {
    uint64_t adj = hex;

    printf_str("0x", 2);

    for (int i = 0; i < 16; i++) {
        if (pad_to && (16 - i) > pad_to && ((adj & 0xf000000000000000) == 0)) {
            adj = (adj << 4);
            continue;
        }
        printf_ch(hex_indices[(adj & 0xf000000000000000) >> 60]);

        adj = ((adj & 0x0fffffffffffffff) << 4);
    }
}

// TODO: re-enable when SSE is available
/* void printf_float(double f, uint8_t precision) {
    int int_part = (int)f;
    printf_int(int_part, 0);

    printf_ch('.');

    double frac_part = f - (double)int_part;

    for (uint8_t i = 0; i < precision; i++) {
        frac_part *= 10.0;
        int digit = (int)frac_part;
        printf_ch('0' + digit);
        frac_part -= (double)digit;
    }
} */

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
                printf_int(va_arg(list, int), 0);
                break;
            }
            case 'i': {
                printf_int(va_arg(list, int), 0);
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
                printf_str("0b", 2);
                printf_binary(va_arg(list, uint64_t));
                break;
            }
            case 'f': {
                // printf_float(va_arg(list, double), 6);
                break;
            }
            case 'c': {
                printf_ch(va_arg(list, int));
                break;
            }
            case 's': {
                printf_str(va_arg(list, const char *), 0);
                break;
            }
            case 'x': {
                printf_hex((uint32_t)va_arg(list, uint32_t), 0);
                break;
            }
            case 'p': {
                printf_hex_long((uint64_t)va_arg(list, void *), 0);
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
            case '.':
            case '0': {
                uint16_t precision = 0;
                // Precision specifier
                i++;
                while (format[i] >= '0' && format[i] <= '9') {
                    precision = (precision * 10) + (format[i] - '0');
                    i++;
                }

                switch (format[i]) {
                case 's':
                    printf_str(va_arg(list, const char *), precision);
                    break;
                case 'd':
                    printf_int(va_arg(list, int), precision);
                    break;
                case 'f':
                    // printf_float(va_arg(list, double), (uint8_t)precision);
                    break;
                case 'x':
                    printf_hex((uint32_t)va_arg(list, uint32_t), precision);
                    break;
                default:
                    break;
                }
                break;
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

    uint8_t *p     = (uint8_t *)fb;
    const size_t n = fb_height * fb_scanline;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)0;
    }

    console_pos = 0;

    release_console();

    return 0;
}