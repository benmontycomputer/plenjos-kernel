#pragma once

#include <stdbool.h>
#include <stdint.h>

void init_keyboard();

bool kbd_buffer_empty();
bool kbd_buffer_full();

void kbd_buffer_push(char ch);
int kbd_buffer_pop(char *data);

#define KBD_BUFFER_SIZE 128

typedef struct {
    int head;
    int tail;
    char buffer[KBD_BUFFER_SIZE];
    bool full;
} __attribute__((packed)) kbd_buffer_state_t;