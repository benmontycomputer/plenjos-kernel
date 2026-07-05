#pragma once

#include "devices/manager.h"
#include "plenjos/dev/kbd.h"
#include "vfs/vfs.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int head;
    int tail;
    kbd_event_t buffer[KBD_BUFFER_SIZE];
    bool full;
} __attribute__((packed)) kbd_buffer_state_t;

bool kbd_buffer_empty(kbd_buffer_state_t *kbd_buffer_state);
bool kbd_buffer_full(kbd_buffer_state_t *kbd_buffer_state);

void kbd_buffer_push(kbd_buffer_state_t *kbd_buffer_state, kbd_event_t event);
int kbd_buffer_pop(kbd_buffer_state_t *kbd_buffer_state, kbd_event_t *data);

int kbd_register(kbd_buffer_state_t **out, endpoint_t **endpoint_out);