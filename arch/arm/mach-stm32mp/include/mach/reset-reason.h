#ifndef __MACH_RESET_REASON_H__
#define __MACH_RESET_REASON_H__

#include <reset_source.h>

#define RCC_RSTF_POR		BIT(0)
#define RCC_RSTF_BOR		BIT(1)
#define RCC_RSTF_PAD		BIT(2)
#define RCC_RSTF_HCSS		BIT(3)
#define RCC_RSTF_VCORE		BIT(4)

#define RCC_RSTF_MPSYS		BIT(6)
#define RCC_RSTF_MCSYS		BIT(7)
#define RCC_RSTF_IWDG1		BIT(8)
#define RCC_RSTF_IWDG2		BIT(9)

#define RCC_RSTF_STDBY		BIT(11)
#define RCC_RSTF_CSTDBY		BIT(12)
#define RCC_RSTF_MPUP0		BIT(13)
#define RCC_RSTF_MPUP1		BIT(14)

struct stm32_reset_reason {
	uint32_t mask;
	enum reset_src_type type;
	int instance;
};

#endif
