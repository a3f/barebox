#include <common.h>
#include <asm/psci.h>
#include <asm/secure.h>
#include <command.h>
#include <getopt.h>
#include <linux/arm-smccc.h>
#include <stdio.h>

#ifdef CONFIG_CPU64
#define PSCI_CPU_ON ARM_PSCI_0_2_FN_CPU64_ON
#else
#define PSCI_CPU_ON ARM_PSCI_0_2_FN_CPU_ON
#endif

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
	struct arm_smccc_res res;

	// racy, possibly garbled, output
	printf("2nd CPU online, now turn off again\n");

	arm_smccc_smc(ARM_PSCI_0_2_FN_CPU_OFF,
		      0, 0, 0, 0, 0, 0, 0, &res);

	printf("2nd CPU still alive?\n");

	while (1)
		;
}

static int do_smc(int argc, char *argv[])
{
	int opt;
	struct arm_smccc_res res = {
		.a0 = 0xdeadbee0,
		.a1 = 0xdeadbee1,
		.a2 = 0xdeadbee2,
		.a3 = 0xdeadbee3,
	};

	if (argc < 2)
		return COMMAND_ERROR_USAGE;

	while ((opt = getopt(argc, argv, "nicz")) > 0) {
		switch (opt) {
		case 'n':
			if (IS_ENABLED(CONFIG_ARM_SECURE_MONITOR))
				armv7_secure_monitor_install();
			else
				printf("barebox not configured as secure monitor\n");
			break;
		case 'i':
			arm_smccc_smc(ARM_PSCI_0_2_FN_PSCI_VERSION,
				      0, 0, 0, 0, 0, 0, 0, &res);
			printf("found psci version %ld.%ld\n", res.a0 >> 16, res.a0 & 0xffff);
			break;
		case 'c':
			arm_smccc_smc(PSCI_CPU_ON,
				      1, (unsigned long)second_entry, 0, 0, 0, 0, 0, &res);
			if ((s32)res.a0 < 0) {
				printf("SMC failed: '%s'\n",
				       psci_xlate_error(res.a0));
				return COMMAND_ERROR;
			}
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
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
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(smc)
	.cmd = do_smc,
	BAREBOX_CMD_DESC("secure monitor test command")
	BAREBOX_CMD_GROUP(CMD_GRP_MISC)
	BAREBOX_CMD_HELP(cmd_smc_help)
BAREBOX_CMD_END
