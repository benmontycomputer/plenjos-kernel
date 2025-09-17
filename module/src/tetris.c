#include "lib/common.h"
#include "lib/string.h"
#include "uconsole.h"
#include <stdbool.h>
#include <stdint.h>

// http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
// The following functions define a portable implementation of rand and srand.

static unsigned long int next = 1; // NB: "unsigned long int" is assumed to be 32 bits wide

int rand(void) // RAND_MAX assumed to be 32767
{
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

void srand(unsigned int seed) {
    next = seed;
}

// Change these to match your framebuffer settings
// #define FB_WIDTH  800
// #define FB_HEIGHT 600
// #define FB_BPP    4 // bytes per pixel (32-bit)
#define FB_WIDTH fb_width
#define FB_HEIGHT fb_height
#define FB_BPP fb_bytes_per_pixel
#define BLOCK_SIZE 24

// Framebuffer pointer (should be mapped to actual framebuffer address)
// extern uint8_t *framebuffer;
#define framebuffer fb

// Tetris board settings
#define BOARD_W 10
#define BOARD_H 20

// Colors (ARGB)
#define COLOR_BG 0xFF222222
#define COLOR_GRID 0xFF444444
#define COLOR_BLOCKS 0xFF00FF00

// Bit array to track dirty cells
static uint8_t dirty[(BOARD_H * BOARD_W + 7) / 8] = { [0 ...((BOARD_H * BOARD_W + 7) / 8) - 1] = 0xff };

static inline bool is_cell_dirty(int x, int y) {
    if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return false;
    return (dirty[(y * BOARD_W + x) / 8] & (1 << ((y * BOARD_W + x) % 8))) != 0;
}

static inline void clear_cell_dirty(int x, int y) {
    if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return;
    dirty[(y * BOARD_W + x) / 8] &= ~(1 << ((y * BOARD_W + x) % 8));
}

static inline void mark_cell_as_dirty(int x, int y) {
    if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return;
    dirty[(y * BOARD_W + x) / 8] |= (1 << ((y * BOARD_W + x) % 8));
}

static inline void mark_row_as_dirty(int row) {
    if (row < 0 || row >= BOARD_H) return;
    for (int x = 0; x < BOARD_W; ++x) {
        mark_cell_as_dirty(x, row);
    }
}

// Tetromino shapes
const int tetrominoes[7][4][4][4] = {
    // I
    { { { 0, 0, 0, 0 }, { 1, 1, 1, 1 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 0, 1, 0 }, { 0, 0, 1, 0 }, { 0, 0, 1, 0 }, { 0, 0, 1, 0 } },
      { { 0, 0, 0, 0 }, { 1, 1, 1, 1 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 1, 0, 0 }, { 0, 1, 0, 0 }, { 0, 1, 0, 0 }, { 0, 1, 0, 0 } } },
    // O
    { { { 0, 0, 0, 0 }, { 0, 1, 1, 0 }, { 0, 1, 1, 0 }, { 0, 0, 0, 0 } },
      { { 0, 0, 0, 0 }, { 0, 1, 1, 0 }, { 0, 1, 1, 0 }, { 0, 0, 0, 0 } },
      { { 0, 0, 0, 0 }, { 0, 1, 1, 0 }, { 0, 1, 1, 0 }, { 0, 0, 0, 0 } },
      { { 0, 0, 0, 0 }, { 0, 1, 1, 0 }, { 0, 1, 1, 0 }, { 0, 0, 0, 0 } } },
    // T
    { { { 0, 0, 0, 0 }, { 1, 1, 1, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 1, 0, 0 }, { 1, 1, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 1, 0, 0 }, { 1, 1, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 1, 0, 0 }, { 0, 1, 1, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 0 } } },
    // S
    { { { 0, 0, 0, 0 }, { 0, 1, 1, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 } },
      { { 1, 0, 0, 0 }, { 1, 1, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 0, 0, 0 }, { 0, 1, 1, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 } },
      { { 1, 0, 0, 0 }, { 1, 1, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 0 } } },
    // Z
    { { { 0, 0, 0, 0 }, { 1, 1, 0, 0 }, { 0, 1, 1, 0 }, { 0, 0, 0, 0 } },
      { { 0, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 0, 0, 0 }, { 1, 1, 0, 0 }, { 0, 1, 1, 0 }, { 0, 0, 0, 0 } },
      { { 0, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 0, 0, 0, 0 } } },
    // J
    { { { 0, 0, 0, 0 }, { 1, 1, 1, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 } },
      { { 0, 1, 0, 0 }, { 0, 1, 0, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 } },
      { { 1, 0, 0, 0 }, { 1, 1, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 1, 1, 0 }, { 0, 1, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 0 } } },
    // L
    { { { 0, 0, 0, 0 }, { 1, 1, 1, 0 }, { 1, 0, 0, 0 }, { 0, 0, 0, 0 } },
      { { 1, 1, 0, 0 }, { 0, 1, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 0, 1, 0 }, { 1, 1, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
      { { 0, 1, 0, 0 }, { 0, 1, 0, 0 }, { 0, 1, 1, 0 }, { 0, 0, 0, 0 } } }
};

typedef struct {
    int type;
    int rotation;
    int x, y;
} Tetromino;

uint8_t board[BOARD_H][BOARD_W];

void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int px = x + i;
            int py = y + j;
            if (px < 0 || px >= FB_WIDTH || py < 0 || py >= FB_HEIGHT) continue;
            uint32_t *p = (uint32_t *)(framebuffer + (py * FB_WIDTH + px) * FB_BPP);
            *p = color;
        }
    }
}

void fb_clear(uint32_t color) {
    for (int y = 0; y < FB_HEIGHT; ++y) {
        for (int x = 0; x < FB_WIDTH; ++x) {
            uint32_t *p = (uint32_t *)(framebuffer + (y * FB_WIDTH + x) * FB_BPP);
            *p = color;
        }
    }
}

void draw_board() {
    // fb_clear(COLOR_BG);
    // Draw grid
    for (int y = 0; y < BOARD_H; ++y) {
        for (int x = 0; x < BOARD_W; ++x) {
            if (is_cell_dirty(x, y)) {
                uint32_t color = board[y][x] ? COLOR_BLOCKS : COLOR_GRID;
                fb_draw_rect(100 + x * BLOCK_SIZE, 50 + y * BLOCK_SIZE, BLOCK_SIZE - 2, BLOCK_SIZE - 2, color);

                // Clear dirty flag
                clear_cell_dirty(x, y);
            }
        }
    }
}

void draw_tetromino(const Tetromino *t, uint32_t color) {
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            // Mark cell as dirty
            mark_cell_as_dirty(t->x + i, t->y + j);
            if (tetrominoes[t->type][t->rotation][j][i]) {
                int bx = t->x + i;
                int by = t->y + j;
                if (bx >= 0 && bx < BOARD_W && by >= 0 && by < BOARD_H) {
                    fb_draw_rect(100 + bx * BLOCK_SIZE, 50 + by * BLOCK_SIZE, BLOCK_SIZE - 2, BLOCK_SIZE - 2, color);
                }
            }
        }
    }
}

bool check_collision(const Tetromino *t, int dx, int dy, int dr) {
    int rot = (t->rotation + dr) % 4;
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            if (tetrominoes[t->type][rot][j][i]) {
                int bx = t->x + dx + i;
                int by = t->y + dy + j;
                if (bx < 0 || bx >= BOARD_W || by < 0 || by >= BOARD_H) return true;
                if (board[by][bx]) return true;
            }
        }
    }
    return false;
}

void place_tetromino(const Tetromino *t) {
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            if (tetrominoes[t->type][t->rotation][j][i]) {
                int bx = t->x + i;
                int by = t->y + j;
                if (bx >= 0 && bx < BOARD_W && by >= 0 && by < BOARD_H) { board[by][bx] = 1; }
            }
        }
    }
}

void clear_lines() {
    for (int y = BOARD_H - 1; y >= 0; --y) {
        bool full = true;
        for (int x = 0; x < BOARD_W; ++x) {
            if (!board[y][x]) {
                full = false;
                break;
            }
        }
        if (full) {
            for (int yy = y; yy > 0; --yy) {
                memcpy(board[yy], board[yy - 1], BOARD_W);
                mark_row_as_dirty(yy);
            }
            mark_row_as_dirty(0);
            memset(board[0], 0, BOARD_W);
            ++y; // check same line again
        }
    }
}

// Dummy input function (replace with actual input handling)
int get_input() {
    // Return 0: none, 1: left, 2: right, 3: rotate, 4: down
    if (kbd_buffer_empty()) return 0;
    char ch;
    kbd_buffer_pop(&ch);
    if (ch == 'a') return 1;
    if (ch == 'd') return 2;
    if (ch == 'w') return 3;
    if (ch == 's') return 4;
    return 0;
}

void tetris_main() {
    fb_clear(COLOR_BG);

    memset(board, 0, sizeof(board));
    Tetromino current = { 0, 0, BOARD_W / 2 - 2, 0 };
    int tick = 0;
    while (1) {
        draw_board();
        draw_tetromino(&current, COLOR_BLOCKS);

        int input = get_input();
        if (input == 1 && !check_collision(&current, -1, 0, 0)) current.x -= 1;
        if (input == 2 && !check_collision(&current, 1, 0, 0)) current.x += 1;
        if (input == 3 && !check_collision(&current, 0, 0, 1)) current.rotation = (current.rotation + 1) % 4;
        if (input == 4 && !check_collision(&current, 0, 1, 0)) current.y += 1;

        if (tick++ % 20 == 0) {
            if (!check_collision(&current, 0, 1, 0)) {
                current.y += 1;
            } else {
                place_tetromino(&current);
                clear_lines();
                current.type = rand() % 7;
                current.rotation = 0;
                current.x = BOARD_W / 2 - 2;
                current.y = 0;
                if (check_collision(&current, 0, 0, 0)) {
                    // Game over
                    break;
                }
            }
        }
        // usleep(20000); // ~50 FPS
        syscall(SYSCALL_SLEEP, 20, 0, 0, 0, 0);
    }
}