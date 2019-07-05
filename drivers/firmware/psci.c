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
#include <command.h>
#include <getopt.h>

static u32 invoke_psci_too_early(ulong function,
				 ulong arg0, ulong arg1, ulong arg2)
{
	panic("fatal: invoke_psci() called before coredevice_init()");
}

static u32 (*psci_invoke_fn)(ulong, ulong, ulong, ulong) = invoke_psci_too_early;

u32 psci_invoke(ulong function, ulong arg0, ulong arg1, ulong arg2)
{
	return psci_invoke_fn(function, arg0, arg1, arg2);
}

static u32 invoke_psci_fn_hvc(ulong function,
			      ulong arg0, ulong arg1, ulong arg2)
{
	struct arm_smccc_res res;
	arm_smccc_hvc(function, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return (long)res.a0;
}

static u32 invoke_psci_fn_smc(ulong function,
			      ulong arg0, ulong arg1, ulong arg2)
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

static const char *psci_xlate_error(int errnum)
{
	static char error_string[10];

	switch (errnum) {
	case 0:
		return "no error";
	case ARM_PSCI_RET_NOT_SUPPORTED:
		return "Operation not supported";
	case ARM_PSCI_RET_INVAL:
		return "Invalid argument";
	case ARM_PSCI_RET_DENIED:
		return "Operation not permitted";
	case ARM_PSCI_RET_ALREADY_ON:
		return "CPU already on";
	case ARM_PSCI_RET_ON_PENDING:
		return "CPU_ON in progress";
	case ARM_PSCI_RET_INTERNAL_FAILURE:
		return "Internal failure";
	case ARM_PSCI_RET_NOT_PRESENT:
		return "CPU is disabled";
	case ARM_PSCI_RET_INVALID_ADDRESS:
		return "Bad address\n";
	};

	sprintf(error_string, "error %d", errnum);
	return error_string;
}

static void second_entry(void)
{
	// racy, garbled output
	printf("2nd CPU online, now turn off again\n");

	psci_invoke(ARM_PSCI_0_2_FN_CPU_OFF, 0, 0, 0);

	printf("2nd CPU still alive?\n");

	while (1)
		;
}

static int do_smc(int argc, char *argv[])
{
	int opt;
	u32 ret;

	while ((opt = getopt(argc, argv, "nicz")) > 0) {
		switch (opt) {
		case 'n':
			// Fixme it
			if (IS_ENABLED(CONFIG_ARM_SECURE_MONITOR))
				armv7_secure_monitor_install();
			else
				printf("barebox not configured as secure monitor\n");
			break;
		case 'i':
			ret = psci_invoke(ARM_PSCI_0_2_FN_PSCI_VERSION,
					  0, 0, 0);
			printf("found psci version %u.%u\n",
			       ret >> 16, ret & 0xffff);
			break;
		case 'c':
			// fixme use 64 bit define if appropriate
			ret = psci_invoke(ARM_PSCI_0_2_FN_CPU_ON,
				    1, (unsigned long)second_entry, 0);
			if ((s32)ret < 0) {
				printf("SMC failed: '%s'\n",
				       psci_xlate_error(ret));
				return COMMAND_ERROR;
			}
			break;
	}

	return 0;
}
BAREBOX_CMD_HELP_START(smc)
BAREBOX_CMD_HELP_TEXT("Secure monitor code test command")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-n",  "Install secure monitor and switch to nonsecure mode")
BAREBOX_CMD_HELP_OPT ("-i",  "Show information about installed PSCI version")
BAREBOX_CMD_HELP_OPT ("-c",  "Start secondary CPU core")
BAREBOX_CMD_HELP_OPT ("-z",  "Turn off secondary CPU core")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(smc)
	.cmd = do_smc,
	BAREBOX_CMD_DESC("secure monitor test command")
	BAREBOX_CMD_GROUP(CMD_GRP_MISC)
BAREBOX_CMD_END
