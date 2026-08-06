// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <driver.h>
#include <fuzz.h>
#include <malloc.h>
#include <linux/sizes.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/mtd-abi.h>

#include "state.h"

/*
 * The device and the format outlive a single fuzzing iteration: creating
 * them per input would dominate the runtime and registering the same
 * device name twice fails anyway. Setup is attempted exactly once.
 */
static int fuzz_state_setup(const char *name,
			    struct state_backend_format **format,
			    struct device **devp)
{
	struct device *dev;
	int ret;

	dev = xzalloc(sizeof(*dev));
	dev_set_name(dev, "%s", name);
	dev->id = DEVICE_ID_SINGLE;

	ret = register_device(dev);
	if (ret) {
		free(dev);
		return ret;
	}

	ret = backend_format_raw_create(format, NULL, "", dev);
	if (ret) {
		unregister_device(dev);
		free(dev);
		return ret;
	}

	*devp = dev;

	return 0;
}

static int fuzz_state_direct(const u8 *data, size_t size)
{
	static struct state_backend_format *format;
	static struct device *dev;
	static bool setup_done;
	void *buf;
	ssize_t len;

	if (!size)
		return 0;

	if (!setup_done) {
		setup_done = true;
		fuzz_state_setup("fuzz-state-direct", &format, &dev);
	}

	if (!format)
		return 0;

	buf = xmemdup(data, size);
	len = size;

	format->verify(format, 0, buf, &len, STATE_FLAG_NO_AUTHENTICATION);

	free(buf);

	return 0;
}
fuzz_test("state-direct", fuzz_state_direct);

#if IS_ENABLED(CONFIG_MTD)
static int fuzz_state_circular(struct mtd_info *mtd)
{
	static struct state_backend_format *format;
	static struct device *dev;
	static bool setup_done;
	struct state_backend_storage_bucket *bucket;
	struct mtd_info_user mtd_uinfo = {};
	void *buf = NULL;
	ssize_t len;
	int ret;

	if (!setup_done) {
		setup_done = true;
		fuzz_state_setup("fuzz-state-circular", &format, &dev);
	}

	if (!format)
		return 0;

	mtd_uinfo.erasesize = mtd->erasesize;
	mtd_uinfo.writesize = mtd->writesize;
	mtd_uinfo.size = mtd->size;
	mtd_uinfo.type = mtd->type;
	mtd_uinfo.mtd = mtd;

	ret = state_backend_bucket_circular_create(dev, NULL, &bucket,
						   0, mtd->writesize,
						   &mtd_uinfo);
	if (ret)
		return 0;

	ret = bucket->read(bucket, &buf, &len);
	if (ret)
		goto out;

	format->verify(format, 0, buf, &len, STATE_FLAG_NO_AUTHENTICATION);

out:
	free(buf);
	bucket->free(bucket);

	return 0;
}
fuzz_test_mtdram("state-circular", SZ_16K, SZ_4K, 64, fuzz_state_circular);
#endif
