// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix <a.fatoum@pengutronix.de>
 */

#include <init.h>
#include <common.h>
#include <io.h>
#include <linux/sizes.h>
#include <asm/psci.h>
#include <asm/gic.h>
#include <asm/system.h>
#include <mach/stm32.h>
#include <asm/secure.h>

#define BOOT_API_A7_CORE0_MAGIC_NUMBER	0xCA7FACE0
#define BOOT_API_A7_CORE1_MAGIC_NUMBER	0xCA7FACE1

#define MPIDR_AFF0			GENMASK(7, 0)

#define RCC_MP_GRSTCSETR		(STM32_RCC_BASE + 0x0404)
#define RCC_MP_GRSTCSETR_MPUP1RST	BIT(5)
#define RCC_MP_GRSTCSETR_MPUP0RST	BIT(4)
#define RCC_MP_GRSTCSETR_MPSYSRST	BIT(0)

#define STM32MP1_PSCI_NR_CPUS		2
#if STM32MP1_PSCI_NR_CPUS > ARM_SECURE_MAX_CPU
#error "invalid value for ARM_SECURE_MAX_CPU"
#endif

static u8 psci_state[STM32MP1_PSCI_NR_CPUS] = {
	 PSCI_AFFINITY_LEVEL_ON,
	 PSCI_AFFINITY_LEVEL_OFF
};

static void psci_set_state(int cpu, u8 state)
{
	psci_state[cpu] = state;
}

static void stm32mp_smp_kick_all_cpus(void)
{
	u32 gic_dist_addr;

	gic_dist_addr = get_gicd_base_address();

	/* kick all CPUs (except this one) by writing to GICD_SGIR */
	writel(1 << 24, gic_dist_addr + GICD_SGIR);
}

static int stm32mp_affinity_info(u32 target_affinity,
				 u32 lowest_affinity_level)
{
	u32 cpu = target_affinity & MPIDR_AFF0;

	if (lowest_affinity_level > 0)
		return ARM_PSCI_RET_INVAL;

	if (target_affinity & ~MPIDR_AFF0)
		return ARM_PSCI_RET_INVAL;

	if (cpu >= STM32MP1_PSCI_NR_CPUS)
		return ARM_PSCI_RET_INVAL;

	return psci_state[cpu];
}

static int stm32mp_migrate_info_type(void)
{
	/*
	 * in Power_State_Coordination_Interface_PDD_v1_1_DEN0022D.pdf
	 * return 2 = Trusted OS is either not present or does not require
	 * migration, system of this type does not require the caller
	 * to use the MIGRATE function.
	 * MIGRATE function calls return NOT_SUPPORTED.
	 */
	return 2;
}

static int stm32mp_cpu_on(u32 target_cpu)
{
	u32 cpu = target_cpu & MPIDR_AFF0;

	if (target_cpu & ~MPIDR_AFF0)
		return ARM_PSCI_RET_INVAL;

	if (cpu >= STM32MP1_PSCI_NR_CPUS)
		return ARM_PSCI_RET_INVAL;

	if (psci_state[cpu] == PSCI_AFFINITY_LEVEL_ON)
		return ARM_PSCI_RET_ALREADY_ON;

	/* write entrypoint in backup RAM register */
	writel(psci_cpu_entry, TAMP_BACKUP_BRANCH_ADDRESS);
	psci_set_state(cpu, PSCI_AFFINITY_LEVEL_ON_PENDING);

	/* write magic number in backup register */
	if (cpu == 0x01)
		writel(BOOT_API_A7_CORE1_MAGIC_NUMBER, TAMP_BACKUP_MAGIC_NUMBER);
	else
		writel(BOOT_API_A7_CORE0_MAGIC_NUMBER, TAMP_BACKUP_MAGIC_NUMBER);

	stm32mp_smp_kick_all_cpus();

	return ARM_PSCI_RET_SUCCESS;
}

static int stm32mp_cpu_off(void)
{
	u32 cpu;

	cpu = psci_get_cpu_id();

	psci_set_state(cpu, PSCI_AFFINITY_LEVEL_OFF);

	dsb();
	isb();

	/* reset core: wfi is managed by BootRom */
	if (cpu == 0x01)
		writel(RCC_MP_GRSTCSETR_MPUP1RST, RCC_MP_GRSTCSETR);
	else
		writel(RCC_MP_GRSTCSETR_MPUP0RST, RCC_MP_GRSTCSETR);

	/* just awaiting reset */
	while (1)
		asm("wfi");

	return 0;
}

static void stm32mp_system_reset(void)
{
	/* System reset */
	writel(RCC_MP_GRSTCSETR_MPSYSRST, RCC_MP_GRSTCSETR);
	/* just awaiting reset */
	while (1)
		asm("wfi");
}

static void stm32mp_system_off(void)
{
	/* System Off is not managed, waiting user power off
	 * TODO: handle I2C write in PMIC Main Control register bit 0 = SWOFF
	 */
	while (1)
		asm("wfi");
}

static struct psci_ops stm32mp_psci_ops = {
	.cpu_on = stm32mp_cpu_on,
	.cpu_off = stm32mp_cpu_off,
	.affinity_info = stm32mp_affinity_info,
	.migrate_info_type = stm32mp_migrate_info_type,
	.system_off = stm32mp_system_off,
	.system_reset = stm32mp_system_reset,
};

static int stm32mp_init(void)
{
	psci_set_ops(&stm32mp_psci_ops);

	return 0;
}
postcore_initcall(stm32mp_init);
