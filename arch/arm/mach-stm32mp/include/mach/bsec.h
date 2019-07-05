// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 */

#ifndef __MACH_STM32MP_BSEC_H__
#define __MACH_STM32MP_BSEC_H__

#include <linux/arm-smccc.h>

#define STM32_SMC_BSEC			0x82001003

#define STM32_BSEC_OTP_MAX_VALUE	(95*4)

 /* Service for BSEC */
 #define STM32_SMC_READ_SHADOW           0x01
 #define STM32_SMC_PROG_OTP              0x02
 #define STM32_SMC_WRITE_SHADOW          0x03
 #define STM32_SMC_READ_OTP              0x04
 #define STM32_SMC_READ_ALL              0x05
 #define STM32_SMC_WRITE_ALL             0x06

static inline s64 stm32_bsec_smc(u32 svc, u8 op, u32 data1, u32 data2)
{
        struct arm_smccc_res res;

        arm_smccc_smc(svc, op, data1, data2, 0, 0, 0, 0, &res);
        if (res.a0)
                return -EINVAL;

        return res.a1;
}

static inline s64 stm32mp_bsec_read_shadow(size_t word_offset)
{
	s64 ret;

	ret = stm32_bsec_smc(STM32_SMC_BSEC, STM32_SMC_READ_SHADOW,
			     word_offset, 0);
	return ret;
}

#endif
