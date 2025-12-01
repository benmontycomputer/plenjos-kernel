#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Console & Basic I/O */

int printf(const char *format, ...);

char *gets(char *str);
int backs();
int setcursor(bool curs);
int clear();

/* Files */

typedef struct {
    size_t fd;
} FILE;

FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);