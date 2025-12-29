#pragma once

#include <stdint.h>

void draw_pixel(int x, int y, uint32_t color);

void draw_line(int x0, int y0, int x1, int y1, int border_thickness, uint32_t color);

void draw_rect(int x, int y, int w, int h, int border_thickness, uint32_t color);
void draw_filled_rect(int x, int y, int w, int h, int border_thickness, uint32_t border_color, uint32_t fill_color);

void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void draw_filled_rounded_rect(int x, int y, int w, int h, int r, uint32_t border_color, uint32_t fill_color);

void draw_arc(int x0, int y0, int angle0, int angle1, int radius, int border_thickness, uint32_t color);
void draw_filled_arc(int x0, int y0, int angle0, int angle1, int radius, int border_thickness, uint32_t border_color, uint32_t fill_color);

void draw_circle(int x0, int y0, int radius, int border_thickness, uint32_t color);
void draw_filled_circle(int x0, int y0, int radius, int border_thickness, uint32_t border_color, uint32_t fill_color);

void draw_char(unsigned short int c, int cx, int cy, uint32_t fg, uint32_t bg);
void draw_string(const char *str, int cx, int cy, uint32_t fg, uint32_t bg);
void draw_hex(uint64_t hex, int cx, int cy, uint32_t fg, uint32_t bg);
void clear_screen(uint32_t bg);