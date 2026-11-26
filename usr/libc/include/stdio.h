#ifndef _STDIO_H
#define _STDIO_H 1

#include "plenjos/limits.h"
#include "stddef.h"
#include "sys/types.h" // POSIX only; should be gated at some point

#include <stdbool.h>
#include <stdint.h>

/* Types */

typedef struct {
    int fd;
    int flags; // __FILE_EOF | __FILE_ERR
} FILE;

#define __FILE_EOF 0x1
#define __FILE_ERR 0x2

typedef struct {
    long int pos;
} fpos_t;

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define BUFSIZ       1024
#define EOF          (-1)
#define FOPEN_MAX    20
// TODO: should this be larger?
// This INCLUDES the null terminator (i.e., max filename strlen is FILENAME_MAX - 1)
#define FILENAME_MAX 256
#define L_tmpnam     20

// TODO: do we want to move these to kernel headers?
#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 0

#define TMP_MAX 32767

extern FILE *stderr;
extern FILE *stdin;
extern FILE *stdout;

/* Wide character input */

/* Console & Basic I/O */

int printf(const char *format, ...);
int putchar(int charc);
int puts(const char *str);

char *gets(char *str);
int backs(void);
int setcursor(bool curs);
int clear(void);

char getchar(void);

/* Files */

FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int fgetc(FILE *stream);
int fputc(int c, FILE *stream);
int fputs(const char *restrict str, FILE *restrict stream);
ssize_t getdelim(char **restrict lineptr, size_t *restrict n, int delim, FILE *restrict stream);
ssize_t getline(char **restrict lineptr, size_t *restrict n, FILE *restrict stream);
int ferror(FILE *stream);
int feof(FILE *stream);
void clearerr(FILE *stream);

#endif /* _STDIO_H */