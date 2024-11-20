// SPDX-License-Identifier: GPL-2.0-or-later

#include <malloc.h>
#include <errno.h>
#include <linux/align.h>
#include <linux/build_bug.h>
#include <linux/printk.h>
#include <linux/log2.h>
#include <linux/sizes.h>
#include <string.h>
#include <fuzz.h>
#include <dlmalloc.h>
#include <xfuncs.h>

void free_sensitive(void *mem)
{
	size_t size = malloc_usable_size(mem);

	if (size)
		memzero_explicit(mem, size);

	free(mem);
}

enum {
	CMD_MALLOC,
	CMD_FREE,
	CMD_REALLOC,
	CMD_CALLOC,
	CMD_MEMALIGN,
	CMD_MALLOC_USABLE_SIZE,
};

struct malloc_cmd {
	ulong op:3;
	ulong ptrslot:8;
	ulong extra_size : (BITS_PER_LONG - 11);
	ulong size;
};
static_assert(sizeof(struct malloc_cmd) == 2 * sizeof(ulong));

struct malloc_arena {
	void *ptrslots[0x100];
	char area[SZ_128M];
};

static unsigned long malloc_brk, malloc_start, malloc_end;

static void *fuzz_sbrk_no_zero(ptrdiff_t increment)
{
	return malloc(increment);
}

static void *fuzz_sbrk(ptrdiff_t increment)
{
	return malloc(increment);
}

static int __maybe_unused fuzz_malloc(const uint8_t *data, size_t size)
{
	static struct malloc_arena *arena;

	if (!PTR_IS_ALIGNED(data, sizeof(struct malloc_cmd)))
		return -EINVAL;
	if (!IS_ALIGNED(size, sizeof(struct malloc_cmd)))
		return 0;

	if (!arena) {
		arena = xmalloc(sizeof(*arena));

		malloc_start = (ulong)&arena->area;
		malloc_end = malloc_start + SZ_128M;

		dlsbrk = fuzz_sbrk;
	}

	memset(arena, 0x00, offsetof(struct malloc_arena, area));
	malloc_brk = malloc_start;

	for (size_t i = 0; i < size / sizeof(struct malloc_cmd);
	     i += sizeof(struct malloc_cmd)) {
		struct malloc_cmd *cmd = (void *)&data[i];
		void *tmp, **ptrslot;

		ptrslot = &arena->ptrslots[cmd->ptrslot];

		switch (cmd->op) {
		case CMD_MALLOC:
			dlfree(*ptrslot);
			*ptrslot = dlmalloc(cmd->size);
			break;
		case CMD_FREE:
			dlfree(*ptrslot);
			*ptrslot = NULL;
			break;
		case CMD_REALLOC:
			tmp = dlrealloc(*ptrslot, cmd->size);
			if (tmp)
				*ptrslot = tmp;
			break;
		case CMD_CALLOC:
			dlfree(*ptrslot);
			*ptrslot = dlcalloc(cmd->extra_size, cmd->size);
			break;

		case CMD_MEMALIGN:
			if (!is_power_of_2(cmd->extra_size))
				continue;
			dlfree(*ptrslot);
			*ptrslot = dlmemalign(cmd->extra_size, cmd->size);
			break;
		case CMD_MALLOC_USABLE_SIZE:
			dlmalloc_usable_size(*ptrslot);
			break;
		}
	}

	return 0;
}
fuzz_test("malloc", fuzz_malloc);
