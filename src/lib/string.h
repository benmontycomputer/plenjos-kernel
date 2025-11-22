#pragma once

#include <stddef.h>

int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
char *strncpy(char *dest, const char *src, size_t n);
char *strtok(char *str, const char *delim);