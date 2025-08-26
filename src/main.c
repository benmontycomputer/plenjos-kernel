#define LIMINE_API_REVISION 3

#include <limine.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lib/stdio.h"

#include "memory/kmalloc.h"
#include "memory/mm.h"

#include "arch/platform.h"

#if ARCH(X86_64)
#include "arch/x86_64/common.h"
#endif

#include "cpu/cpu.h"

#include "devices/input/keyboard/keyboard.h"

#include "shell/shell.h"

#include "proc/proc.h"
#include "proc/scheduler.h"
#include "proc/thread.h"

#include "syscall/syscall.h"

#include "exec/elf.h"

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
static volatile struct limine_module_request module_request
    = { .id = LIMINE_MODULE_REQUEST, .revision = 3 };

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_executable_address_request executable_addr_request
    = { .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST, .revision = 3 };

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
    uint8_t *pdest = (uint8_t *)dest;
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
    uint8_t *pdest = (uint8_t *)dest;
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
        if (p1[i] != p2[i]) { return p1[i] < p2[i] ? -1 : 1; }
    }

    return 0;
}

// Halt and catch fire function.
__attribute__((noreturn)) //
void
hcf(void) {
    for (;;) {
        asm("hlt");
    }
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) { hcf(); }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) { hcf(); }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    fb = framebuffer->address;
    fb_scanline = (int)framebuffer->pitch;
    fb_width = (int)framebuffer->width;
    fb_height = (int)framebuffer->height;
    fb_bytes_per_pixel = (int)((framebuffer->bpp + 7) / 8);
    printf("Framebuffer: %p\n", framebuffer->address);

    init_memory_manager();
    init_kernel_heap();

    if (!executable_addr_request.response) {
        printf("Couldn't get kernel load addresses. Halt!\n");
        hcf();
    }

    kernel_load_phys = executable_addr_request.response->physical_base;
    kernel_load_virt = executable_addr_request.response->virtual_base;

// Setup the GDT
#if ARCH(X86_64)
    init_x86_64();
#endif

    load_smp();

    init_keyboard();

#define helloworld "Hello World!\n"

    char *hwld = (char *)kmalloc_heap(14);

    memcpy(hwld, helloworld, 14);

    printf(hwld);

    // This tests console autoscroll
    /* for (uint64_t n = 0; n <= 0x3F; n++) {
        printf("%p\n", (void *)n);
    } */

    // TODO: free hwld

    char *str;

    for (int i = 0; i < 32; i++) {
        str = (char *)kmalloc_heap(PAGE_LEN);

#define test_str "[] Testing heap #%d.    "

        memcpy(str, test_str, 25);

        // printf(str, i);
    }

    for (int i = 32; i < 64; i++) {
        str = (char *)kmalloc_heap(PAGE_LEN);

#define test_str "[] Testing heap #%d at %p.\n"

        memcpy(str, test_str, 28);

        // printf(str, i, str);

        kfree_heap(str);
    }

    syscalls_init();

    // start_shell();
    /* proc_t *shell_proc = create_proc("kshell");
    thread_t *shell_thread = create_thread(shell_proc, "kshell_t0", start_shell, NULL);
    thread_ready(shell_thread);

    assign_thread_to_cpu(shell_thread); */

    if (!module_request.response) {
        printf("No modules detected.\n");
    } else {
        void *elf_addr = module_request.response->modules[0]->address;

        proc_t *shell_proc = create_proc("kshell");

        uint64_t entry, stack;

        loadelf(elf_addr, shell_proc->pml4, &entry, &stack);

        // map_virtual_memory_using_alloc(get_physaddr(stack, shell_proc->pml4), stack, 0x4000, PAGE_FLAG_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITE, alloc_paging_node, kernel_pml4);

        thread_t *shell_thread = create_thread(shell_proc, "kshell_t0", (void *)entry, NULL);

        // kfree_heap(shell_thread->stack);

        // shell_thread->stack = (void *)stack;
        // shell_thread->regs.iret_rsp = stack + 0x4000;

        thread_ready(shell_thread);

        // shell_thread->regs.cr3 = virt_to_phys((uint64_t)kernel_pml4);

        // uint64_t d;
        // loadelf(elf_addr, kernel_pml4, &d, &d);

        printf("Assigning thread...\n\n");
        assign_thread_to_cpu(shell_thread);
    }

    printf("No more code. Halt!\n");

    // We're done, just hang...
    hcf();
}