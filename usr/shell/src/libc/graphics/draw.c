#include "draw.h"

#include "../../uconsole.h"

static inline int abs(int a) {
    if (a < 0) a = -a;
    return a;
}

inline void _draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= fb_width || y < 0 || y >= fb_height) return;
    uint32_t *pixel_addr = (uint32_t *)(fb + (y * fb_scanline) + (x * fb_bytes_per_pixel));
    *pixel_addr = color;
}

void draw_pixel(int x, int y, uint32_t color) {
    _draw_pixel(x, y, color);
}

void draw_line(int x0, int y0, int x1, int y1, int border_thickness, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2; // error value e_xy

    for (;;) {
        // Draw pixel with border thickness
        for (int bx = -border_thickness / 2; bx <= border_thickness / 2; bx++) {
            for (int by = -border_thickness / 2; by <= border_thickness / 2; by++) {
                _draw_pixel(x0 + bx, y0 + by, color);
            }
        }

        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_rect(int x, int y, int w, int h, int border_thickness, uint32_t color) {
    // Top border
    draw_line(x, y, x + w - 1, y, border_thickness, color);
    // Bottom border
    draw_line(x, y + h - 1, x + w - 1, y + h - 1, border_thickness, color);
    // Left border
    draw_line(x, y, x, y + h - 1, border_thickness, color);
    // Right border
    draw_line(x + w - 1, y, x + w - 1, y + h - 1, border_thickness, color);
}
void draw_filled_rect(int x, int y, int w, int h, int border_thickness, uint32_t border_color, uint32_t fill_color) {
    // Draw filled area
    for (int i = 0; i < h; i++) {
        draw_line(x + border_thickness, y + i, x + w - border_thickness - 1, y + i, 1, fill_color);
    }
    // Draw border
    draw_rect(x, y, w, h, border_thickness, border_color);
}

void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    // Draw straight edges
    draw_line(x + r, y, x + w - r - 1, y, 1, color);         // Top edge
    draw_line(x + r, y + h - 1, x + w - r - 1, y + h - 1, 1, color); // Bottom edge
    draw_line(x, y + r, x, y + h - r - 1, 1, color);         // Left edge
    draw_line(x + w - 1, y + r, x + w - 1, y + h - r - 1, 1, color); // Right edge

    // Draw corners using midpoint circle algorithm
    int cx = r;
    int cy = 0;
    int err = 0;

    while (cx >= cy) {
        // Top-left corner
        _draw_pixel(x + r - cx, y + r - cy, color);
        _draw_pixel(x + r - cy, y + r - cx, color);
        // Top-right corner
        _draw_pixel(x + w - r + cx - 1, y + r - cy, color);
        _draw_pixel(x + w - r + cy - 1, y + r - cx, color);
        // Bottom-left corner
        _draw_pixel(x + r - cx, y + h - r + cy - 1, color);
        _draw_pixel(x + r - cy, y + h - r + cx - 1, color);
        // Bottom-right corner
        _draw_pixel(x + w - r + cx - 1, y + h - r + cy - 1, color);
        _draw_pixel(x + w - r + cy - 1, y + h - r + cx - 1, color);

        cy += 1;
        if (err <= 0) {
            err += 2 * cy + 1;
        } else {
            cx -= 1;
            err += 2 * (cy - cx) + 1;
        }
    }
}
void draw_filled_rounded_rect(int x, int y, int w, int h, int r, uint32_t border_color, uint32_t fill_color) {
    // Fill straight edges between corners
    for (int i = y + r; i < y + h - r; i++) {
        draw_line(x, i, x + w - 1, i, 1, fill_color);
    }
    for (int i = x + r; i < x + w - r; i++) {
        draw_line(i, y, i, y + h - 1, 1, fill_color);
    }

    // Fill corners (quarter circles)
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                // Top-left
                _draw_pixel(x + r + dx, y + r + dy, fill_color);
                // Top-right
                _draw_pixel(x + w - r - 1 + dx, y + r + dy, fill_color);
                // Bottom-left
                _draw_pixel(x + r + dx, y + h - r - 1 + dy, fill_color);
                // Bottom-right
                _draw_pixel(x + w - r - 1 + dx, y + h - r - 1 + dy, fill_color);
            }
        }
    }
    // Draw border
    draw_rounded_rect(x, y, w, h, r, border_color);
}

void draw_arc(int x0, int y0, int angle0, int angle1, int radius, int border_thickness, uint32_t color) {
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        for (int t = 0; t < border_thickness; t++) {
            _draw_pixel(x0 + x - t, y0 + y, color);
            _draw_pixel(x0 + y, y0 + x - t, color);
            _draw_pixel(x0 - y + t, y0 + x, color);
            _draw_pixel(x0 - x + t, y0 + y, color);
            _draw_pixel(x0 - x + t, y0 - y, color);
            _draw_pixel(x0 - y + t, y0 - x, color);
            _draw_pixel(x0 + y, y0 - x + t, color);
            _draw_pixel(x0 + x - t, y0 - y, color);
        }

        y += 1;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x -= 1;
            err += 2 * (y - x) + 1;
        }
    }
}
void draw_filled_arc(int x0, int y0, int angle0, int angle1, int radius, int border_thickness, uint32_t border_color, uint32_t fill_color) {
    // Draw filled area
    /*for (int r = 0; r < radius - border_thickness; r++) {
        for (int angle = angle0; angle <= angle1; angle++) {
            double rad = angle * 3.14159265358979323846 / 180.0;
            int x = x0 + (int)(r * cos(rad));
            int y = y0 + (int)(r * sin(rad));
            _draw_pixel(x, y, fill_color);
        }
    } */
    // Draw border
    draw_arc(x0, y0, angle0, angle1, radius, border_thickness, border_color);
}

void draw_circle(int x0, int y0, int radius, int border_thickness, uint32_t color) {
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        for (int t = 0; t < border_thickness; t++) {
            _draw_pixel(x0 + x - t, y0 + y, color);
            _draw_pixel(x0 + y, y0 + x - t, color);
            _draw_pixel(x0 - y + t, y0 + x, color);
            _draw_pixel(x0 - x + t, y0 + y, color);
            _draw_pixel(x0 - x + t, y0 - y, color);
            _draw_pixel(x0 - y + t, y0 - x, color);
            _draw_pixel(x0 + y, y0 - x + t, color);
            _draw_pixel(x0 + x - t, y0 - y, color);
        }

        y += 1;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x -= 1;
            err += 2 * (y - x) + 1;
        }
    }
}
void draw_filled_circle(int x0, int y0, int radius, int border_thickness, uint32_t border_color, uint32_t fill_color) {
    // Draw filled circle
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                _draw_pixel(x0 + x, y0 + y, fill_color);
            }
        }
    }
    // Draw border
    draw_circle(x0, y0, radius, border_thickness, border_color);
}

void draw_char(unsigned short int c, int cx, int cy, uint32_t fg, uint32_t bg) {
    if (c < 32 || c > 126) return; // Unsupported character

    // Not implemented
}
void draw_string(const char *str, int cx, int cy, uint32_t fg, uint32_t bg) {
    // Not implemented
}
void draw_hex(uint64_t hex, int cx, int cy, uint32_t fg, uint32_t bg) {
    // Not implemented
}
void clear_screen(uint32_t bg) {
    for (int y = 0; y < fb_height; y++) {
        for (int x = 0; x < fb_width; x++) {
            _draw_pixel(x, y, bg);
        }
    }
}