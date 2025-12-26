#include "syscall/syscall_helpers.h"

#include "kernel.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "memory/mm_common.h"
#include "plenjos/errno.h"
#include "plenjos/limits.h"
#include "proc/proc.h"

#include <stdbool.h>

// TODO: any attempts to read/write from an out-of-bounds buffer should kill the process (segfault?)

int copy_to_user_buf(void *dest, void *src, size_t count, pml4_t *current_pml4) {
    size_t first_page_len = PAGE_LEN - ((uint64_t)dest % PAGE_LEN);
    size_t last_page_len  = ((uint64_t)dest + count) % PAGE_LEN;

    uint64_t offs = 0;

    while (offs < count) {
        uint64_t addr = (uint64_t)dest + offs;

        page_t *page = find_page(addr, false, current_pml4);
        if (!page || !(page->present)) {
            printf("check_buf: page not mapped at addr %p\n", addr);
            return -EFAULT;
        }
        if (!page->user) {
            printf("check_buf: permission denied (not user accessible) at addr %p\n", addr);
            return -EFAULT;
        }
        if (!page->rw) {
            printf("check_buf: permission denied (not writeable) at addr %p\n", addr);
            return -EFAULT;
        }

        if (offs == 0) {
            // First page (or we only need 1 page); might not have to write a full page
            size_t to_copy = (count < first_page_len) ? count : first_page_len;
            memcpy((void *)phys_to_virt((page->frame << 12) + (addr % PAGE_LEN)), src + offs, to_copy);
            offs += to_copy;
        } else if (offs + PAGE_LEN > count) {
            // Last page; might not have to write a full page
            memcpy((void *)phys_to_virt((page->frame << 12) + (addr % PAGE_LEN)), src + offs, last_page_len);
            offs += last_page_len;
        } else {
            // Full page
            memcpy((void *)phys_to_virt(page->frame << 12), src + offs, PAGE_LEN);
            offs += PAGE_LEN;
        }
    }

    return 0;
}

int copy_to_kernel_buf(void *dest, void *src, size_t count, pml4_t *current_pml4) {
    size_t first_page_len = PAGE_LEN - ((uint64_t)src % PAGE_LEN);
    size_t last_page_len  = ((uint64_t)src + count) % PAGE_LEN;

    uint64_t offs = 0;

    while (offs < count) {
        uint64_t addr = (uint64_t)src + offs;

        page_t *page = find_page(addr, false, current_pml4);
        if (!page || !(page->present)) {
            printf("check_buf: page not mapped at addr %p\n", addr);
            return -EFAULT;
        }
        if (!page->user) {
            printf("check_buf: permission denied (not user accessible) at addr %p\n", addr);
            return -EFAULT;
        }

        if (offs == 0) {
            // First page (or we only need 1 page); might not have to read a full page
            size_t to_copy = (count < first_page_len) ? count : first_page_len;
            memcpy(dest + offs, (void *)phys_to_virt((page->frame << 12) + (addr % PAGE_LEN)), to_copy);
            offs += to_copy;
        } else if (offs + PAGE_LEN > count) {
            // Last page; might not have to read a full page
            memcpy(dest + offs, (void *)phys_to_virt((page->frame << 12) + (addr % PAGE_LEN)), last_page_len);
            offs += last_page_len;
        } else {
            // Full page
            memcpy(dest + offs, (void *)phys_to_virt(page->frame << 12), PAGE_LEN);
            offs += PAGE_LEN;
        }
    }

    return 0;
}

int handle_string_arg(pml4_t *current_pml4, uint64_t user_ptr, const char **out) {
    uint64_t str_ptr = phys_to_virt(get_physaddr(user_ptr, current_pml4));
    uint64_t len     = strlen((char *)str_ptr) + 1;

    char *kstr = kmalloc_heap(len);
    if (!kstr) {
        return -ENOMEM;
    }

    int check = copy_to_kernel_buf(kstr, (void *)user_ptr, len, current_pml4);
    if (check < 0) {
        kfree_heap(kstr);
        return check;
    }

    *out = kstr;
    return 0;
}

int handle_relative_path(const char *rel, proc_t *proc, char out[PATH_MAX]) {
    if (!rel || !proc || !out) {
        return -EINVAL;
    }

    if (rel[0] == '/') {
        // Absolute path
        strncpy(out, rel, PATH_MAX);
        out[PATH_MAX - 1] = '\0';
        return 0;
    } else {
        // Relative path
        if (proc->cwd[0] == '\0') {
            // No cwd set; treat as root
            if (strlen(rel) + 2 > PATH_MAX) {
                return -ENAMETOOLONG;
            }
            // snprintf(out, PATH_MAX, "/%s", rel);
            *out = '/';
            strncpy(out + 1, rel, PATH_MAX - 1);
            out[PATH_MAX - 1] = '\0';
        } else {
            // snprintf(out, PATH_MAX, "%s/%s", proc->cwd, rel);
            size_t cwd_len = strlen(proc->cwd);
            size_t rel_len = strlen(rel);
            if (cwd_len + rel_len + 2 > PATH_MAX) {
                return -ENAMETOOLONG;
            }

            strncpy(out, proc->cwd, cwd_len + 1);
            out[cwd_len] = '/';
            strncpy(out + cwd_len + 1, rel, rel_len + 1);
            out[PATH_MAX - 1] = '\0';
        }
        return 0;
    }
}

int collapse_path(const char *path_in, char path_out[PATH_MAX]) {
    if (!path_in || !path_out) {
        return -EINVAL;
    }

    char *temp = kmalloc_heap(PATH_MAX);
    if (!temp) {
        return -ENOMEM;
    }
    size_t out_idx = 0;

    size_t len = strlen(path_in);
    for (size_t i = 0; i < len; ) {
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
            } else if ((i + 1 < len) && (path_in[i + 1] == '.') &&
                       ((i + 2 == len) || (path_in[i + 2] == '/'))) {
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