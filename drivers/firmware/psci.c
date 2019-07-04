// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 Masahiro Yamada <yamada.masahiro@socionext.com>
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>
#include <driver.h>
#include <asm/psci.h>
#include <linux/arm-smccc.h>

static long (*psci_invoke_fn)(ulong, ulong, ulong, ulong);

#if 0 /* FIXME look ino TF-A */
-static int psci_xlate_ret(int ret)
-{
-       switch (ret) {
-       case ARM_PSCI_RET_SUCCESS:
-               return 0;
-       case ARM_PSCI_RET_NOT_SUPPORTED:
-               return -EOPNOTSUPP;
-       case ARM_PSCI_RET_INVALID_ADDRESS:
-               return -EINVAL;
-       case ARM_PSCI_RET_DENIED:
-               return -EPERM;
-       };
-
-       return ret;
-}
#endif

long psci_invoke(ulong function, ulong arg0, ulong arg1, ulong arg2)
{
#if 0 /* FIXME */
	if (!psci_invoke_fn)
		return -EPROBE_DEFER;
#endif

	return psci_invoke_fn(function, arg0, arg1, arg2);
}

static long invoke_psci_fn_hvc(ulong function,
			       ulong arg0, ulong arg1, ulong arg2)
{
	struct arm_smccc_res res;
	arm_smccc_hvc(function, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return (long)res.a0;
}

static long invoke_psci_fn_smc(ulong function,
			       ulong arg0, ulong arg1, ulong arg2)
{
	struct arm_smccc_res res;
	arm_smccc_smc(function, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return (long)res.a0;
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
