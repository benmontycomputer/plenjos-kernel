#include "elf.h"

#include "plenjos/errno.h"
#include "plenjos/stat.h"
#include "syscall.h"

int loadelf(const char *path, struct elf_object *out_obj) {
    struct elf_object obj = { 0 };

    // First, load the ELF file from the given path.
    int fd = syscall_open(path, SYSCALL_OPEN_FLAG_READ, 0);

    struct kstat stat;
    syscall_fstat(fd, &stat);

    if (stat.size <= 0) {
        // Handle error: file size is invalid
        // For simplicity, we return an empty elf_object here.
        syscall_print("loadelf: invalid file size\n");
        return -EIO;
    }

    size_t file_size = (size_t)stat.size;

    // TODO: use ulimit to determine this
    // TODO: use mmap or something when this is to large for the stack
    if (file_size >= 0x80000 /* 512 KiB */) {
        syscall_print("loadelf: ELF file too large\n");
        return -EIO;
    }

    uint8_t file_buf[file_size]; // 512 KiB buffer on stack

    ssize_t res = syscall_read(fd, file_buf, file_size);
    if (res < 0 || (size_t)res != file_size) {
        syscall_print("loadelf: failed to read entire ELF file\n");
        return -EIO;
    } else {
        syscall_print("loadelf: read ");
        syscall_print_ptr((uint64_t)file_size);
        syscall_print(" bytes from ELF file\n");
    }

    return 0;
}