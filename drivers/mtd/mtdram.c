// SPDX-License-Identifier: GPL-2.0-only
/*
 * Lightweight in-memory MTD device, used to feed fuzz test input to
 * code that expects to operate on an MTD device.
 */

#include <mtdram.h>
#include <linux/mtd/mtd.h>
#include <driver.h>
#include <malloc.h>
#include <string.h>
#include <xfuncs.h>

struct mtdram {
	struct mtd_info mtd;
	struct device dev;
	void *buf;
	size_t buf_size;
};

/* Bounds check that cannot overflow for attacker-controlled offsets */
static bool mtdram_range_ok(struct mtdram *mtdram, loff_t off, size_t len)
{
	if (off < 0 || off > mtdram->buf_size)
		return false;

	return len <= mtdram->buf_size - off;
}

static int mtdram_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct mtdram *mtdram = container_of(mtd, struct mtdram, mtd);

	if (!mtdram_range_ok(mtdram, instr->addr, instr->len))
		return -EINVAL;

	memset(mtdram->buf + instr->addr, 0xff, instr->len);

	return 0;
}

static int mtdram_read(struct mtd_info *mtd, loff_t from, size_t len,
		       size_t *retlen, u_char *buf)
{
	struct mtdram *mtdram = container_of(mtd, struct mtdram, mtd);

	if (!mtdram_range_ok(mtdram, from, len))
		return -EINVAL;

	memcpy(buf, mtdram->buf + from, len);
	*retlen = len;

	return 0;
}

static int mtdram_write(struct mtd_info *mtd, loff_t to, size_t len,
			size_t *retlen, const u_char *buf)
{
	struct mtdram *mtdram = container_of(mtd, struct mtdram, mtd);

	if (!mtdram_range_ok(mtdram, to, len))
		return -EINVAL;

	memcpy(mtdram->buf + to, buf, len);
	*retlen = len;

	return 0;
}

struct mtdram *mtdram_init(size_t size, size_t erasesize, size_t writesize)
{
	struct mtdram *mtdram;
	struct mtd_info *mtd;
	int ret;

	if (!size || !erasesize || !writesize)
		return NULL;
	if (size % erasesize || erasesize % writesize)
		return NULL;

	mtdram = xzalloc(sizeof(*mtdram));
	mtdram->buf_size = size;
	mtdram->buf = xmalloc(size);
	memset(mtdram->buf, 0xff, size);

	dev_set_name(&mtdram->dev, "mtdram");
	mtdram->dev.id = DEVICE_ID_DYNAMIC;

	ret = register_device(&mtdram->dev);
	if (ret)
		goto err;

	mtd = &mtdram->mtd;
	mtd->type = MTD_RAM;
	mtd->erasesize = erasesize;
	mtd->writesize = writesize;
	mtd->writebufsize = writesize;
	mtd->flags = MTD_WRITEABLE | MTD_BIT_WRITEABLE;
	mtd->size = size;
	mtd->_erase = mtdram_erase;
	mtd->_read = mtdram_read;
	mtd->_write = mtdram_write;
	mtd->dev.parent = &mtdram->dev;

	ret = add_mtd_device(mtd, "mtdram", DEVICE_ID_DYNAMIC);
	if (ret)
		goto err_unreg;

	return mtdram;

err_unreg:
	unregister_device(&mtdram->dev);
err:
	free(mtdram->buf);
	free(mtdram);
	return NULL;
}

void mtdram_setup(struct mtdram *mtdram, const void *data, size_t size)
{
	memset(mtdram->buf, 0xff, mtdram->buf_size);
	if (data && size)
		memcpy(mtdram->buf, data, min(size, mtdram->buf_size));
}

struct mtd_info *mtdram_get_mtd(struct mtdram *mtdram)
{
	return &mtdram->mtd;
}

void mtdram_free(struct mtdram *mtdram)
{
	del_mtd_device(&mtdram->mtd);
	unregister_device(&mtdram->dev);
	free(mtdram->buf);
	free(mtdram);
}
