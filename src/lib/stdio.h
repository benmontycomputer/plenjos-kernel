#pragma once

#include <stdint.h>
#include <stdbool.h>

// Use in IRQ handlers in case a process has STDIO locked when interrupt happens
int printf_nolock(const char *format, ...);

int printf(const char *format, ...);

char *gets(char *str);

int backs();

int setcursor(bool curs);

int clear();