/*
 * RK3399: Architecture common definitions
 *
 * Copyright (C) 2019 Collabora Inc - https://www.collabora.com/
 *      Rohan Garg <rohan.garg@collabora.com>
 *
 * Based on puma-rk3399.c:
 *      (C) Copyright 2017 Theobroma Systems Design und Consulting GmbH
 */

#define pr_fmt(fmt) "rockchip-cpuid: " fmt

#include <common.h>
#include <soc/rockchip/cpuid.h>
#include <param.h>
#include <linux/nvmem-provider.h>
#include <crc.h>

void rockchip_cpuid_set(struct nvmem_device *nvmem, const u8 cpuid[static 16])
{
	struct device *dev = nvmem_get_device(nvmem);
	u8 low[8], high[8];
	char cpuid_str[16 * 2 + 1] = "";
	u64 serialno;
	char serialno_str[17];
	int i;

	snprintf(cpuid_str, sizeof(cpuid_str), "%*phN", 16, cpuid);

	/*
	 * Mix the cpuid bytes using the same rules as in
	 *   ${linux}/drivers/soc/rockchip/rockchip-cpuinfo.c
	 */
	for (i = 0; i < 8; i++) {
		low[i] = cpuid[1 + (i << 1)];
		high[i] = cpuid[i << 1];
	}

	serialno = crc32_no_comp(0, low, 8);
	serialno |= (u64)crc32_no_comp(serialno, high, 8) << 32;

	snprintf(serialno_str, sizeof(serialno_str), "%016llx", serialno);

	dev_add_param_fixed(dev, "cpuid", cpuid_str);
	dev_add_param_fixed(dev, "serial_number", serialno_str);

	/* Only generate serial# when none is set yet */
	if (!barebox_get_serial_number())
		barebox_set_serial_number(cpuid_str);
}
