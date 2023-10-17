/* SPDX-License-Identifier: GPL-2.0-only */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>

#define setjmp barebox_setjmp
#define longjmp barebox_longjmp

#include <asm/setjmp.h>
#include <mach/linux.h>

static jmp_buf fuzzer_jmpbuf, barebox_jmpbuf;
static const uint8_t *fuzz_data;
size_t fuzz_size;

static void *main_stack_bottom;
static unsigned main_stack_size;

static void *barebox_stack;

static __always_inline size_t str_has_prefix(const char *str, const char *prefix)
{
	size_t len = strlen(prefix);
	return strncmp(str, prefix, len) == 0 ? len : 0;
}

static __attribute__((noreturn)) void start_barebox_coop(void)
{
	char argv0[4097];
	ssize_t ret;
	char *argv[6] = {
		argv0, (char []) { "--fuzz" }, NULL, NULL
	};

	finish_switch_fiber(NULL, &main_stack_bottom, &main_stack_size);

	ret = selfpath(argv0, sizeof(argv0));
	if (ret > 0) {
		argv[2] = getenv("BAREBOX_FUZZ_TARGET") ?: basename(argv0);
		if (argv[2])
			argv[2] += str_has_prefix(argv[2], "fuzz-");
	}

	if (!argv[2]) {
		fputs("can't determine fuzz target\n", stderr);
		exit(1);
	}

	sandbox_main(3, argv);

	exit(1);
}

const uint8_t *fuzzer_get_data(size_t *size);
const uint8_t *fuzzer_get_data(size_t *size)
{
	void *fake_stack_save = NULL;

	if (!barebox_setjmp(barebox_jmpbuf)) {
		start_switch_fiber(&fake_stack_save, main_stack_bottom, main_stack_size);
		barebox_longjmp(fuzzer_jmpbuf, 1);
	}

	finish_switch_fiber(fake_stack_save, &main_stack_bottom, &main_stack_size);

	*size = fuzz_size;
	return fuzz_data;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	void *fake_stack_save = NULL;

	fuzz_data = data;
	fuzz_size = size;

	if (!barebox_stack) {
		barebox_stack = aligned_alloc(16, CONFIG_STACK_SIZE);
		if (!barebox_stack)
			return -ENOMEM;

		initjmp(barebox_jmpbuf, start_barebox_coop,
			barebox_stack + CONFIG_STACK_SIZE);

		if (!barebox_setjmp(fuzzer_jmpbuf)) {
			start_switch_fiber(&fake_stack_save, barebox_stack, CONFIG_STACK_SIZE);
			barebox_longjmp(barebox_jmpbuf, 1);
		}

		finish_switch_fiber(fake_stack_save, &main_stack_bottom, &main_stack_size);
	}

	if (!barebox_setjmp(fuzzer_jmpbuf)) {
		start_switch_fiber(&fake_stack_save, barebox_stack, CONFIG_STACK_SIZE);
		barebox_longjmp(barebox_jmpbuf, 1);
	}

	finish_switch_fiber(fake_stack_save, &main_stack_bottom, &main_stack_size);

	return 0;
}
