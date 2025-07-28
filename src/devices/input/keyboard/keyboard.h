#pragma once

void init_keyboard();

bool kbd_buffer_empty();
bool kbd_buffer_full();

void kbd_buffer_push(char ch);
int kbd_buffer_pop(char *data);