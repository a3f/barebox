/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _SOC_ROCKCHIP_CPUID_H__
#define _SOC_ROCKCHIP_CPUID_H__

#include <linux/types.h>

struct nvmem_device;

void rockchip_cpuid_set(struct nvmem_device *, const u8 cpuid[static 16]);

#endif
