MODULE_DIR = usr/shell

# Nuke built-in rules and variables.
MAKEFLAGS += -rR
.SUFFIXES:

# This is the name that our final executable will have.
# Change as needed.
override OUTPUT := myos

# User controllable toolchain and toolchain prefix.
TOOLCHAIN :=
TOOLCHAIN_PREFIX :=
ifneq ($(TOOLCHAIN),)
    ifeq ($(TOOLCHAIN_PREFIX),)
        TOOLCHAIN_PREFIX := $(TOOLCHAIN)-
    endif
endif

# User controllable C compiler command.
ifneq ($(TOOLCHAIN_PREFIX),)
    CC := $(TOOLCHAIN_PREFIX)gcc
else
    CC := cc
endif

# User controllable linker command.
LD := $(TOOLCHAIN_PREFIX)ld

# User controllable objcopy command.
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy

# Defaults overrides for variables if using "llvm" as toolchain.
ifeq ($(TOOLCHAIN),llvm)
    CC := clang
    LD := ld.lld
endif

# User controllable C flags.
CFLAGS := -g -O2 -pipe

# User controllable C preprocessor flags. We set none by default.
CPPFLAGS :=

# User controllable nasm flags.
NASMFLAGS := -F dwarf -g

# User controllable linker flags. We set none by default.
LDFLAGS :=

# Check if CC is Clang.
override CC_IS_CLANG := $(shell ! $(CC) --version 2>/dev/null | grep -q '^Target: '; echo $$?)

# If the C compiler is Clang, set the target as needed.
ifeq ($(CC_IS_CLANG),1)
    override CC += \
        -target x86_64-unknown-none-elf
endif

# Internal C flags that should not be changed by the user.
# VLAs are banned because they can cause stack overflows in kernel code.
override CFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
	-Wvla \
	-Werror=vla \
	-pedantic \
    -ffreestanding \
    -fno-stack-protector \
	-Wframe-larger-than=512 \
	-Werror=frame-larger-than=512 \
    -fno-stack-check \
	-fno-omit-frame-pointer \
    -fno-lto \
    -fno-PIC \
    -m64 \
    -march=x86-64 \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel

# Internal C preprocessor flags that should not be changed by the user.
override CPPFLAGS := \
    -I src \
	-I include \
    $(CPPFLAGS) \
    -DLIMINE_API_REVISION=3 \
    -MMD \
    -MP

# Internal nasm flags that should not be changed by the user.
override NASMFLAGS := \
    -f elf64 \
    $(NASMFLAGS) \
    -Wall

# Internal linker flags that should not be changed by the user.
override LDFLAGS += \
    -m elf_x86_64 \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    -T linker.lds

# Use "find" to glob all *.c, *.S, and *.asm files in the tree and obtain the
# object and header dependency file names.
override SRCFILES := $(shell find -L src -type f 2>/dev/null | LC_ALL=C sort)
override CFILES := $(filter %.c,$(SRCFILES))
override ASFILES := $(filter %.S,$(SRCFILES))
override NASMFILES := $(filter %.asm,$(SRCFILES))
override PSFFILES := $(filter %.psf,$(SRCFILES))
override OBJ := $(addprefix obj/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o) $(NASMFILES:.asm=.asm.o) $(PSFFILES:.psf=.psf.o))
override HEADER_DEPS := $(addprefix obj/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d))

# Default target. This must come first, before header dependencies.
.PHONY: all
.PHONY: kernel_module

all: bin/$(OUTPUT) kernel_module

kernel_module:
	$(MAKE) -C $(MODULE_DIR)

# Include header dependencies.
-include $(HEADER_DEPS)

# Link rules for the final executable.
bin/$(OUTPUT): GNUmakefile linker.lds $(OBJ)
	mkdir -p "$$(dirname $@)"
	$(LD) $(OBJ) $(LDFLAGS) -o $@

# Compilation rules for *.c files.
obj/%.c.o: %.c GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Compilation rules for *.S files.
obj/%.S.o: %.S GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Compilation rules for *.asm (nasm) files.
obj/%.asm.o: %.asm GNUmakefile
	mkdir -p "$$(dirname $@)"
	nasm $(NASMFLAGS) $< -o $@

# Compilation rules for *.psf files.
obj/%.psf.o: %.psf GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(OBJCOPY) -O elf64-x86-64 -B i386 -I binary $< $@

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -rf bin obj usr/shell/bin usr/shell/obj

uefi_run:
	qemu-system-x86_64 \
		-m 4096 \
		-smp cores=4,threads=1,sockets=1,maxcpus=4 \
		-boot d \
		-drive file=../image.iso,if=ide,media=cdrom \
		-d guest_errors,cpu_reset,invalid_mem,unimp \
		-vga std \
		-serial stdio \
		-smbios type=0,uefi=on \
		-rtc base=utc,clock=host \
        -no-reboot

uefi_run_gdb:
	qemu-system-x86_64 \
		-m 4096 \
		-smp cores=4,threads=1,sockets=1,maxcpus=4 \
		-boot d \
		-drive file=../image.iso,if=ide,media=cdrom \
		-d guest_errors,cpu_reset,invalid_mem,unimp \
		-vga std \
		-serial stdio \
		-smbios type=0,uefi=on \
		-rtc base=utc,clock=host \
        -no-reboot \
        -s -S

#-device i8042

#qemu-system-x86_64 \
		-machine q35 \
		-m 4096 \
		-smp cores=2,threads=2,sockets=1,maxcpus=4 \
		-boot d \
		-hda $(DISK_DIR)/disk.img \
		-cdrom $(BUILD_DIR)/$(OS_NAME)-$(OS_VERSION)-image.iso \
		-serial stdio \
		-d guest_errors,int,cpu_reset \
		-D $(DEBUG_DIR)/qemu.log \
		-vga std \
		-bios /usr/share/OVMF/OVMF_CODE.fd  \
		-rtc base=utc,clock=host