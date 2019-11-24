// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Vaisala Oyj. All rights reserved.
 */

#include <common.h>
#include <init.h>
#include <of.h>
#include <linux/nvmem-consumer.h>
#include <linux/reboot-mode.h>

struct nvmem_reboot_mode {
	struct reboot_mode_driver reboot;
	struct nvmem_cell *cell;
};

static int nvmem_reboot_mode_write(struct reboot_mode_driver *reboot,
				   u32 magic)
{
	struct nvmem_reboot_mode *nvmem_rbm;
	int ret;

	nvmem_rbm = container_of(reboot, struct nvmem_reboot_mode, reboot);

	ret = nvmem_cell_write(nvmem_rbm->cell, &magic, sizeof(magic));
	if (ret < 0)
		dev_err(reboot->dev, "update reboot mode bits failed\n");

	return ret;
}

static int nvmem_reboot_mode_probe(struct device_d *dev)
{
	int ret;
	size_t len;
	struct nvmem_reboot_mode *nvmem_rbm;
	void *magicbuf;

	nvmem_rbm = xzalloc(sizeof(*nvmem_rbm));

	nvmem_rbm->reboot.dev = dev;
	nvmem_rbm->reboot.write = nvmem_reboot_mode_write;
	nvmem_rbm->reboot.priority = 200;

	nvmem_rbm->cell = nvmem_cell_get(dev, "reboot-mode");
	if (IS_ERR(nvmem_rbm->cell)) {
		dev_err(dev, "failed to get the nvmem cell reboot-mode\n");
		return PTR_ERR(nvmem_rbm->cell);
	}

	magicbuf = nvmem_cell_read(nvmem_rbm->cell, &len);
	if (IS_ERR(magicbuf) || len != 4) {
		dev_err(dev, "error reading reboot mode: %s\n",
			strerrorp(magicbuf));
		return PTR_ERR(magicbuf);
	}

	ret = reboot_mode_register(&nvmem_rbm->reboot, *(u32 *)magicbuf);
	if (ret)
		dev_err(dev, "can't register reboot mode\n");

	return ret;
}

static const struct of_device_id nvmem_reboot_mode_of_match[] = {
	{ .compatible = "nvmem-reboot-mode" },
	{ /* sentinel */ }
};

static struct driver_d nvmem_reboot_mode_driver = {
	.probe = nvmem_reboot_mode_probe,
	.name = "nvmem-reboot-mode",
	.of_compatible = nvmem_reboot_mode_of_match,
};
postcore_platform_driver(nvmem_reboot_mode_driver);
