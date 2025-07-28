#include <stdint.h>
#include <stdbool.h>

#include "devices/input/keyboard/keyboard.h"
#include "devices/input/keyboard/ps2kbd.h"

#define KBD_BUFFER_SIZE 128

// the head is where the next char goes; the tail is the element added least recently
static int kbd_buffer_head, kbd_buffer_tail;
static char kbd_buffer[KBD_BUFFER_SIZE];

static bool full;

static void advance_tail() {
    kbd_buffer_tail = (kbd_buffer_tail + 1) % KBD_BUFFER_SIZE;
}

static void advance_head() {
    kbd_buffer_head = (kbd_buffer_head + 1) % KBD_BUFFER_SIZE;
}

bool kbd_buffer_empty() {
    return (!full) && (kbd_buffer_head == kbd_buffer_tail);
}

bool kbd_buffer_full() {
    return full;
}

void kbd_buffer_push(char ch) {
    kbd_buffer[kbd_buffer_head] = ch;

    if (full) {
        advance_tail();
    }
    advance_head();
    
    full = (kbd_buffer_head == kbd_buffer_tail);
}

int kbd_buffer_pop(char *data) {
    if (kbd_buffer_empty()) return -1;

    *data = kbd_buffer[kbd_buffer_tail];

    advance_tail();

    full = false;

    return 0;
}

void init_keyboard() {
    kbd_buffer_head = 0;
    kbd_buffer_tail = 0;

    full = false;

    init_ps2_keyboard();
}