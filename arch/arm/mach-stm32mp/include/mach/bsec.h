#ifndef __MACH_STM32_BSEC_H__
#define __MACH_STM32_BSEC_H__

#include <mach/smc.h>

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

static inline enum bsec_smc_t bsec_read_field(unsigned field, unsigned *val)
{
	return stm32mp_smc(STM32_SMC_BSEC, BSEC_SMC_READ_SHADOW,
			   field, 0, val);
}

static inline enum bsec_smc_t bsec_write_field(unsigned field, unsigned val)
{
	return stm32mp_smc(STM32_SMC_BSEC, BSEC_SMC_WRITE_SHADOW,
			   field, val, NULL);
}

#endif
