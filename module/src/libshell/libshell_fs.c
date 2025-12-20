#include "libshell/libshell.h"

#include "dirent.h"

#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"

#include "sys/stat.h"
#include "sys/types.h"

#include "errno.h"

int ls_cmd(int argc, char argv[][CMD_BUFFER_MAX]) {
    if (argc < 2) {
        argv[1][0] = '.';
        argv[1][1] = '\0';
    }

    DIR *dir = opendir(argv[1]);
    if (!dir) {
        switch (errno) {
        case ENOENT:
            printf("ls: directory %s does not exist.\n", argv[1]);
            break;
        case ENOTDIR:
            printf("ls: %s is not a directory.\n", argv[1]);
            break;
        case EACCES:
            printf("ls: permission denied to open directory %s.\n", argv[1]);
            break;
        default:
            printf("ls: failed to open directory %s (errno %d).\n", argv[1], errno);
            break;
        }
        return -1;
    }

    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    switch (errno) {
    case 0:
        break;
    case EBADF:
        printf("ls: directory stream for %s is not valid.\n", argv[1]);
        closedir(dir);
        return -1;
    default:
        printf("ls: error reading directory %s (errno %d).\n", argv[1], errno);
        closedir(dir);
        return -1;
    }

    closedir(dir);
    return 0;
}

int mkdir_cmd(int argc, char argv[][CMD_BUFFER_MAX]) {
    if (argc < 2) {
        errno = EINVAL;
        return -1;
    }

    int res = mkdir(argv[1], 0755);
    if (res < 0) {
        switch (errno) {
        case EEXIST:
            printf("mkdir: directory %s already exists.\n", argv[1]);
            break;
        case ENOENT:
            printf("mkdir: parent directory of %s does not exist.\n", argv[1]);
            break;
        case EACCES:
            printf("mkdir: permission denied to create directory %s.\n", argv[1]);
            break;
        default:
            printf("mkdir: failed to create directory %s (errno %d).\n", argv[1], errno);
            break;
        }
        return -1;
    }

    return 0;
}

static void _readfile_help() {
    printf("Usage: readfile [file]\n");

    // Options: -x for hex (4 hex digits at a time), -b for hex bytes (no prefix), -B for binary, -t for text (default)
    printf("Options:\n");
    printf("  -x : display file contents in hexadecimal format (no prefix; 4 hex digits at a time)\n");
    printf("  -b : display file contents in hexadecimal format (no prefix; 1 byte / 2 hex digits at a time)\n");
    printf("  -B : display file contents in binary format (8 bits at a time)\n");
    printf("  -t : display file contents as text (default)\n");
}

void print_byte_hex(uint8_t byte) {
    const char hex_chars[] = "0123456789abcdef";
    putchar(hex_chars[(byte >> 4) & 0x0F]);
    putchar(hex_chars[byte & 0x0F]);
}

void print_byte_binary(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        putchar((byte & (1 << i)) ? '1' : '0');
    }
}

typedef enum {
    READFILE_MODE_TEXT,
    READFILE_MODE_HEX,
    READFILE_MODE_HEX_BYTES,
    READFILE_MODE_BINARY
} readfile_mode_t;

int readfile_cmd(int argc, char argv[][CMD_BUFFER_MAX]) {
    if (argc < 2) {
        _readfile_help();

        errno = EINVAL;
        return -1;
    }

    readfile_mode_t mode = READFILE_MODE_TEXT;
    bool fname_is_arg2 = false;

    if (argv[1][0] == '-') {
        fname_is_arg2 = true;
        switch (argv[1][1]) {
        case 'x':
            mode = READFILE_MODE_HEX;
            break;
        case 'b':
            mode = READFILE_MODE_HEX_BYTES;
            break;
        case 'B':
            mode = READFILE_MODE_BINARY;
            break;
        case 't':
            mode = READFILE_MODE_TEXT;
            break;
        default:
            _readfile_help();
            errno = EINVAL;
            return -1;
        }
    }

    const char *fname = fname_is_arg2 ? argv[2] : argv[1];

    FILE *file = fopen(fname, "r");
    if (!file) {
        switch (errno) {
        case ENOENT:
            printf("readfile: file %s does not exist.\n", fname);
            break;
        case EACCES:
            printf("readfile: permission denied to read file %s.\n", fname);
            break;
        default:
            printf("readfile: failed to open file %s (errno %d).\n", fname, errno);
            break;
        }
        return -1;
    }

    switch (mode) {
    case READFILE_MODE_HEX: {
        // Do we want to flip the endianness here?
        bool space = false;
        int ch;
        while ((ch = fgetc(file)) != EOF) {
            print_byte_hex((uint8_t)ch);
            space && putchar(' ');
            space = !space;
        }
        break;
    }
    case READFILE_MODE_HEX_BYTES: {
        int ch;
        while ((ch = fgetc(file)) != EOF) {
            print_byte_hex((uint8_t)ch);
            putchar(' ');
        }
        break;
    }
    case READFILE_MODE_BINARY: {
        int ch;
        while ((ch = fgetc(file)) != EOF) {
            print_byte_binary((uint8_t)ch);
            putchar(' ');
        }
        break;
    }
    case READFILE_MODE_TEXT: {
        int ch;
        while ((ch = fgetc(file)) != EOF) {
            putchar(ch);
        }
        break;
    }
    }

    putchar('\n');

    fclose(file);
    return 0;
}