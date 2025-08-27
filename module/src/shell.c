#include <stdint.h>

#include "../../src/syscall/syscall.h"

#include "uconsole.h"
#include "lib/stdio.h"

// Uses PSF1 format
extern void kputchar(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32_t fg, uint32_t bg);

extern void kputs(const char *str, int cx, int cy);
extern void kputhex(uint64_t hex, int cx, int cy);

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi) {
    uint64_t out;

    asm volatile("mov %[_rax], %%rax\n"
                 "mov %[_rbx], %%rbx\n"
                 "mov %[_rcx], %%rcx\n"
                 "mov %[_rdx], %%rdx\n"
                 "mov %[_rsi], %%rsi\n"
                 "mov %[_rdi], %%rdi\n"
                 "int $0x80\n"
                 "mov %%rax, %[_out]\n"
                 : [_out] "=r"(out)
                 : [_rax] "r"(rax), [_rbx] "r"(rbx), [_rcx] "r"(rcx), [_rdx] "r"(rdx), [_rsi] "r"(rsi), [_rdi] "r"(rdi)
                 : "rax", "rbx", "rcx", "rdx", "rsi", "rdi" // Clobbered registers
    );

    return out;
}

// __attribute__((section(".data")))
const char teststr_shell[] = "\ntest!!!\n\n\0";
// char buffer[3];

volatile char *buf = (char *)0x300000;
fb_info_t *fb_info = (fb_info_t *)0x301000;

/* uint64_t fb;
int fb_scanline,fb_width,fb_height,fb_bytes_per_pixel; */

// char str_shell[] = " is the char\n";
// char buffer[3];

// __attribute__((section(".text")))
void _start() {
    // *((char *)0x800) = 0;
    // SYSCALL_PRINT

    // str_shell[1] = 0;
    // buffer[1] = 'd';
    // buffer[2] = '\0';

    // TODO: handle errors in these syscalls

    syscall(SYSCALL_PRINT, (uint64_t)teststr_shell, 0, 0, 0, 0);

    uint64_t buf_len = 0x1000;

    syscall(SYSCALL_MEMMAP, (uint64_t)buf, buf_len, 0, 0, 0);

    *(buf) = '\0';
    *(buf + 1) = '\0';

    syscall(SYSCALL_MEMMAP, (uint64_t)fb_info, sizeof(fb_info_t), 0, 0, 0);

    syscall(SYSCALL_GET_FB, (uint64_t)fb_info, 0, 0, 0, 0);

    syscall(SYSCALL_PRINT_PTR, (uint64_t)fb_info, 0, 0, 0, 0);
    syscall(SYSCALL_PRINT_PTR, (uint64_t)fb_info->fb_ptr, 0, 0, 0, 0);

    /* fb = (uint64_t)fb_info->fb_ptr;

    // for(;;) {}

    fb_scanline = fb_info->fb_scanline;
    fb_width = fb_info->fb_width;
    fb_height = fb_info->fb_height;
    fb_bytes_per_pixel = fb_info->fb_bytes_per_pixel; */

    // kputs("Test string!", 100, 10);

    // for(;;) {}

    for (;;) {
        uint64_t ch_uint64 = syscall(SYSCALL_READ, 0, 0, 0, 0, 0);;
        buf[0] = (char)(ch_uint64);
        // syscall(4, ch_uint64, 0, 0, 0, 0);
        // syscall(SYSCALL_PRINT, (uint64_t)buf, 0, 0, 0, 0);
        // kputs((const char *)&teststr_shell, 100, 10);
        ((uint32_t *)fb_info->fb_ptr)[0] = 0xFFFF0000;

        printf("%c", buf[0]);
    }
}