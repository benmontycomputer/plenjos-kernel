#include <stdint.h>
#include <stdbool.h>

#include "devices/input/keyboard/keyboard.h"

#include "lib/stdio.h"

kbd_buffer_state_t kbd_buffer_state;

static void advance_tail() {
    kbd_buffer_state.tail = (kbd_buffer_state.tail + 1) % KBD_BUFFER_SIZE;
}

static void advance_head() {
    kbd_buffer_state.head = (kbd_buffer_state.head + 1) % KBD_BUFFER_SIZE;
}

bool kbd_buffer_empty() {
    return (!kbd_buffer_state.full) && (kbd_buffer_state.head == kbd_buffer_state.tail);
}

bool kbd_buffer_full() {
    return kbd_buffer_state.full;
}

void kbd_buffer_push(kbd_event_t event) {
    kbd_buffer_state.buffer[kbd_buffer_state.head] = event;

    if (kbd_buffer_state.full) {
        advance_tail();
    }
    advance_head();

    kbd_buffer_state.full = (kbd_buffer_state.head == kbd_buffer_state.tail);
}

int kbd_buffer_pop(kbd_event_t *data) {
    if (kbd_buffer_empty()) return -1;

    *data = kbd_buffer_state.buffer[kbd_buffer_state.tail];

    advance_tail();

    kbd_buffer_state.full = false;

    return 0;
}

void init_keyboard_interface() {
    kbd_buffer_state.head = 0;
    kbd_buffer_state.tail = 0;

    kbd_buffer_state.full = false;

    printf("kbd: %p\n", &kbd_buffer_state);
}