#pragma once

#include <stdint.h>

/* struct fb_info {
    char *fb_ptr;
    int fb_scanline;
    int fb_width;
    int fb_height;
    int fb_bytes_per_pixel;
} __attribute__((packed));
typedef struct fb_info fb_info_t; */

#include "../../src/syscall/syscall.h"
#include "../../src/devices/input/keyboard/keyboard.h"

extern fb_info_t fb_info;
extern kbd_buffer_state_t *kbd_buffer_state;

#define fb ((char *)fb_info.fb_ptr)
#define fb_scanline (fb_info.fb_scanline)
#define fb_width (fb_info.fb_width)
#define fb_height (fb_info.fb_height)
#define fb_bytes_per_pixel (fb_info.fb_bytes_per_pixel)

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