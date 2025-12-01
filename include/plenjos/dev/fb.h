#pragma once

struct fb_info {
    char *fb_ptr;
    int fb_scanline;
    int fb_width;
    int fb_height;
    int fb_bytes_per_pixel;
} __attribute__((packed));
typedef struct fb_info fb_info_t;