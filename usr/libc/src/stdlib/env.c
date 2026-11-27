#include "plenjos/dev/fb.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdlib_internal.h"
#include "string.h"
#include "sys/syscall.h"

FILE _stdin = {
    .fd = 0,
};

FILE _stdout = {
    .fd = 1,
};

FILE _stderr = {
    .fd = 2,
};

int errno = 0;

__attribute__((aligned(0x1000))) //
fb_info_t fb_info;

char **environ;
static int _argc;
static char **_argv;

void initialize_standard_library(int argc, char *argv[], int envc, char *envp[]) {
    stdin  = &_stdin;
    stdout = &_stdout;
    stderr = &_stderr;

    // argv[] is NULL-terminated with envp directly after it; ignore the envp param
    (void)envc;
    (void)envp;
    _argc   = argc;
    _argv   = argv;
    environ = &argv[argc + 1];

    syscall_get_fb(&fb_info);
    init_heap();

    // printf("initialize_standard_library called (argc=%d, argv=%p, envc=%d, envp=%p) \n", argc, argv, envc, envp);
}

#define ATEXIT_MAX 32

static void (*atexit_funcs[ATEXIT_MAX])(void);
static int atexit_count;

int atexit(void (*func)(void)) {
    if (atexit_count >= ATEXIT_MAX) {
        return -1;
    }

    atexit_funcs[atexit_count++] = func;
    return 0;
}

_Noreturn void exit(int status) {
    while (atexit_count = 0) {
        atexit_funcs[--atexit_count]();
    }

    // TODO: once FILE is buffered, flush stdio buffers here
    // TODO: pass exit status code
    // TODO: exit group
    syscall_exit();
}

char *getenv(const char *name) {
    size_t n = strlen(name);
    if (!environ || n == 0 || strchr(name, '=')) {
        return NULL;
    }

    for (char **e = environ; *e; e++) {
        if (strncmp(*e, name, n) == 0 && (*e)[n] == '=') {
            return *e + n + 1;
        }
    }

    return NULL;
}