#define LIMINE_API_REVISION 3

#include "arch/platform.h"
#include "cpu/cpu.h"
#include "devices/input/keyboard/drivers/ps2kbd.h"
#include "devices/input/keyboard/keyboard.h"
#include "devices/pci/pci.h"
#include "devices/storage/ide.h"
#include "exec/elf.h"
#include "lib/serial.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "proc/proc.h"
#include "proc/scheduler.h"
#include "proc/thread.h"
#include "syscall/syscall.h"
#include "timer/pit.h"
#include "vfs/fscache.h"
#include "vfs/iso9660/iso9660.h"
#include "vfs/vfs.h"

#include <limine.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if ARCH(X86_64)
#include "arch/x86_64/common.h"
#include "arch/x86_64/gdt/gdt.h"
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

void test_func() {
    for (size_t i = 0; i < 10; i++) {
        printf("Test %d on core %d\n", i, get_curr_core());
        pit_sleep(1000);
    }
}

void stack_push_qword(uint64_t qword, uint64_t **rsp) {
    *rsp  = (uint64_t *)((uint8_t *)(*rsp) - sizeof(uint64_t));
    **rsp = qword;
}

// Halt and catch fire function.
__attribute__((noreturn)) //
void hcf(void) {
    for (;;) {
        asm volatile("hlt");
    }
}

#define LINKER_OFFS 0x7fff80000000ULL

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
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
    printf("Framebuffer: %p\n", framebuffer->address);

    // map_virtual_memory(virt_to_phys((uint64_t)fb), fb_height * fb_scanline, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE,
    // kernel_pml4);

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

    debug_serial = true;

    init_serial();

    printf("Debugging to serial output now...\n");

    load_smp();

    vfs_init();

    init_keyboard();

    // TODO: make this part of a device manager
    init_ps2_keyboard();
    pci_scan();
    ide_init();

#define helloworld "Hello World!\n"

    char *hwld = (char *)kmalloc_heap(14);

    memcpy(hwld, helloworld, 14);

    printf(hwld);

    kfree_heap(hwld);

    // This tests console autoscroll
    /* for (uint64_t n = 0; n <= 0x3F; n++) {
        printf("%p\n", (void *)n);
    } */

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

    clear();

    /* vfs_handle_t *iso_root_handle;
    int res = vfs_open("/iso9660/bin/init", SYSCALL_OPEN_FLAG_READ, 0, 0, &iso_root_handle);
    if (res == 0) {
        printf("Loading init from iso9660...\n\n");
        uint64_t file_size = ((struct vfs_iso9660_cache_node_data
    *)iso_root_handle->backing_node->internal_data)->dir_record->data_length_le;

        void *elf_buf = kmalloc_heap(file_size);
        if (!elf_buf) {
            printf("OOM error allocating memory for elf buffer\n");
            hcf();
        }

        ssize_t read_bytes = vfs_read(iso_root_handle, elf_buf, file_size);
        vfs_close(iso_root_handle);
        if (read_bytes < 0) {
            printf("Error reading iso9660/bin/init: %d\n", (int)read_bytes);
            hcf();
        } else if ((uint64_t)read_bytes != file_size) {
            printf("Short read reading iso9660/bin/init: expected %p, got %p\n", (void *)file_size,
                   (void *)read_bytes);
            hcf();
        }

        proc_t *shell_proc = create_proc("init", NULL);

        uint64_t entry, stack;

        loadelf(elf_buf, shell_proc->pml4, &entry, &stack);
        kfree_heap(elf_buf);

        thread_t *shell_thread = create_thread(shell_proc, "init_t0", (void *)entry, NULL);

        thread_ready(shell_thread);

        kfree_heap(elf_buf);
    } */
    vfs_handle_t *ld_handle = NULL;
    int res                 = vfs_open("/iso9660/lib/ld.so", SYSCALL_OPEN_FLAG_READ, 0, 0, &ld_handle);
    void *linker_elf_buf    = NULL;
    ssize_t linker_read_bytes;
    if (res == 0) {
        printf("Loading ld.so from iso9660...\n\n");
        uint64_t file_size = ((struct vfs_iso9660_cache_node_data *)ld_handle->backing_node->internal_data)
                                 ->dir_record->data_length_le;

        linker_elf_buf = kmalloc_heap(file_size);
        if (!linker_elf_buf) {
            printf("OOM error allocating memory for elf buffer\n");
            hcf();
        }

        linker_read_bytes = vfs_read(ld_handle, linker_elf_buf, file_size);
        vfs_close(ld_handle);
        if (linker_read_bytes < 0) {
            printf("Error reading iso9660/lib/ld.so: %d\n", (int)linker_read_bytes);
            hcf();
        } else if ((uint64_t)linker_read_bytes != file_size) {
            printf("Short read reading iso9660/lib/ld.so: expected %p, got %p\n", (void *)file_size,
                   (void *)linker_read_bytes);
            hcf();
        }
    } else {
        printf("Error opening iso9660/lib/ld.so: %d\n", res);
        hcf();
    }

    // ld_handle has already been closed

    res = vfs_open("/iso9660/bin/init", SYSCALL_OPEN_FLAG_READ, 0, 0, &ld_handle);
    if (res == 0) {
        printf("Loading init from iso9660...\n\n");
        uint64_t file_size = ((struct vfs_iso9660_cache_node_data *)ld_handle->backing_node->internal_data)
                                 ->dir_record->data_length_le;
        proc_t *shell_proc = create_proc("init", NULL);

        uint64_t entry;
        void *elf_buf = kmalloc_heap(file_size);
        if (!elf_buf) {
            printf("OOM error allocating memory for elf buffer\n");
            hcf();
        }

        ssize_t read_bytes = vfs_read(ld_handle, elf_buf, file_size);
        vfs_close(ld_handle);
        if (read_bytes < 0) {
            printf("Error reading iso9660/bin/init: %d\n", (int)read_bytes);
            hcf();
        } else if ((uint64_t)read_bytes != file_size) {
            printf("Short read reading iso9660/bin/init: expected %p, got %p\n", (void *)file_size, (void *)read_bytes);
            hcf();
        }

        printf("Read %p bytes for init and %p bytes for ld.so\n", (void *)read_bytes, (void *)linker_read_bytes);

        // Load the linker first
        uint64_t linker_entry;
        loadelf(linker_elf_buf, shell_proc->pml4, &linker_entry, LINKER_OFFS);
        // kfree_heap(linker_elf_buf);

        // 0x7fff80000000ULL
        loadelf(elf_buf, shell_proc->pml4, &entry, 0x400000ULL);
        kfree_heap(elf_buf);

        thread_t *shell_thread = create_thread(shell_proc, "init_t0", (void *)linker_entry, NULL);

        // Everything we are about to do fits in the top page of the stack.
        uint64_t *rsp_og
            = (uint64_t *)(phys_to_virt(get_physaddr((uint64_t)shell_thread->stack_top - PAGE_LEN, shell_proc->pml4))
                           + 0x1000);
        uint64_t *rsp = rsp_og;

        // First, setup the stack
        const char *argv0 = "/iso9660/bin/init";
        const char *argv1 = NULL;

        const char *envp0 = NULL;

        const uint64_t argc = 1;

        size_t argv0_len = strlen(argv0) + 1;

        // Push argv0 onto the stack (getting address aligned to 8 bytes)
        uint64_t argv0_addr = ((uint64_t)(rsp)-argv0_len) & ~0x7ULL;
        rsp                 = (uint64_t *)argv0_addr;
        memcpy((void *)argv0_addr, argv0, argv0_len);

        uint64_t str_usr_addr = (uint64_t)shell_thread->stack_top - ((uint64_t)rsp_og - argv0_addr);

        // Align the rsp to 16-bytes (we have even number of values left to push after this)
        rsp = (uint64_t *)((uint64_t)(rsp) & ~0xF);

        // Push envp0, argv1, argv0, argc onto the stack
        stack_push_qword(0, &rsp);
        stack_push_qword(0, &rsp);
        stack_push_qword((uint64_t)str_usr_addr, &rsp);
        stack_push_qword((uint64_t)argc, &rsp);

        shell_thread->regs.iret_rsp -= (uint64_t)rsp_og - (uint64_t)rsp;

        printf("Final stack pointer for init: %p from %p; entry: %p\n", (void *)shell_thread->regs.iret_rsp,
               (void *)rsp, (void *)entry);

        shell_thread->regs.rdi = shell_thread->regs.iret_rsp;
        shell_thread->regs.rsi = LINKER_OFFS; // Linker base addr
        shell_thread->regs.rdx = 0x400000ULL; // Shell binary base addr

        thread_ready(shell_thread);
    } else {
        printf("\nWARN: Falling back to module...\n\n");

        if (!module_request.response) {
            printf("No modules detected.\n");
        } else {
            // for (size_t i = 0; i < 12; i++) {
            void *elf_addr = module_request.response->modules[0]->address;

            proc_t *shell_proc = create_proc("kshell", NULL);

            uint64_t entry;

            loadelf(elf_addr, shell_proc->pml4, &entry, 0);

            thread_t *shell_thread = create_thread(shell_proc, "kshell_t0", (void *)entry, NULL);

            thread_ready(shell_thread);
            // }
        }
    }

    /* for (size_t i = 0; i < 4; i++) {
        proc_t *test_proc = create_proc("testproc");
        thread_t *test_thread = create_thread(test_proc, "testproc_t0", test_func, NULL);

        test_thread->regs.iret_cs = KERNEL_CS; // Kernel code
        test_thread->regs.iret_ss = KERNEL_DS; // Kernel stack/data

        test_thread->regs.ds = KERNEL_DS;
        test_thread->regs.es = KERNEL_DS;
        test_thread->regs.fs = KERNEL_DS;
        test_thread->regs.gs = KERNEL_DS;

        thread_ready(test_thread);
    } */

    start_scheduler();

    printf("No more code. Halt!\n");

    // We're done, just hang...
    hcf();
}