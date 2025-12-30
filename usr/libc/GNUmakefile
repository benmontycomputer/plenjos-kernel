# Toolchain
CC      = x86_64-elf-gcc
AR      = x86_64-elf-ar
LD      = x86_64-elf-ld
CFLAGS  = -m64 -ffreestanding -g -O2 -Wall -Wextra -fno-builtin -mno-mmx -mno-sse -mno-sse2 -fPIC -MMD -MP -I include -I ../../include
ASMFLAGS = -m64 -ffreestanding -g -O2 -Wall -fPIC
OBJCOPY = x86_64-elf-objcopy

# Directories
SRC_DIR = src
OBJ_DIR = obj
LIB_DIR = lib
CRT_OBJ_DIR = crt

# Find all C and assembly files recursively
ALL_CFILES := $(shell find $(SRC_DIR) -name '*.c')
ALL_SFILES := $(shell find $(SRC_DIR) -name '*.S')

# Filter out crt files from the source files
CFILES := $(filter-out $(SRC_DIR)/crt/%,$(ALL_CFILES))
SFILES := $(filter-out $(SRC_DIR)/crt/%,$(ALL_SFILES))
PSFFILES := $(shell find $(SRC_DIR) -name '*.psf')
COBJS  := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.c.o,$(CFILES))
SOBJS  := $(patsubst $(SRC_DIR)/%.S,$(OBJ_DIR)/%.S.o,$(SFILES))
PSFOBJS := $(patsubst $(SRC_DIR)/%.psf,$(OBJ_DIR)/%.psf.o,$(PSFFILES))
OBJS   = $(COBJS) $(SOBJS) $(PSFOBJS)

CRTCFILES := $(shell find $(SRC_DIR)/crt -name '*.c')
CRTSFILES := $(shell find $(SRC_DIR)/crt -name '*.S')
CRTCOBJS := $(patsubst $(SRC_DIR)/crt/%.c,$(CRT_OBJ_DIR)/%.o,$(filter %.c,$(CRTCFILES)))
CRTSOBJS := $(patsubst $(SRC_DIR)/crt/%.S,$(CRT_OBJ_DIR)/%.o,$(filter %.S,$(CRTSFILES)))
CRT_OBJS = $(CRTCOBJS) $(CRTSOBJS)

# Dependency files (.d) corresponding to .o files
DEPS := $(OBJS:.o=.d) $(CRT_OBJS:.o=.d)

# Output
STATIC_LIB = $(LIB_DIR)/libc.a
SHARED_LIB = $(LIB_DIR)/libc.so

# Default target
all: $(OBJ_DIR) $(CRT_OBJ_DIR) $(CRT_OBJS) $(LIB_DIR) $(SHARED_LIB)

# Build object files from C
$(OBJ_DIR)/%.c.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Build object files from assembly
$(OBJ_DIR)/%.S.o: $(SRC_DIR)/%.S
	mkdir -p $(dir $@)
	$(CC) $(ASMFLAGS) -c $< -o $@

# Build CRT object files from C
$(CRT_OBJ_DIR)/%.o: $(SRC_DIR)/crt/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Build CRT object files from assembly
$(CRT_OBJ_DIR)/%.o: $(SRC_DIR)/crt/%.S
	mkdir -p $(dir $@)
	$(CC) $(ASMFLAGS) -c $< -o $@

# Compilation rules for *.psf files.
$(OBJ_DIR)/%.psf.o: $(SRC_DIR)/%.psf
	mkdir -p $(dir $@)
	$(OBJCOPY) -O elf64-x86-64 -B i386 -I binary $< $@

# Static library
$(STATIC_LIB): $(OBJS)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $^

# Shared library (optional)
$(SHARED_LIB): $(OBJS)
	mkdir -p $(dir $@)
	$(LD) -shared -o $@ $^

# Include dependency files (ignore missing)
-include $(DEPS)

# Create directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CRT_OBJ_DIR):
	mkdir -p $(CRT_OBJ_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Clean
clean:
	rm -rf $(OBJ_DIR) $(CRT_OBJ_DIR) $(LIB_DIR)

.PHONY: all clean