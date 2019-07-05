// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>
#include <mach/stm32.h>
#include <mach/bsec.h>
#include <mach/revision.h>

/* DBGMCU register */
#define DBGMCU_IDC		(STM32_DBGMCU_BASE + 0x00)
#define DBGMCU_APB4FZ1		(STM32_DBGMCU_BASE + 0x2C)
#define DBGMCU_APB4FZ1_IWDG2	BIT(2)
#define DBGMCU_IDC_DEV_ID_MASK	GENMASK(11, 0)
#define DBGMCU_IDC_DEV_ID_SHIFT	0
#define DBGMCU_IDC_REV_ID_MASK	GENMASK(31, 16)
#define DBGMCU_IDC_REV_ID_SHIFT	16

#define RCC_DBGCFGR		(STM32_RCC_BASE + 0x080C)
#define RCC_DBGCFGR_DBGCKEN	BIT(8)

/* BSEC OTP index */
#define BSEC_OTP_RPN	1
#define BSEC_OTP_PKG	16

/* Device Part Number (RPN) = OTP_DATA1 lower 8 bits */
#define RPN_SHIFT	0
#define RPN_MASK	GENMASK(7, 0)

/* Package = bit 27:29 of OTP16
 * - 100: LBGA448  (FFI) => AA = LFBGA 18x18mm 448 balls p. 0.8mm
 * - 011: LBGA354  (LCI) => AB = LFBGA 16x16mm 359 balls p. 0.8mm
 * - 010: TFBGA361 (FFC) => AC = TFBGA 12x12mm 361 balls p. 0.5mm
 * - 001: TFBGA257 (LCC) => AD = TFBGA 10x10mm 257 balls p. 0.5mm
 * - others: Reserved
 */
#define PKG_SHIFT	27
#define PKG_MASK	GENMASK(2, 0)

#define PKG_AA_LBGA448	4
#define PKG_AB_LBGA354	3
#define PKG_AC_TFBGA361	2
#define PKG_AD_TFBGA257	1

static int __stm32mp_cputype;
int stm32mp_cputype(void)
{
	return __stm32mp_cputype;
}

static int __stm32mp_silicon_revision;
int stm32mp_silicon_revision(void)
{
	return __stm32mp_silicon_revision;
}

static int __stm32mp_package;
int stm32mp_package(void)
{
	return __stm32mp_package;
}

static inline u32 read_idc(void)
{
	setbits_le32(RCC_DBGCFGR, RCC_DBGCFGR_DBGCKEN);
	return readl(IOMEM(DBGMCU_IDC));
}

/* Get Device Part Number (RPN) from OTP */
static u32 get_cpu_rpn(u32 *rpn)
{
	int ret = bsec_read_field(BSEC_OTP_RPN, rpn);
	if (ret)
		return ret;

	*rpn = (*rpn >> RPN_SHIFT) & RPN_MASK;
	return 0;
}

static u32 get_cpu_revision(void)
{
	return (read_idc() & DBGMCU_IDC_REV_ID_MASK) >> DBGMCU_IDC_REV_ID_SHIFT;
}

static u32 get_cpu_type(u32 *type)
{
	u32 id;
	int ret = get_cpu_rpn(type);
	if (ret)
		return ret;

	id = (read_idc() & DBGMCU_IDC_DEV_ID_MASK) >> DBGMCU_IDC_DEV_ID_SHIFT;
	*type |= id << 16;
	return 0;
}

static int get_cpu_package(u32 *pkg)
{
	int ret = bsec_read_field(BSEC_OTP_PKG, pkg);
	if (ret)
		return ret;

	*pkg = (*pkg >> PKG_SHIFT) & PKG_MASK;
	return 0;
}

static int setup_cpu_type(void)
{
	const char *cputypestr;
	const char *cpupkgstr;

	get_cpu_type(&__stm32mp_cputype);
	switch (__stm32mp_cputype) {
	case CPU_STM32MP157Cxx:
		cputypestr = "157C";
		break;
	case CPU_STM32MP157Axx:
		cputypestr = "157A";
		break;
	case CPU_STM32MP153Cxx:
		cputypestr = "153C";
		break;
	case CPU_STM32MP153Axx:
		cputypestr = "153A";
		break;
	case CPU_STM32MP151Cxx:
		cputypestr = "151C";
		break;
	case CPU_STM32MP151Axx:
		cputypestr = "151A";
		break;
	default:
		cputypestr = "????";
		break;
	}

	get_cpu_package(&__stm32mp_package );
	switch (__stm32mp_package) {
	case PKG_AA_LBGA448:
		cpupkgstr = "AA";
		break;
	case PKG_AB_LBGA354:
		cpupkgstr = "AB";
		break;
	case PKG_AC_TFBGA361:
		cpupkgstr = "AC";
		break;
	case PKG_AD_TFBGA257:
		cpupkgstr = "AD";
		break;
	default:
		cpupkgstr = "??";
		break;
	}

	__stm32mp_silicon_revision = get_cpu_revision();

	pr_debug("cputype = 0x%x, package = 0x%x, revision = 0x%x\n",
		 __stm32mp_cputype, __stm32mp_package, __stm32mp_silicon_revision);
	pr_info("detected STM32MP%s%s Rev.%c\n", cputypestr, cpupkgstr,
		(__stm32mp_silicon_revision >> 12) + 'A' - 1);
	return 0;
}

static int stm32mp_init(void)
{
	setup_cpu_type();

	return 0;
}
postcore_initcall(stm32mp_init);
