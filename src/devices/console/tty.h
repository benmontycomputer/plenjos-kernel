#pragma once

#include "stdint.h"

typedef int (*console_io_func_t)(void *func_args);
typedef int (*console_io_func_t)(void *func_args);

struct tty {
    console_io_func_t in;
    console_io_func_t out;
    void *in_data;
    void *out_data;

    // For interfacing with the in func
    char in_buf[16];

    // NOTE: this holds at most one char unless that char is the start of an escape sequence
    char out_buf[16];
};

extern struct tty tty_main;
int setup_main_tty();