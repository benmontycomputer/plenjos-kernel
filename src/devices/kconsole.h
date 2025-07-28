#ifndef _KERNEL_KCONSOLE_H
#define _KERNEL_KCONSOLE_H

#include <stdint.h>

#define FONT_W 9
#define FONT_H 16

// Uses PSF1 format
void kputchar(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32_t fg, uint32_t bg);

void kputs(const char *str, int cx, int cy);
void kputhex(uint64_t hex, int cx, int cy);

#endif