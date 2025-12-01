
FILE_NAME = user_shell

override OUTPUT = user_shell.elf


# ====== Compiler & Linker ======
# GCC = /usr/local/x86_64-elf/bin/x86_64-elf-gcc
CC = $(TOOLCHAIN_PREFIX)gcc
CFLAGS := -g -Wall -Wextra -std=gnu11 -m64 -march=x86-64 \
           -fno-pie -no-pie -nostdlib -nostartfiles \
		   -mno-mmx -mno-sse -mno-sse2 \
           -ffreestanding -O2

CPPFLAGS := -I ../include -I src -I src/libc

# LD = /usr/local/x86_64-elf/bin/x86_64-elf-ld
LD = $(TOOLCHAIN_PREFIX)ld
LINKER_FILE = linker.lds
LDFLAGS := -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 -T $(LINKER_FILE)

OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy

# ====== Directories ======
BUILD_DIR = build

# ====== Source and Object Files ======
SRC_FILES := $(shell find . -name '*.c')
PSF_FILES := $(shell find . -name '*.psf')
# OBJ_FILES := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRC_FILES))
OBJ_FILES := $(addprefix obj/,$(SRC_FILES:.c=.c.o) $(PSF_FILES:.psf=.psf.o))

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
all: bin/$(OUTPUT)

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

obj/%.S.o: %.S GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Compilation rules for *.psf files.
obj/%.psf.o: %.psf GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(OBJCOPY) -O elf64-x86-64 -B i386 -I binary $< $@

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -rf bin obj

# ====== Rules ======

# Compile .c to build/%.o
#$(BUILD_DIR)/%.o: %.c
#	@mkdir -p $(dir $@)
#	$(CC) $(CFLAGS) -c $< -o $@

# Linking
#linking: $(BUILD_DIR)/$(FILE_NAME).elf

#$(BUILD_DIR)/$(FILE_NAME).elf: $(OBJ_FILES)
#	@mkdir -p $(BUILD_DIR)
#	$(LD) $(LDFLAGS) -T $(LINKER_FILE) -o $@ $^
#	@echo "Linked object files into $@"
#
#	make clean

# Clean
#clean:
#	# Delete all .o and .d files recursively inside build directory
#	find $(BUILD_DIR) -type f \( -name '*.o' -o -name '*.d' \) -delete
#
#	# Remove empty directories (which had only .o, .d files) inside build directory
#	find $(BUILD_DIR) -type d -empty -delete
#
#	# Remove all files from build directory
#	# rm -rf $(BUILD_DIR)/*
#
#	@echo "Cleaned object files and empty directories."