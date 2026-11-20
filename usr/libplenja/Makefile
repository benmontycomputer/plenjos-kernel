# libplenja: string + file helpers for user programs. Built against the in-tree libc and installed into root/lib by
# the top-level GNUmakefile. Consumers use -isystem usr/libplenja/include and -L usr/libplenja/lib -lplenja.

TOOLCHAIN_PREFIX ?= x86_64-elf-

CC = $(TOOLCHAIN_PREFIX)gcc
LD = $(TOOLCHAIN_PREFIX)ld

CFLAGS   = -m64 -ffreestanding -fno-builtin -fno-stack-protector -g -O2 -Wall -Wextra \
           -mno-mmx -mno-sse -mno-sse2 -fPIC -MMD -MP
CPPFLAGS = -I fs -I str -isystem ../libc/include -I ../../include

SRCS    := fs/libplenja-fs.c str/libplenja-str.c
OBJS    := $(patsubst %.c,obj/%.c.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)
HEADERS := include/libplenja.h include/libplenja-fs.h include/libplenja-str.h

.PHONY: all clean
all: lib/libplenja.so $(HEADERS)

obj/%.c.o: %.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

lib/libplenja.so: $(OBJS)
	mkdir -p $(dir $@)
	$(LD) -shared -o $@ $^

include/libplenja.h: libplenja.h
	mkdir -p $(dir $@)
	cp $< $@

include/libplenja-fs.h: fs/libplenja-fs.h
	mkdir -p $(dir $@)
	cp $< $@

include/libplenja-str.h: str/libplenja-str.h
	mkdir -p $(dir $@)
	cp $< $@

-include $(DEPS)

clean:
	rm -rf obj lib include
