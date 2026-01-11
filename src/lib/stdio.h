#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Color codes (for serial output; not currently supported for framebuffer rendering) */
#define TEXT_BLACK   "\e[0;30m"
#define TEXT_RED     "\e[0;31m"
#define TEXT_GREEN   "\e[0;32m"
#define TEXT_YELLOW  "\e[0;33m"
#define TEXT_BLUE    "\e[0;34m"
#define TEXT_MAGENTA "\e[0;35m"
#define TEXT_CYAN    "\e[0;36m"
#define TEXT_RESET   "\e[0;0m"

// Use in IRQ handlers in case a process has STDIO locked when interrupt happens
int printf_nolock(const char *format, ...);

int printf(const char *format, ...);

typedef enum {
    KERNEL_PANIC = 0x0,
    KERNEL_SEVERE_FAULT,
    KERNEL_SEVERE_EXTERNAL_FAULT,
    KERNEL_EXTERNAL_FAULT,
    KERNEL_WARN,
    KERNEL_INFO,
    KERNEL_DEBUG_0,
    KERNEL_DEBUG_1,
    KERNEL_DEBUG_2,
    USER_FAULT = 0x9,
    USER_WARN
} kernel_msg_level_t;

/** There are levels to kprintf:
 *
 * Scemantics: if "error" is in the name, something must be unrecoverable. This could be:
 *  - The system as a whole (kernel panic; reboot required) -> KERNEL PANIC
 *  - The specific device whose driver threw the error -> DEVICE
 *  - The function (think syscall) which threw the error
 *  - etc.
 *
 * There is a KERNEL_SEVERE_FAULT but not a KERNEL_FAULT, because, unlike userspace errors, we can control and eliminate
 * kernel errors. If there are external factors involved, KERNEL_SEVERE_EXTERNAL_FAULT and KERNEL_EXTERNAL_FAULT can be
 * used, but any kernel error caused solely by buggy code is, by definition, severe.
 *
 * 0-8: Kernel errors/warnings: these errors are used to indicate faults from kernel-level code. Data and/or parameters
 * passed from user space MUST NOT be able to trigger ANY of these, UNLESS said data triggers a VALID BUT BUGGY piece of
 * kernel-space code. The only unrecoverable kernel error/warning is PANIC; by definition, any unrecoverable problem in
 * the kernel must lead to a panic.
 *
 * 0: Panic: fatal kernel-level errors; used to indicate faults in kernel-level code and/or potential memory corruption
 * that require the system to be halted. These should ONLY happen when the system is in a completely unrecoverable
 * state. Kernel-space drivers should avoid intentionally throwing these errors at all costs; killing the driver is
 * preferable to throwing a panic. Only the kernel itself should throw these errors, however the kernel can throw these
 * errors if it detects a fault in a kernel-level driver, as well as in its own code.
 *
 * Panics, as the name implies, automatically throw kernel panics when called.
 *
 * 1: Kernel-level code/corruption error: used to indicate that there is a bug in kernel-space code. System is
 * RECOVERABLE; function that threw the error is NOT RECOVERABLE. Both kernel-space drivers and the kernel itself may
 * throw this error. External issues like buggy devices SHOULD NOT trigger this error; they should instead trigger a
 * kernel-level I/O error.
 *
 * 2: Kernel-level unrecoverable external error: contained error; can be pinned on external factors like a buggy device.
 * The specific device whose driver threw the error is UNRECOVERABLE.
 *
 * 3: Kernel-level recoverable external error: contained error; can be pinned on external factors like a buggy device.
 * The device is in a RECOVERABLE state, but the function throwing the error is in an UNRECOVERABLE state.
 *
 * 4: Kernel-level warning: everything is SAFE and RECOVERABLE, but some kernel-level code noticed a potential minor
 * issue.
 *
 * 5-8: varying kernel-level debugging messages
 *
 * 9: User-level error: error in userspace; the triggering program is UNRECOVERABLE
 * 10: User-level warning: warning in userspace; the triggering program is RECOVERABLE
 */

int kout_nolock(kernel_msg_level_t level, const char *format, ...);
int kout(kernel_msg_level_t level, const char *format, ...);

_Noreturn void panic(const char *format, ...);

char *gets(char *str);

int backs();

int setcursor(bool curs);

int clear();