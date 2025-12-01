#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Types */

typedef struct {
    size_t fd;
} FILE;

typedef struct {
    long int pos;
} fpos_t;

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define BUFSIZ 1024
#define EOF (-1)
#define FOPEN_MAX 20
// TODO: should this be larger?
#define FILENAME_MAX 256
#define L_tmpnam 20

#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 0

#define TMP_MAX 32767

FILE *stderr;
FILE *stdin;
FILE *stdout;

/* Wide character input */

/* Console & Basic I/O */

int printf(const char *format, ...);

char *gets(char *str);
int backs();
int setcursor(bool curs);
int clear();

/* Files */

FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);