#include <stdbool.h>
#include <stdint.h>
#include "stddef.h"

#include "keyboard.h"
#include "plenjos/dev/kbd.h"

extern kbd_buffer_state_t *kbd_buffer_state;

static void advance_tail() {
    kbd_buffer_state->tail = (kbd_buffer_state->tail + 1) % KBD_BUFFER_SIZE;
}

static void advance_head() {
    kbd_buffer_state->head = (kbd_buffer_state->head + 1) % KBD_BUFFER_SIZE;
}

bool kbd_buffer_empty() {
    return (!kbd_buffer_state->full) && (kbd_buffer_state->head == kbd_buffer_state->tail);
}

bool kbd_buffer_full() {
    return kbd_buffer_state->full;
}

void kbd_buffer_push(char ch) {
    kbd_buffer_state->buffer[kbd_buffer_state->head] = ch;

    if (kbd_buffer_state->full) {
        advance_tail();
    }
    advance_head();

    kbd_buffer_state->full = (kbd_buffer_state->head == kbd_buffer_state->tail);
}

int kbd_buffer_pop(char *data) {
    if (kbd_buffer_empty()) return -1;

    *data = kbd_buffer_state->buffer[kbd_buffer_state->tail];

    advance_tail();

    kbd_buffer_state->full = false;

    return 0;
}