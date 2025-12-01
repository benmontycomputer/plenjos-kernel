#pragma once

#include <stdint.h>
#include <stdbool.h>

int printf(const char *format, ...);

char *gets(char *str);

int backs();

int setcursor(bool curs);

int clear();