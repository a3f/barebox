// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 Masahiro Yamada <yamada.masahiro@socionext.com>
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>
#include <driver.h>
#include <asm/psci.h>
#include <asm/secure.h>
#include <linux/arm-smccc.h>

static u32 (*psci_invoke_fn)(ulong, ulong, ulong, ulong);

static int psci_xlate_error(s32 errnum)
{
       switch (errnum) {
       case ARM_PSCI_RET_NOT_SUPPORTED:
               return -ENOTSUPP; // Operation not supported
       case ARM_PSCI_RET_INVAL:
               return -EINVAL; // Invalid argument
       case ARM_PSCI_RET_DENIED:
               return -EPERM; // Operation not permitted
       case ARM_PSCI_RET_ALREADY_ON:
               return -EBUSY; // CPU already on
       case ARM_PSCI_RET_ON_PENDING:
               return -EALREADY; // CPU_ON in progress
       case ARM_PSCI_RET_INTERNAL_FAILURE:
               return -EIO; // Internal failure
       case ARM_PSCI_RET_NOT_PRESENT:
	       return -ESRCH; // Trusted OS not present on core
       case ARM_PSCI_RET_DISABLED:
               return -ENODEV; // CPU is disabled
       case ARM_PSCI_RET_INVALID_ADDRESS:
               return -EACCES; // Bad address
       default:
	       return errnum;
       };
}

int psci_invoke(ulong function, ulong arg0, ulong arg1, ulong arg2,
		ulong *result)
{
	ulong ret;
	if (!psci_invoke_fn)
		return -EPROBE_DEFER;

	ret = psci_invoke_fn(function, arg0, arg1, arg2);
	if (result)
		*result = ret;

	switch (function) {
	case ARM_PSCI_0_2_FN_PSCI_VERSION:
	case ARM_PSCI_1_0_FN64_STAT_RESIDENCY:
	case ARM_PSCI_1_0_FN64_STAT_COUNT:
		/* These don't return an error code */
		return 0;
	}

	return psci_xlate_error(ret);
}

static u32 invoke_psci_fn_hvc(ulong function, ulong arg0, ulong arg1, ulong arg2)
{
	struct arm_smccc_res res;
	arm_smccc_hvc(function, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static u32 invoke_psci_fn_smc(ulong function, ulong arg0, ulong arg1, ulong arg2)
{
	struct arm_smccc_res res;
	arm_smccc_smc(function, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static int __init psci_probe(struct device_d *dev)
{
	const char *method;
	const char *version;
	const void *param;
	int ret;

	ret = dev_get_drvdata(dev, (const void **)&version);
	if (ret)
		return -ENODEV;

	ret = of_property_read_string(dev->device_node, "method", &method);
	if (ret) {
		dev_warn(dev, "missing \"method\" property\n");
		return -ENXIO;
	}

	if (!strcmp(method, "hvc")) {
		psci_invoke_fn = invoke_psci_fn_hvc;
	} else if (!strcmp(method, "smc")) {
		psci_invoke_fn = invoke_psci_fn_smc;
	} else {
		pr_warn("invalid \"method\" property: %s\n", method);
		return -EINVAL;
	}

	param = dev_add_param_string_fixed(dev, "version", version);
	return PTR_ERR_OR_ZERO(param);
}

static __maybe_unused struct of_device_id psci_dt_ids[] = {
	{ .compatible = "arm,psci", .data = "0.1" },
	{ .compatible = "arm,psci-0.2", .data = "0.2" },
	{ .compatible = "arm,psci-1.0", .data = "1.0" },
	{ /* sentinel */ },
};

static struct driver_d psci_driver = {
	.name = "psci",
	.probe = psci_probe,
	.of_compatible = DRV_OF_COMPAT(psci_dt_ids),
};
coredevice_platform_driver(psci_driver);
