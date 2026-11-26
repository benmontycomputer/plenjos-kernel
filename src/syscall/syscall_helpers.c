#include "syscall/syscall_helpers.h"

#include "kernel.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "memory/mm_common.h"
#include "plenjos/errno.h"
#include "plenjos/limits.h"
#include "proc/task.h"

#include <stdbool.h>

// TODO: any attempts to read/write from an out-of-bounds buffer should kill the process (segfault?)

int copy_to_user_buf(void *dest, void *src, size_t count, bool override_write_check, mm_t *mm) {
    size_t first_page_len = PAGE_LEN - ((uint64_t)dest % PAGE_LEN);
    size_t last_page_len  = ((uint64_t)dest + count) % PAGE_LEN;

    uint64_t offs = 0;

    while (offs < count) {
        uint64_t addr = (uint64_t)dest + offs;

        vm_area_t *vma = vmlookup((void *)addr, mm);
        if (!vma) {
            printf("copy_to_user_buf(): page not mapped at addr %p\n", addr);
            return -EFAULT;
        }
        if (!(vma->flags & VMA_USER)) {
            printf("copy_to_user_buf(): permission denied (not user accessible) at addr %p\n", addr);
            return -EFAULT;
        }
        if (!(vma->flags & VMA_RW) && !override_write_check) {
            printf("copy_to_user_buf(): permission denied (not writeable) at addr %p\n", addr);
            return -EFAULT;
        }

        physptr_t phys = get_physaddr(addr, mm);
        if (!phys) {
            printf("copy_to_user_buf(): page not mapped at addr %p\n", addr);
            return -EFAULT;
        }
        void *dest = (void *)phys_to_virt((uint64_t)phys);

        if (offs == 0) {
            // First page (or we only need 1 page); might not have to write a full page
            size_t to_copy = (count < first_page_len) ? count : first_page_len;
            memcpy(dest, src + offs, to_copy);
            offs += to_copy;
        } else if (offs + PAGE_LEN > count) {
            // Last page; might not have to write a full page
            memcpy(dest, src + offs, last_page_len);
            offs += last_page_len;
        } else {
            // Full page
            memcpy(dest, src + offs, PAGE_LEN);
            offs += PAGE_LEN;
        }
    }

    return 0;
}

int copy_to_kernel_buf(void *dest, void *src, size_t count, mm_t *mm) {
    size_t first_page_len = PAGE_LEN - ((uint64_t)src % PAGE_LEN);
    size_t last_page_len  = ((uint64_t)src + count) % PAGE_LEN;

    uint64_t offs = 0;

    while (offs < count) {
        uint64_t addr = (uint64_t)src + offs;

        vm_area_t *vma = vmlookup((void *)addr, mm);
        if (!vma) {
            printf("copy_to_kernel_buf(): page not mapped at addr %p\n", addr);
            return -EFAULT;
        }
        if (!(vma->flags & VMA_USER)) {
            printf("copy_to_kernel_buf(): permission denied (not user accessible) at addr %p\n", addr);
            return -EFAULT;
        }

        physptr_t phys = get_physaddr(addr, mm);
        if (!phys) {
            printf("copy_to_kernel_buf(): page not mapped at addr %p\n", addr);
            return -EFAULT;
        }
        void *src = (void *)phys_to_virt((uint64_t)phys);

        if (offs == 0) {
            // First page (or we only need 1 page); might not have to read a full page
            size_t to_copy = (count < first_page_len) ? count : first_page_len;
            memcpy(dest + offs, src, to_copy);
            offs += to_copy;
        } else if (offs + PAGE_LEN > count) {
            // Last page; might not have to read a full page
            memcpy(dest + offs, src, last_page_len);
            offs += last_page_len;
        } else {
            // Full page
            memcpy(dest + offs, src, PAGE_LEN);
            offs += PAGE_LEN;
        }
    }

    return 0;
}

static ssize_t strnlen_user(mm_t *mm, uint64_t user_ptr, size_t max_len) {
    size_t len = 0;

    while (len < max_len) {
        uint64_t addr = user_ptr + len;

        vm_area_t *vma = vmlookup((void *)addr, mm);
        if (!vma || !(vma->flags & VMA_USER)) {
            return -EFAULT;
        }

        uint64_t page_off = addr % PAGE_LEN;
        size_t in_page    = PAGE_LEN - page_off;
        physptr_t phys    = get_physaddr(addr, mm);
        if (!phys) {
            return -EFAULT;
        }
        const char *chunk = (const char *)phys_to_virt(phys);

        for (size_t i = 0; i < in_page && len < max_len; i++, len++) {
            if (chunk[i] == '\0') {
                return (ssize_t)len;
            }
        }
    }

    return -ENAMETOOLONG;
}

int handle_string_arg(uint64_t user_ptr, const char **out, mm_t *mm) {
#ifdef _KERNEL_EXTRA_CHECKS_FOR_NULL_ARGS
    if (!out) {
        kout(KERNEL_PANIC, "handle_string_arg(): out must not be NULL!\n");
    }
#endif
    *out = NULL;

    if (user_ptr == 0) {
        printf("handle_string_arg: null pointer passed from userland\n");
        return -EFAULT;
    }

    ssize_t len = strnlen_user(mm, user_ptr, PATH_MAX);
    if (len < 0) {
        printf("handle_string_arg: bad pointer passed from userland (errno %d)\n", (int)len);
        return (int)len;
    }

    len += 1;

    char *kstr = kmalloc_heap(len);
    if (!kstr) {
        return -ENOMEM;
    }

    int check = copy_to_kernel_buf(kstr, (void *)user_ptr, len, mm);
    if (check < 0) {
        kfree_heap(kstr);
        return check;
    }

    kstr[len - 1] = '\0'; // Safeguard for modification by another task w/ same mm

    *out = kstr;
    return 0;
}

int handle_relative_path(const char *rel, task_t *task, char *out) {
    if (!rel || !task || !out) {
        return -EINVAL;
    }

    size_t rel_len = strlen(rel);
    if (rel_len == 0) {
        return -ENOENT; // POSIX required
    }

    if (rel[0] == '/') {
        // Absolute path
        if (rel_len + 1 > PATH_MAX) {
            return -ENAMETOOLONG;
        }
        memcpy(out, rel, rel_len + 1);
    } else {
        const char *cwd = task->cwd[0] ? task->cwd : "/";
        size_t cwd_len  = strlen(cwd);
        bool has_slash  = cwd[cwd_len - 1] == '/';

        if (cwd_len + (has_slash ? 0 : 1) + rel_len + 1 > PATH_MAX) {
            return -ENAMETOOLONG;
        }

        memcpy(out, cwd, cwd_len);
        if (!has_slash) {
            out[cwd_len++] = '/';
        }
        memcpy(out + cwd_len, rel, rel_len + 1);
    }

    return collapse_path(out, out);
}

int collapse_path(const char *path_in, char *path_out) {
    if (!path_in || !path_out) {
        return -EINVAL;
    }

    char *temp = kmalloc_heap(PATH_MAX);
    if (!temp) {
        return -ENOMEM;
    }
    size_t out_idx = 0;

    size_t len = strlen(path_in);
    for (size_t i = 0; i < len;) {
        // Skip multiple slashes
        if (path_in[i] == '/') {
            while (i < len && path_in[i] == '/') {
                i++;
            }
            if (out_idx == 0 || temp[out_idx - 1] != '/') {
                temp[out_idx++] = '/';
            }
            continue;
        }

        // Handle . and ..
        if (path_in[i] == '.') {
            if ((i + 1 == len) || (path_in[i + 1] == '/')) {
                // Single dot
                i++;
                continue;
            } else if ((i + 1 < len) && (path_in[i + 1] == '.') && ((i + 2 == len) || (path_in[i + 2] == '/'))) {
                // Double dot
                i += 2;
                if (out_idx > 1) {
                    // Remove last component
                    out_idx--;
                    while (out_idx > 0 && temp[out_idx - 1] != '/') {
                        out_idx--;
                    }
                }
                continue;
            }
        }

        // Copy normal character
        while (i < len && path_in[i] != '/') {
            temp[out_idx++] = path_in[i++];
        }
    }

    // Remove trailing slash if not root
    if (out_idx > 1 && temp[out_idx - 1] == '/') {
        out_idx--;
    }

    temp[out_idx] = '\0';
    strncpy(path_out, temp, PATH_MAX);
    path_out[PATH_MAX - 1] = '\0';
    kfree_heap(temp);
    return 0;
}