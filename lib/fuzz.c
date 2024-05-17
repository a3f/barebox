// SPDX-License-Identifier: GPL-2.0-only

#include <fuzz.h>
#include <string.h>
#include <common.h>
#include <malloc.h>

int call_for_each_fuzz_test(int (*fn)(const struct fuzz_test *test))
{
	const struct fuzz_test *test;
	int ret;

	for_each_fuzz_test(test) {
		ret = fn(test);
		if (ret)
			return ret;
	}

	return 0;
}

#ifdef CONFIG_FUZZ_EXTERNAL
const u8 *fuzzer_get_data(size_t *len);
#else
static inline const u8 *fuzzer_get_data(size_t *len)
{
	return NULL;
}
#endif

static const struct fuzz_test *fuzz;

static int fuzz_main(void)
{
	const void *buf;
	size_t len;

	if ((buf = fuzzer_get_data(&len)))
		fuzz_test_once(fuzz, buf, len);

	while ((buf = fuzzer_get_data(&len))) {
		fuzz_test_once(fuzz, buf, len);
	}

	pr_emerg("fuzzing unexpectedly ended");
	return -1;
}

int setup_external_fuzz(const char *fuzz_name)
{
	const struct fuzz_test *test;

	for_each_fuzz_test(test) {
		if (!strcmp(test->name, fuzz_name)) {
			fuzz = test;
			barebox_main = fuzz_main;
			barebox_loglevel = MSG_CRIT;
			return 0;
		}
	}

	return -1;
}

bool fuzz_external_active(void)
{
	return fuzz != NULL;
}
