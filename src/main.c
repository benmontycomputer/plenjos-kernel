#include "arch/platform.h"
#include "cpu/cpu.h"
#include "devices/manager.h"
#include "exec/elf.h"
#include "kernel.h"
#include "lib/serial.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "memory/vmdump.h"
#include "proc/scheduler.h"
#include "proc/task.h"
#include "syscall/syscall.h"
#include "timer/pit.h"
#include "vfs/fscache.h"
#include "vfs/vfs.h"

#include <limine.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if ARCH(X86_64)
#include "arch/x86_64/common.h"
#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/gdt/tss.h"
#endif

bool debug_serial = false;

char *fb;
int fb_scanline, fb_width, fb_height, fb_bytes_per_pixel;

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests"))) //
static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_framebuffer_request framebuffer_request
    = { .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 3 };

// Get Module info by using limine bootloader
__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_module_request module_request = { .id = LIMINE_MODULE_REQUEST, .revision = 3 };

__attribute__((used, section(".limine_requests"))) //
volatile struct limine_executable_file_request executable_file_request
    = { .id = LIMINE_EXECUTABLE_FILE_REQUEST, .revision = 3 };

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_executable_address_request executable_addr_request
    = { .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST, .revision = 3 };

__attribute__((used, section(".limine_requests"))) //
volatile struct limine_executable_cmdline_request executable_cmdline_request
    = { .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST, .revision = 3 };

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_stack_size_request stack_size_request
    = { .id = LIMINE_STACK_SIZE_REQUEST, .revision = 3, .stack_size = KERNEL_STACK_SIZE };

uint64_t kernel_load_phys = 0;
uint64_t kernel_load_virt = 0;

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start"))) //
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) //
static volatile LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest      = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest      = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i - 1] = psrc[i - 1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Halt and catch fire function.
__attribute__((noreturn)) //
void hcf(void) {
    for (;;) {
        asm volatile("hlt");
    }
}

_Noreturn void panic_nomsg() {
    if (smp_loaded) stop_scheduler();
    hcf();
}

task_t *init_task;

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
// Also, this must NOT be static.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    fb                 = framebuffer->address;
    fb_scanline        = (int)framebuffer->pitch;
    fb_width           = (int)framebuffer->width;
    fb_height          = (int)framebuffer->height;
    fb_bytes_per_pixel = (int)((framebuffer->bpp + 7) / 8);
    kout(KERNEL_INFO, "Framebuffer: %p\n", framebuffer->address);

    // map_virtual_memory(virt_to_phys((uint64_t)fb), fb_height * fb_scanline, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE,
    // kernel_pml4);

    init_memory_manager();
    // Ensure lock functions see a (non-switchable) thread context
    populate_ktask(kernel_mm);
    // Setup the GDT
#if ARCH(X86_64)
    init_x86_64();
#endif

    init_kernel_heap();

    if (!executable_addr_request.response) {
        panic("Couldn't get kernel load addresses. Halt!\n");
    }

    kernel_load_phys = executable_addr_request.response->physical_base;
    kernel_load_virt = executable_addr_request.response->virtual_base;

    // TODO: integrate this into device manager
    init_serial();

    kout(KERNEL_INFO, "Debugging to serial output now...\n");
    kout(KERNEL_INFO, "KERNEL CMDLINE: %s\n", executable_cmdline_request.response->cmdline);
    kout(KERNEL_INFO, "Boot disk info: mbr %x, GPT not supported by kernel yet\n",
         executable_file_request.response->executable_file->mbr_disk_id);

    load_smp();

    start_scheduler();

    vfs_init();

    device_manager_init();

    kout(KERNEL_INFO, "\nDevice manager initalized.\n\n");

    // Boot with "vmdump" on the kernel command line (limine.conf) to print the live kernel page tables
    if (vm_dump_requested(executable_cmdline_request.response->cmdline)) {
        vm_dump("kernel pml4", kernel_pml4, VM_DUMP_ALL);
    }

    clear();

    init_task = create_task("init", NULL);

    int res = vfs_open("/dev/tty0", SYSCALL_OPEN_FLAG_READ, 0, 0, &init_task->fds[0]);
    if (res) {
        kout(KERNEL_PANIC, "Got res %d when opening /dev/tty0 for init's stdin\n", res);
    }

    res = vfs_open("/dev/tty0", SYSCALL_OPEN_FLAG_WRITE, 0, 0, &init_task->fds[1]);
    if (res) {
        kout(KERNEL_PANIC, "Got res %d when opening /dev/tty0 for init's stdout\n", res);
    }

    res = vfs_open("/dev/tty0", SYSCALL_OPEN_FLAG_WRITE, 0, 0, &init_task->fds[2]);
    if (res) {
        kout(KERNEL_PANIC, "Got res %d when opening /dev/tty0 for init's stderr\n", res);
    }

    debug_serial = true;

    execute("/bin/init", init_task);

    cpu_scheduler_task();
}

void populate_string_hexnum(char *buf, size_t len, const char *str, uint32_t hex) {
    size_t str_len = strlen(str);

    if (str_len + 9 > len) {
        kout(KERNEL_PANIC, "populate_string_hexnum(): len too short!\n");
        return;
    }

    strncpy(buf, str, str_len);

    char *buf_end = buf + str_len;

    int started = 0;

    for (int shift = 28; shift >= 0; shift -= 4) {
        uint32_t nibble = (hex >> shift) & 0xF;

        if (!started) {
            if (nibble == 0 && shift != 0) {
                continue;
            }

            started = 1;
        }

        *buf_end++ = "0123456789abcdef"[nibble];
    }

    *buf_end = '\0';
}