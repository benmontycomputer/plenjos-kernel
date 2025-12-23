#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "plenjos/dev/kbd.h"

void init_keyboard_interface();

bool kbd_buffer_empty();
bool kbd_buffer_full();

void kbd_buffer_push(kbd_event_t event);
int kbd_buffer_pop(kbd_event_t *data);