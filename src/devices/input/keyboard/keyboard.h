#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "plenjos/dev/kbd.h"
#include "vfs/vfs.h"

extern vfs_ops_block_t endpoint_ops_keyboard;

void init_keyboard();

bool kbd_buffer_empty();
bool kbd_buffer_full();

void kbd_buffer_push(kbd_event_t event);
int kbd_buffer_pop(kbd_event_t *data);