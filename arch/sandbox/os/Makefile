# SPDX-License-Identifier: GPL-2.0-only

machine-y := sandbox

machdirs := $(patsubst %,arch/sandbox/mach-%/,$(machine-y))

KBUILD_CPPFLAGS = $(patsubst %,-I$(srctree)/%include,$(machdirs)) \
		  -I$(srctree)/arch/sandbox/include

KBUILD_CPPFLAGS += -D_FILE_OFFSET_BITS=64 -include $(objtree)/include/generated/autoconf.h

KBUILD_CFLAGS := -Wall

NOSTDINC_FLAGS :=

ifeq ($(CONFIG_SANDBOX_LINUX_I386),y)
KBUILD_CFLAGS += -m32
endif

pbl-y = common.o tap.o setjmp.o
pbl-$(CONFIG_MALLOC_LIBC) += libc_malloc.o
pbl-$(CONFIG_FUZZ_EXTERNAL) += fuzz.o
pbl-$(CONFIG_ARCH_HAS_STACK_DUMP) += unwind.o

CFLAGS_sdl.pbl.o = $(shell $(PKG_CONFIG) sdl2 --cflags)
pbl-$(CONFIG_SDL) += sdl.o

CFLAGS_ftdi.pbl.o = $(shell $(PKG_CONFIG) libftdi1 --cflags)
pbl-$(CONFIG_GPIO_LIBFTDI1) += ftdi.o
