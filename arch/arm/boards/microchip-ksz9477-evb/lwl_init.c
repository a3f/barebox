typedef int make_iso_compilers_happy;

#define CONFIG_BUS_SPEED_133MHZ
#define CONFIG_CPU_CLK_528MHZ
/*
 * PMC Setting
 *
 * The main oscillator is enabled as soon as possible in the lowlevel_clock_init
 * and MCK is switched on the main oscillator.
 */
#if defined(CONFIG_BUS_SPEED_133MHZ)

#define MASTER_CLOCK		132000000

#if defined(CONFIG_CPU_CLK_528MHZ)

/* PCK = 528MHz, MCK = 132MHz */
#define PLLA_MULA		43
#define BOARD_PCK		((unsigned long)(BOARD_MAINOSC * \
							(PLLA_MULA + 1)))
#define BOARD_MCK		((unsigned long)((BOARD_MAINOSC * \
							(PLLA_MULA + 1)) / 4))

#define BOARD_CKGR_PLLA		(AT91C_CKGR_SRCA | AT91C_CKGR_OUTA_0)
#define BOARD_PLLACOUNT		(0x3F << 8)
#define BOARD_MULA		((AT91C_CKGR_MULA << 2) & (PLLA_MULA << 18))
#define BOARD_DIVA		(AT91C_CKGR_DIVA & 1)

#define 	AT91C_PMC_MDIV_4		(0x2UL <<  8)
#define AT91C_PMC_CSS		(0x7UL <<  0)
#define 	AT91C_PMC_CSS_SLOW_CLK		(0x0UL)
#define 	AT91C_PMC_CSS_MAIN_CLK		(0x1UL)
#define 	AT91C_PMC_CSS_PLLA_CLK		(0x2UL)
#define 	AT91C_PMC_CSS_UPLL_CLK		(0x3UL)
#define 	AT91C_PMC_CSS_SYS_CLK		(0x4UL)
#define BOARD_PRESCALER_MAIN_CLOCK	(AT91C_PMC_MDIV_4 \
					| AT91C_PMC_CSS_MAIN_CLK)

#define BOARD_PRESCALER_PLLA		(AT91C_PMC_MDIV_4 \
					| AT91C_PMC_CSS_PLLA_CLK)

#elif defined(CONFIG_CPU_CLK_396MHZ)

/* PCK = 396MHz, MCK = 132MHz */
#define PLLA_MULA		65
#define BOARD_PCK		((unsigned long)((BOARD_MAINOSC * \
						(PLLA_MULA + 1)) / 2))
#define BOARD_MCK		((unsigned long)((BOARD_MAINOSC * \
						(PLLA_MULA + 1)) / 2 / 3))

#define BOARD_CKGR_PLLA		(AT91C_CKGR_SRCA | AT91C_CKGR_OUTA_0)
#define BOARD_PLLACOUNT		(0x3F << 8)
#define BOARD_MULA		((AT91C_CKGR_MULA << 2) & (PLLA_MULA << 18))
#define BOARD_DIVA		(AT91C_CKGR_DIVA & 1)

/* Master Clock Register */
#define BOARD_PRESCALER_MAIN_CLOCK	(AT91C_PMC_PLLADIV2_2 \
					| AT91C_PMC_MDIV_3 \
					| AT91C_PMC_CSS_MAIN_CLK)

#define BOARD_PRESCALER_PLLA		(AT91C_PMC_PLLADIV2_2 \
					| AT91C_PMC_MDIV_3 \
					| AT91C_PMC_CSS_PLLA_CLK)

#elif defined(CONFIG_CPU_CLK_266MHZ)

/* PCK = 264MHz, MCK = 132MHz */
#define PLLA_MULA		43
#define BOARD_PCK		((unsigned long)((BOARD_MAINOSC * \
						(PLLA_MULA + 1)) / 2))
#define BOARD_MCK		((unsigned long)((BOARD_MAINOSC * \
						(PLLA_MULA + 1)) / 2 / 2))

#define BOARD_CKGR_PLLA		(AT91C_CKGR_SRCA | AT91C_CKGR_OUTA_0)
#define BOARD_PLLACOUNT		(0x3F << 8)
#define BOARD_MULA		((AT91C_CKGR_MULA << 2) & (PLLA_MULA << 18))
#define BOARD_DIVA		(AT91C_CKGR_DIVA & 1)

#define BOARD_PRESCALER_MAIN_CLOCK	(AT91C_PMC_PLLADIV2_2 \
					| AT91C_PMC_MDIV_2 \
					| AT91C_PMC_CSS_MAIN_CLK)

#define BOARD_PRESCALER_PLLA		(AT91C_PMC_PLLADIV2_2 \
					| AT91C_PMC_MDIV_2 \
					| AT91C_PMC_CSS_PLLA_CLK)

#else
#error "No cpu clock provided!"
#endif /* #if defined(CONFIG_CPU_CLK_528MHZ) */

#elif defined(CONFIG_BUS_SPEED_166MHZ)

#if defined(CONFIG_CPU_CLK_498MHZ)

/* PCK = 496MHz, MCK = 166MHz */
#define PLLA_MULA		82
#define BOARD_PCK		((unsigned long)((BOARD_MAINOSC * \
						(PLLA_MULA + 1)) / 2))
#define BOARD_MCK		((unsigned long)((BOARD_MAINOSC * \
						(PLLA_MULA + 1)) / 2 / 3))
#define MASTER_CLOCK		166000000

#define BOARD_CKGR_PLLA		(AT91C_CKGR_SRCA | AT91C_CKGR_OUTA_0)
#define BOARD_PLLACOUNT		(AT91C_CKGR_PLLACOUNT && (0x3F << 8))
#define BOARD_MULA		((AT91C_CKGR_MULA << 2) & (PLLA_MULA << 18))
#define BOARD_DIVA		(AT91C_CKGR_DIVA & 1)

/* Master Clock Register */
#define BOARD_PRESCALER_MAIN_CLOCK	(AT91C_PMC_PLLADIV2_2 \
					| AT91C_PMC_MDIV_3 \
					| AT91C_PMC_CSS_MAIN_CLK)

#define BOARD_PRESCALER_PLLA		(AT91C_PMC_PLLADIV2_2 \
					| AT91C_PMC_MDIV_3 \
					| AT91C_PMC_CSS_PLLA_CLK)

#elif defined(CONFIG_CPU_CLK_332MHZ)

/* PCK = 330MHz, MCK = 166MHz */
#define PLLA_MULA		54
#define BOARD_PCK		((unsigned long)((BOARD_MAINOSC * \
						(PLLA_MULA + 1)) / 2))
#define BOARD_MCK		((unsigned long)((BOARD_MAINOSC * \
						(PLLA_MULA + 1)) / 2 / 2))
#define MASTER_CLOCK		165000000

#define BOARD_CKGR_PLLA		(AT91C_CKGR_SRCA | AT91C_CKGR_OUTA_0)
#define BOARD_PLLACOUNT		(0x3F << 8)
#define BOARD_MULA		((AT91C_CKGR_MULA << 2) & (PLLA_MULA << 18))
#define BOARD_DIVA		(AT91C_CKGR_DIVA & 1)

/* Master Clock Register */
#define BOARD_PRESCALER_MAIN_CLOCK	(AT91C_PMC_PLLADIV2_2 \
					| AT91C_PMC_MDIV_2 \
					| AT91C_PMC_CSS_MAIN_CLK)

#define BOARD_PRESCALER_PLLA		(AT91C_PMC_PLLADIV2_2 \
					| AT91C_PMC_MDIV_2 \
					| AT91C_PMC_CSS_PLLA_CLK)

#else
#error "No cpu clock provided!"
#endif /* #if defined(CONFIG_CPU_CLK_498MHZ) */

#else
#error "No main clock provided!"
#endif /* #if defined(CONFIG_BUS_SPEED_133MHZ) */

#define writel(value, addr) \
	(*(volatile unsigned int *)((unsigned long)addr)) = (value)
#define readl(addr) \
	(*(volatile unsigned int *)((unsigned long)addr))

#define AT91C_BASE_PMC		0xfffffc00
#define AT91C_ID_AIC	0	/* Advanced Interrupt Controller (AIC) */
#define AT91C_ID_SYS	1	/* System Controller Interrupt */
#define AT91C_ID_DBGU	2	/* Debug Unit Interrupt */
#define AT91C_ID_PIT	3	/* Periodic Interval Timer Interrupt */
#define AT91C_ID_WDT	4	/* Watchdog Timer Interrupt */
#define AT91C_ID_SMC	5	/* Multi-bit ECC Interrupt */
#define AT91C_ID_PIOA	6	/* Parallel I/O Controller A */
#define AT91C_ID_PIOB	7	/* Parallel I/O Controller B */
#define AT91C_ID_PIOC	8	/* Parallel I/O Controller C */
#define AT91C_ID_PIOD	9	/* Parallel I/O Controller D */
#define AT91C_ID_PIOE	10	/* Parallel I/O Controller E */
#define write_pmc(offset, value) writel(value, offset + AT91C_BASE_PMC)
#define read_pmc(offset) readl(offset + AT91C_BASE_PMC)
#define pmc_enable_periph_clock _pmc_enable_periph_clock
#define PMC_PCER1	0x100	/* Peripheral Clock Enable Register1  32:63 PERI_ID */
#define PMC_PCDR1	0x104	/* Peripheral Clock Disable Register1 32:63 PERI_ID */
#define PMC_PCSR1	0x108	/* Peripheral Clock Status Register1  32:63 PERI_ID */
#define PMC_PCR		0x10C	/* Peripheral Control Register */
#define PMC_OCR		0x110	/* Oscillator Calibration Register */
#define PMC_PCER	0x10	/* Peripheral Clock Enable Register  (0:31 PERI_ID) */
#define AT91C_NUM_PIO		5
#define AT91C_BASE_PIOA		0xfffff200
#define AT91C_BASE_PIOB		0xfffff400
#define AT91C_BASE_PIOC		0xfffff600
#define AT91C_BASE_PIOD		0xfffff800
#define AT91C_BASE_PIOE		0xfffffa00

static inline int pio_base_addr(unsigned int pio)
{
	if (pio > AT91C_NUM_PIO)
		return -1;

	if (pio == 0)
		return AT91C_BASE_PIOA;
	else if (pio == 1)
		return AT91C_BASE_PIOB;
	else if (pio == 2)
		return AT91C_BASE_PIOC;
	else if (pio == 3)
#ifdef AT91C_BASE_PIOD
		return AT91C_BASE_PIOD;
#else
		return -1;
#endif
	else if (pio == 4)
#ifdef AT91C_BASE_PIOE
		return AT91C_BASE_PIOE;
#else
		return -1;
#endif
	else
		return -1;
}
static inline void write_pio(unsigned int pio,
			unsigned int offset,
			const unsigned int value)
{
	int base = pio_base_addr(pio);

	if (base == -1)
		return;

	writel(value, offset + base);
}

static inline unsigned int read_pio(unsigned int pio, unsigned int offset)
{
	int base = pio_base_addr(pio);

	if (base == -1)
		return 0;

	return readl(offset + base);
}
static int _pmc_enable_periph_clock(unsigned int periph_id)
{
	unsigned int mask = 0x01 << (periph_id % 32);

	if ((periph_id / 32) == 1)
		write_pmc(PMC_PCER1, mask);
	else if ((periph_id / 32) == 0)
		write_pmc(PMC_PCER, mask);
	else
		return -1;

	return 0;
}
#define PIO_DEFAULT	(0 << 0)
#define PIO_PULLUP	(1 << 0)
#define PIO_DEGLITCH	(1 << 1)
#define PIO_OPENDRAIN	(1 << 2)
#define PIO_PULLDOWN	(1 << 3)
#define PIO_DRVSTR_LO	(1 << 4)
#define PIO_DRVSTR_ME	(1 << 5)
#define PIO_DRVSTR_HI	(1 << 6)
enum pio_type {
	PIO_PERIPH_A,
	PIO_PERIPH_B,
	PIO_PERIPH_C,
	PIO_PERIPH_D,
	PIO_PERIPH_E,
	PIO_PERIPH_F,
	PIO_PERIPH_G,
	PIO_INPUT,
	PIO_OUTPUT
};

struct pio_desc {
	const char	*pin_name;	/* Pin Name */
	unsigned int	pin_num;	/* Pin number */
	unsigned int	default_value;	/* Default value for outputs */
	unsigned char	attribute;
	enum pio_type	type;
};
#define	AT91C_PIN_PA(io)	(0 * PIO_NUM_IO + io)
#define	AT91C_PIN_PB(io)	(1 * PIO_NUM_IO + io)
#define	AT91C_PIN_PC(io)	(2 * PIO_NUM_IO + io)
#define	AT91C_PIN_PD(io)	(3 * PIO_NUM_IO + io)
#define	AT91C_PIN_PE(io)	(4 * PIO_NUM_IO + io)

/* Number of IO handled by one PIO controller */
#define PIO_NUM_IO		32
static inline unsigned pin_to_controller(unsigned pin)
{
	return (pin) / PIO_NUM_IO;
}
static inline unsigned pin_to_mask(unsigned pin)
{
	return 1 << ((pin) % PIO_NUM_IO);
}
#define	PIO_IDR		0x0024	/* PIO Interrupt Disable Register */
#define PIO_PPUDR	0x0060	/* Pull-up Disable Register */
#define PIO_PPUER	0x0064	/* Pull-up Enable Register */
#define PIO_PPUSR	0x0068	/* Pull-up Status Register */
#define	PIO_IDR		0x0024	/* PIO Interrupt Disable Register */
#define PIO_ASR		0x0070	/* Peripheral Select Register 1 */
#define PIO_PDR		0x0004	/* PIO Disable Register */
#undef CPU_HAS_PIO4
#define CPU_HAS_PIO3 1
#define PIO_SP1		0x0070	/* Select Peripheral 1 Register */
#define PIO_SP2		0x0074	/* Select Peripheral 2 Register */
#define PIO_ABSR	0x0078	/* AB Select Status Register */
/* 0x007c ~ 0x008c */
#define PIO_PPDDR	0x0090	/* Pull-down Disable Register */
#define PIO_PPDER	0x0094	/* Pull-down Enable Register */
#define PIO_PPDSR	0x0098	/* Pull-down Status Register */
static int pio_set_a_periph(unsigned pin, int config)
{
	unsigned pio = pin_to_controller(pin);
	unsigned mask = pin_to_mask(pin);

	if (config & PIO_PULLUP && config & PIO_PULLDOWN)
		return -1;

#if defined CPU_HAS_PIO4
	pio4_set_periph(pio, mask, config, AT91C_PIO_CFGR_FUNC_PERIPH_A);
#else
	write_pio(pio, PIO_IDR, mask);
	write_pio(pio, ((config & PIO_PULLUP) ? PIO_PPUER : PIO_PPUDR), mask);
#ifdef CPU_HAS_PIO3
	write_pio(pio, ((config & PIO_PULLDOWN) ? PIO_PPDER : PIO_PPDDR), mask);

	write_pio(pio, PIO_SP1, read_pio(pio, PIO_SP1) & ~mask);
	write_pio(pio, PIO_SP2, read_pio(pio, PIO_SP2) & ~mask);
#else
	write_pio(pio, PIO_ASR, mask);
#endif
	write_pio(pio, PIO_PDR, mask);
#endif

	return 0;
}

#define BAUDRATE(mck, baud) \
	(((((mck) * 10) / ((baud) * 16)) % 10) >= 5) ? \
	(mck / (baud * 16) + 1) : ((mck) / (baud * 16))

static void initialize_dbgu(void)
{
	/* Configure DBGU pin */
	const struct pio_desc dbgu_pins[] = {
		{"RXD", AT91C_PIN_PB(30), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"TXD", AT91C_PIN_PB(31), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};

	/* *** Register offset in AT91S_DBGU structure ***/
#define DBGU_CR		0x00	/* Control Register */
#define DBGU_MR		0x04	/* Mode Register */
#define DBGU_IER	0x08	/* Interrupt Enable Register */
#define DBGU_IDR	0x0C	/* Interrupt Disable Register */
#define DBGU_IMR	0x10	/* Interrupt Mask Register */
#define DBGU_CSR	0x14	/* Channel Status Register */
#define DBGU_RHR	0x18	/* Receiver Holding Register */
#define DBGU_THR	0x1C	/* Transmitter Holding Register */
#define DBGU_BRGR	0x20	/* Baud Rate Generator Register */
#define DBGU_CIDR	0x40	/* Chip ID Register */
#define DBGU_EXID	0x44	/* Chip ID Extension Register */
#define DBGU_FNTR	0x48	/* Force NTRST Register */
#define DBGU_ADDRSIZE	0xEC	/* DBGU ADDRSIZE REGISTER */
#define DBGU_IPNAME1	0xF0	/* DBGU IPNAME1 REGISTER */
#define DBGU_IPNAME2	0xF4	/* DBGU IPNAME2 REGISTER */
#define DBGU_FEATURES	0xF8	/* DBGU FEATURES REGISTER */
#define DBGU_VER	0xFC	/* DBGU VERSION REGISTER */

	/* -------- DBGU_CR : (DBGU Offset: 0x0) Debug Unit Control Register --------*/ 
#define AT91C_DBGU_RSTRX	(0x1UL << 2)
#define AT91C_DBGU_RSTTX	(0x1UL << 3)
#define AT91C_DBGU_RXEN		(0x1UL << 4)
#define AT91C_DBGU_RXDIS	(0x1UL << 5)
#define AT91C_DBGU_TXEN		(0x1UL << 6)
#define AT91C_DBGU_TXDIS	(0x1UL << 7)
#define AT91C_DBGU_RSTSTA	(0x1UL << 8)
	/*  Configure the dbgu pins */
	pmc_enable_periph_clock(AT91C_ID_PIOB);
	const struct pio_desc *pio_desc = dbgu_pins;

	unsigned pio, pin = 0;

	do {
		if (pio_desc == 0) break;

		/*
		 * Sets all the pio muxing of the corresponding device as defined
		 * in its platform_data struct
		 */
		while (pio_desc->pin_name) {
			pio = pin_to_controller(pio_desc->pin_num);
			if (pio_desc->type == PIO_PERIPH_A) {
				pio_set_a_periph(pio_desc->pin_num,
						 pio_desc->attribute);
			}
			++pin;
			++pio_desc;
		}

	} while (0);

	/* Enable clock */
	pmc_enable_periph_clock(AT91C_ID_DBGU);

#define AT91C_BASE_DBGU		0xffffee00
#define  USART_BASE AT91C_BASE_DBGU

#define write_usart(offset, value) writel(value, offset + USART_BASE)
#define read_usart(offset) readl(offset + USART_BASE)

#define AT91C_DBGU_NBSTOP	(0x3UL << 12)
#define		AT91C_DBGU_NBSTOP_1BIT			(0x0UL << 12)
#define		AT91C_DBGU_NBSTOP_1_5BIT		(0x1UL << 12)
#define		AT91C_DBGU_NBSTOP_2BIT			(0x2UL << 12)
#define AT91C_DBGU_CHRL		(0x3UL << 6)
#define		AT91C_DBGU_CHRL_5BIT			(0x0UL << 6)
#define		AT91C_DBGU_CHRL_6BIT			(0x1UL << 6)
#define		AT91C_DBGU_CHRL_7BIT			(0x2UL << 6)
#define		AT91C_DBGU_CHRL_8BIT			(0x3UL << 6)
#define AT91C_DBGU_PAR		(0x7UL << 9)
#define 	AT91C_DBGU_PAR_EVEN			(0x0UL << 9)
#define 	AT91C_DBGU_PAR_ODD			(0x1UL << 9)
#define 	AT91C_DBGU_PAR_SPACE			(0x2UL << 9)
#define 	AT91C_DBGU_PAR_MARK			(0x3UL << 9)
#define 	AT91C_DBGU_PAR_NONE			(0x4UL << 9)
#define AT91C_DBGU_CHMODE	(0x3UL << 14)
#define 	AT91C_DBGU_CHMODE_NORMAL		(0x0UL << 14)
#define 	AT91C_DBGU_CHMODE_AUTO			(0x1UL << 14)
#define 	AT91C_DBGU_CHMODE_LOCAL			(0x2UL << 14)
#define 	AT91C_DBGU_CHMODE_REMOTE		(0x3UL << 14)
	/* Disable interrupts */
	write_usart(DBGU_IDR, -1);

	/* Reset the receiver and transmitter */
	write_usart(DBGU_CR, AT91C_DBGU_RSTRX
		    | AT91C_DBGU_RSTTX
		    | AT91C_DBGU_RXDIS
		    | AT91C_DBGU_TXDIS);

	/* Configure the baudrate */
	write_usart(DBGU_BRGR, BAUDRATE(MASTER_CLOCK, 115200));

	/* Configure USART in Asynchronous mode */
	write_usart(DBGU_MR, AT91C_DBGU_PAR_NONE
		    | AT91C_DBGU_CHMODE_NORMAL
		    | AT91C_DBGU_CHRL_8BIT
		    | AT91C_DBGU_NBSTOP_1BIT);

	/* Enable RX and Tx */
	write_usart(DBGU_CR, AT91C_DBGU_RXEN | AT91C_DBGU_TXEN);
}







#define AT91C_BASE_WDT		0xfffffe40
#define wdt_write(offset, value) writel(value, AT91C_BASE_WDT + offset)
#define wdt_read(offset) readl(AT91C_BASE_WDT + offset)
int _timer_init(void)
{
#define PIT_MR		0x00	/* Period Interval Mode Register */
#define PIT_SR		0x04	/* Period Interval Status Register */
#define PIT_PIVR	0x08	/* Period Interval Value Register */
#define PIT_PIIR	0x0c	/* Period Interval Image Register */

	/*-------- PITC_PIMR : (PITC Offset: 0x0) Periodic Interval Mode Register -------*/
#define AT91C_PIT_PIV		(0xFFFFF << 0)	/* Periodic Interval Value */
#define AT91C_PIT_PITEN	(0x01 << 24)	/* Periodic Interval Timer Enabled */
#define AT91C_PIT_PITIEN	(0x01 << 25)	/* Periodic Interval Timer Interrupt Enable */
	/*-------- PITC_PISR : (PITC Offset: 0x4) Periodic Interval Status Register --------*/
#define AT91C_PIT_PITS		(0x01 << 0)	/* (PITC) Periodic Interval Timer Status */
	/*-------- PITC_PIVR : (PITC Offset: 0x8) Periodic Interval Value Register --------*/
	/*-------- PITC_PIIR : (PITC Offset: 0xc) Periodic Interval Image Register --------*/
#define AT91C_PIT_CPIV		(0xFFFFF << 0)	/* (PITC) Current Periodic Interval Value */
#define AT91C_PIT_PICNT	(0xFFF << 20)	/* (PITC) Periodic Interval Counter */
#define MAX_PIV		0xfffff
#define pit_readl(reg) readl(AT91C_BASE_PITC + reg)
#define pit_writel(value, reg) writel(value, (AT91C_BASE_PITC + reg));
#define AT91C_BASE_PITC		0xfffffe30
	pit_writel((MAX_PIV | AT91C_PIT_PITEN), PIT_MR);

	/* Enable PITC Clock */
#define AT91C_ID_SYS	1	/* System Controller Interrupt */
#ifdef AT91C_ID_PIT /* this branch */
	pmc_enable_periph_clock(AT91C_ID_PIT);
#else
	pmc_enable_periph_clock(AT91C_ID_SYS);
#endif
	return 0;
}

#define AT91C_PMC_MDIV		(0x3UL <<  8)
#define AT91C_PMC_PLLADIV2	(0x1UL << 12)
#define AT91C_PMC_H32MXDIV	(0x1UL << 24)
#define AT91C_PMC_CSS		(0x7UL <<  0)
#define AT91C_PMC_MCKRDY	(0x1UL << 3)
#define PMC_SR		0x68	/* Status Register */
#define SAMA5D3X
int _pmc_cfg_mck(unsigned int pmc_mckr)
{
	unsigned int tmp;

	/*
	 * Program the PRES field in the PMC_MCKR register
	 */
#define PMC_MCKR	0x30	/* Master Clock Register */
#define	AT91C_PMC_PRES		(0xFUL << 2)
#define	AT91C_PMC_ALT_PRES	(0xFUL << 4)
	tmp = read_pmc(PMC_MCKR);
	tmp &= (~(0x1 << 13));
#if defined(AT91SAM9X5) || defined(AT91SAM9N12) || defined(SAMA5D3X) \
	|| defined(SAMA5D4) || defined(SAMA5D2)
	tmp &= (~AT91C_PMC_ALT_PRES);
	tmp |= (pmc_mckr & AT91C_PMC_ALT_PRES);
#else
	tmp &= (~AT91C_PMC_PRES);
	tmp |= (pmc_mckr & AT91C_PMC_PRES);
#endif
	write_pmc(PMC_MCKR, tmp);

	/*
	 * Program the MDIV field in the PMC_MCKR register
	 */
	tmp = read_pmc(PMC_MCKR);
	tmp &= (~AT91C_PMC_MDIV);
	tmp |= (pmc_mckr & AT91C_PMC_MDIV);
	write_pmc(PMC_MCKR, tmp);

	/*
	 * Program the PLLADIV2 field in the PMC_MCKR register
	 */
	tmp = read_pmc(PMC_MCKR);
	tmp &= (~AT91C_PMC_PLLADIV2);
	tmp |= (pmc_mckr & AT91C_PMC_PLLADIV2);
	write_pmc(PMC_MCKR, tmp);

	/*
	 * Program the H32MXDIV field in the PMC_MCKR register
	 */
	tmp = read_pmc(PMC_MCKR);
	tmp &= (~AT91C_PMC_H32MXDIV);
	tmp |= (pmc_mckr & AT91C_PMC_H32MXDIV);
	write_pmc(PMC_MCKR, tmp);

	/*
	 * Program the CSS field in the PMC_MCKR register,
	 * wait for MCKRDY bit to be set in the PMC_SR register
	 */
	tmp = read_pmc(PMC_MCKR);
	tmp &= (~AT91C_PMC_CSS);
	tmp |= (pmc_mckr & AT91C_PMC_CSS);
	write_pmc(PMC_MCKR, tmp);

	while (!(read_pmc(PMC_SR) & AT91C_PMC_MCKRDY))
		;

	return 0;
}
#define AT91C_DBGU_TXRDY	(0x1UL <<  1)
static void usart_putc(const char c)
{
	while (!(read_usart(DBGU_CSR) & AT91C_DBGU_TXRDY))
		;

	write_usart(DBGU_THR, c);
}

static void usart_puts(const char *ptr)
{
	int i = 0;

	while (ptr[i] != '\0') {
		if (ptr[i] == '\n')
			usart_putc('\r');
		usart_putc(ptr[i]);
		i++;
	}
}

#define PMC_PLLAR	0x28	/* PLL A Register */
#define AT91C_CKGR_SRCA		(0x1UL << 29)
#define AT91C_PMC_LOCKA		(0x1UL << 1)
#define PLLA_SETTINGS		(BOARD_CKGR_PLLA \
				| BOARD_PLLACOUNT \
				| BOARD_MULA \
				| BOARD_DIVA)
#define AT91C_CKGR_OUTA		(0x3UL << 14)
#define 	AT91C_CKGR_OUTA_0		(0x0UL << 14)
#define 	AT91C_CKGR_OUTA_1		(0x1UL << 14)
#define 	AT91C_CKGR_OUTA_2		(0x2UL << 14)
#define 	AT91C_CKGR_OUTA_3		(0x3UL << 14)
#define AT91C_CKGR_MULA		(0xff << 16)
#define	AT91C_CKGR_ALT_MULA	(0x7f << 18)
#define		AT91C_CKGR_ALT_MULA_MSK		0x7f
#define		AT91C_CKGR_ALT_MULA_OFFSET	18
#define AT91C_CKGR_SRCA		(0x1UL << 29)
#define AT91C_CKGR_DIVA		(0xff << 0)
#define		AT91C_CKGR_DIVA_MSK		0xff
#define		AT91C_CKGR_DIVA_OFFSET		0
#define		AT91C_CKGR_DIVA_0		0x0
#define		AT91C_CKGR_DIVA_BYPASS		0x1
void _hw_init(void)
{
	/* Disable watchdog */ {
		unsigned int reg;
#define WDTC_MR		0x04	/* Watchdog Mode Register */
#define AT91C_WDTC_WDDIS	(0x1UL << 15)
		reg = wdt_read(WDTC_MR);
		reg |= AT91C_WDTC_WDDIS;
		wdt_write(WDTC_MR, reg);
	}

	/*
	 * At this stage the main oscillator
	 * is supposed to be enabled PCK = MCK = MOSC
	 */

	/* Configure PLLA = MOSC * (PLL_MULA + 1) / PLL_DIVA */ {
		write_pmc((unsigned int)PMC_PLLAR, 0 | AT91C_CKGR_SRCA);
		write_pmc((unsigned int)PMC_PLLAR, PLLA_SETTINGS);

		while (!(read_pmc(PMC_SR) & AT91C_PMC_LOCKA))
			;
	}

#define PMC_PLLICPR	0x80	/* PLL Charge Pump Current Register */
#define 	AT91C_PMC_IPLLA_3		(0x3UL <<  8)
	/* Initialize PLLA charge pump */
	write_pmc(PMC_PLLICPR, AT91C_PMC_IPLLA_3);

	/* Switch PCK/MCK on Main clock output */
	_pmc_cfg_mck(BOARD_PRESCALER_MAIN_CLOCK);


	/* Switch PCK/MCK on PLLA output */
	_pmc_cfg_mck(BOARD_PRESCALER_PLLA);

#ifdef CONFIG_USER_HW_INIT
	/* Set GMAC & EMAC pins to output low */
	at91_special_pio_output_low();
#endif

	/* Init timer */
	_timer_init();

	/* initialize the dbgu */
	initialize_dbgu();
	usart_puts("Helloa Worldekkk, " __TIME__ "\n");
}



