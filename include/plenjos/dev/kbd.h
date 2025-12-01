#pragma once

#include <stdbool.h>
#include <stdint.h>

#define KBD_BUFFER_SIZE 128

typedef struct {
    int head;
    int tail;
    char buffer[KBD_BUFFER_SIZE];
    bool full;
} __attribute__((packed)) kbd_buffer_state_t;