#include "stdio.h"
#include "string.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

// #include "memory/mm.h"

FILE *stderr = NULL;
FILE *stdout = NULL;

static bool cursor;

inline void printf_ch(char ch) {
    fputc((int)ch, stdout);
}

void printf_str(const char *str, uint16_t max_chars) {
    int n = strlen(str);
    if (n > max_chars) {
        n = max_chars;
    }
    fwrite(str, sizeof(char), n, stdout);
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

void printf_long(int64_t num, uint16_t min_digits) {
    uint64_t adj = 0;

    char vals[((8 * sizeof(int64_t)) + 9) / 10] = { 0 };

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

void printf_ulong(uint64_t num) {
    uint64_t adj = num;

    char vals[((8 * sizeof(uint64_t)) + 9) / 10] = { 0 };

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

    bool started_printing = false;
    for (int i = 0; i < 8; i++) {
        if (pad_to && (8 - i) > pad_to && ((adj & 0xf0000000) == 0) && !started_printing) {
            adj = (adj << 4);
            continue;
        }

        started_printing = true;

        printf_ch(hex_indices[(adj & 0xf0000000) >> 28]);

        adj = ((adj & 0x0fffffff) << 4);
    }
}

void printf_hex_long(uint64_t hex, uint16_t pad_to) {
    uint64_t adj = hex;

    printf_str("0x", 2);

    bool started_printing = false;
    for (int i = 0; i < 16; i++) {
        if (pad_to && (16 - i) > pad_to && ((adj & 0xf000000000000000) == 0) && !started_printing) {
            adj = (adj << 4);
            continue;
        }

        started_printing = true;

        printf_ch(hex_indices[(adj & 0xf000000000000000) >> 60]);

        adj = ((adj & 0x0fffffffffffffff) << 4);
    }
}

// %d or %i for integers, %f for floating-point, %s for string, %c for char, %x for hex, %p for pointer, %ld for signed
// long, %lu for unsigned long
int printf(const char *format, ...) {
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
                printf_str(va_arg(list, const char *), -1);
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
                    printf_long(va_arg(list, int64_t), 0);
                    break;
                case 'u':
                    printf_ulong(va_arg(list, uint64_t));
                    break;
                default:
                    va_arg(list, void *); // consume arg
                    break;
                }
                break;
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
                    va_arg(list, void *); // consume arg
                    break;
                }
                break;
            }
            default:
                printf_ch(format[i]);
                break;
            }
        } else {
            printf_ch(format[i]);
        }
    }

    return 0;
}

int backs() {
    fputc('\b', stdout);

    return 0;
}

int setcursor(bool curs) {
    fputs(curs ? "\x1b[?12h" : "\x1b[?12l", stdout);

    return 0;
}

int clear() {
    fputs("\x1b[2J", stdout);

    return 0;
}

int putchar(int charc) {
    fputc(charc, stdout);

    return charc;
}

int puts(const char *str) {
    // TODO: is this safe? Why is fputs's str a __restrict__ but not puts? (both are correct)
    fputs(str, stdout);

    return 0;
}