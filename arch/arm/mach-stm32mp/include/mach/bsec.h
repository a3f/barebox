#ifndef __MACH_STM32_OCOTP_H__
#define __MACH_STM32_OCOTP_H__

#include <linux/arm-smccc.h>

/*
 * Return status
 */
enum bsec_smc_t {
	BSEC_SMC_OK		= 0,
	BSEC_SMC_ERROR		= -1,
	BSEC_SMC_DISTURBED	= -2,
	BSEC_SMC_INVALID_PARAM	= -3,
	BSEC_SMC_PROG_FAIL	= -4,
	BSEC_SMC_LOCK_FAIL	= -5,
	BSEC_SMC_WRITE_FAIL	= -6,
	BSEC_SMC_SHADOW_FAIL	= -7,
	BSEC_SMC_TIMEOUT	= -8,
};

/* Service for BSEC */
#define BSEC_SMC_READ_SHADOW		0x01
#define BSEC_SMC_PROG_OTP		0x02
#define BSEC_SMC_WRITE_SHADOW		0x03
#define BSEC_SMC_READ_OTP		0x04
#define BSEC_SMC_READ_ALL		0x05
#define BSEC_SMC_WRITE_ALL		0x06

static inline enum bsec_smc_t __bsec_smc(void __iomem *bsec,
			     u8 op, u32 data1, u32 data2, unsigned *val)
{
        struct arm_smccc_res res;

        arm_smccc_smc((ulong)bsec, op, data1, data2, 0, 0, 0, 0, &res);
        if (val)
                *val = res.a1;

        return res.a0;
}

static inline enum bsec_smc_t bsec_read_field(void __iomem *base,
					      unsigned field, unsigned *val)
{
	return __bsec_smc(base, BSEC_SMC_READ_SHADOW, field, 0, val);
}

static inline enum bsec_smc_t bsec_write_field(void __iomem *base,
					       unsigned field, unsigned val)
{
	return __bsec_smc(base, BSEC_SMC_WRITE_SHADOW, field, val, NULL);
}

#endif
