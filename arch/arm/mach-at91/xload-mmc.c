#include <common.h>
#include <mach/xload.h>
#include <mach/sama5_bootsource.h>
#include <mach/hardware.h>
#include <mach/sama5d2_ll.h>
#include <mach/gpio.h>
#include <linux/sizes.h>
#include <asm/cache.h>
#include <pbl.h>

static void __naked __noreturn xload_bb(void __noreturn (*bb)(void), u32 r4)
{
	asm volatile("mov r4, %0" : : "r"(r4) : );
	asm volatile("bx  %0"     : : "r"(bb) : );
}

static void at91_fat_start_image(struct pbl_bio *bio,
				 void *buf, unsigned int len,
				 u32 r4)
{
	void __noreturn (*bb)(void);
	int ret;

	ret = pbl_fat_load(bio, "barebox.bin", buf, len);
	if (ret) {
		pr_err("pbl_fat_load: error %d\n", ret);
		return;
	}

	bb = buf;

	sync_caches_for_execution();

	xload_bb(bb, r4);
}

/**
 * sama5d2_sdhci_start_image - Load and start an image from FAT-formatted SDHCI
 * @r4: value of r4 passed by BootROM
 *
 * Return: If successul, this function does not return. A negative error
 * code is returned when this function fails.
 */
void __noreturn sama5d2_sdhci_start_image(u32 r4)
{
	void *buf = (void *)SAMA5_DDRCS;
	void __iomem *base;
	struct pbl_bio bio;
	unsigned periph;
	const s8 *pin;
	const s8 sdhci0_pins[] = {
		2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 13, 10, 11, 12, -1
	}, sdhci1_pins[] = {
		18, 19, 20, 21, 22, 28, 30, -1
	};
	unsigned id;
	int ret;

	if (sama5_bootsource_instance(r4) == 0) {
		base = SAMA5D2_BASE_SDHC0;
		id = SAMA5D2_ID_SDMMC0;
		pin = sdhci0_pins;
		periph = AT91_MUX_PERIPH_A;
	} else {
		base = SAMA5D2_BASE_SDHC1;
		id = SAMA5D2_ID_SDMMC1;
		pin = sdhci1_pins;
		periph = AT91_MUX_PERIPH_E;
	}

	sama5d2_pmc_enable_periph_clock(SAMA5D2_ID_PIOA);
	for (; *pin >= 0; pin++) {
		at91_mux_pio4_set_periph(SAMA5D2_BASE_PIOA,
					 BIT(*pin), periph);
	}

	sama5d2_pmc_enable_periph_clock(id);
	sama5d2_pmc_enable_generic_clock(id, AT91_PMC_GCKCSS_UPLL_CLK, 1);

	ret = at91_sdhci_bio_init(&bio, base);
	if (ret)
		goto out_panic;

	at91_fat_start_image(&bio, buf, SZ_16M, r4);

out_panic:
	panic("FAT chainloading failed\n");
}
