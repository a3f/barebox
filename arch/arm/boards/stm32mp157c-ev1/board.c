// SPDX-License-Identifier: GPL-2.0+

#include <bootsource.h>
#include <common.h>
#include <init.h>
#include <mach/bbu.h>

static int ev1_device_init(void)
{
	int flags;

	if (!of_machine_is_compatible("st,stm32mp157c-ev1"))
		return 0;

	flags = bootsource_get_instance() == 0 ? BBU_HANDLER_FLAG_DEFAULT : 0;
	stm32mp_bbu_mmc_register_handler("sd", "/dev/mmc0.ssbl", flags);

	flags = bootsource_get_instance() == 1 ? BBU_HANDLER_FLAG_DEFAULT : 0;
	stm32mp_bbu_mmc_register_handler("emmc", "/dev/mmc1.ssbl", flags);

	if (bootsource_get_instance() == 0)
		of_device_enable_path("/chosen/environment-sd");
	else
		of_device_enable_path("/chosen/environment-emmc");

	barebox_set_model("STM32MP157C-EV1");

	return 0;
}
device_initcall(ev1_device_init);
