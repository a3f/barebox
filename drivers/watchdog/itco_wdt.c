// SPDX-License-Identifier: GPL-2.0
/*
 * EFI iTCO support (Version 3 and later)
 *
 * Copyright (c) 2006-2011 Wim Van Sebroeck <wim@iguana.be>.
 * Copyright (c) 2019 Siemens AG
 * Copyright (c) 2019 Ahmad Fatoum, Pengutronix
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Christian Storm <christian.storm@siemens.com>
 */
#define DEBUG 1
#define pr_fmt(fmt) "itco-wdt: " fmt

#include <common.h>
#include <init.h>
#include <watchdog.h>
#include <efi.h>
#include <efi/pci.h>
#include <efi/efi.h>
#include <efi/efi-device.h>
#include <linux/pci.h>

#define ACPIBASE		0x40
#define ACPICTRL_PMCBASE	0x44

#define PMBASE_ADDRMASK		0x0000ff80
#define PMCBASE_ADDRMASK	0xfffffe00

#define ACPIBASE_SMI_OFF	0x30
#define ACPIBASE_SMI_END	0x33
#define ACPIBASE_PMC_OFF	0x08
#define ACPIBASE_PMC_END	0x0c
#define ACPIBASE_TCO_OFF	0x60
#define ACPIBASE_TCO_END	0x7f

#define ACPIBASE_GCS_OFF	0x3410

#define RCBABASE		0xf0

#define SMI_TCO_MASK		(1 << 13)

#define TCO_TMR_HLT_MASK	(1 << 11)

/* SMI Control and Enable Register */
#define SMI_EN(itco)	((itco)->smibase)

#define TCO_RLD		0x00 /* TCO Timer Reload/Curr. Value */
#define TCOv1_TMR	0x01 /* TCOv1 Timer Initial Value*/
#define TCO_DAT_IN	0x02 /* TCO Data In Register	*/
#define TCO_DAT_OUT	0x03 /* TCO Data Out Register	*/
#define TCO1_STS	0x04 /* TCO1 Status Register	*/
#define TCO2_STS	0x06 /* TCO2 Status Register	*/
#define TCO1_CNT	0x08 /* TCO1 Control Register	*/
#define TCO2_CNT	0x0a /* TCO2 Control Register	*/
#define TCOv2_TMR	0x12 /* TCOv2 Timer Initial Value*/

#define PMC_NO_REBOOT_MASK	(1 << 4)

#define PCI_ID_ITCO_INTEL_ICH9		0x2918
#define PCI_ID_ITCO_INTEL_BAYTRAIL	0x0f1c

struct itco_priv;

struct itco_info {
	u32 pci_id;
	const char *name;
	u32 pmbase;
	int (*update_no_reboot_bit)(struct itco_priv *itco, bool set);
	unsigned version;
};

struct itco_priv {
	struct watchdog wdd;
	struct efi_pci_io_protocol *protocol;
	void __iomem *io;
	u32 smibase;
	u32 tcobase;
	void __iomem *gcs_pmc;
	struct itco_info *info;
	unsigned timeout;
};

static inline u32 itco_read(struct itco_priv *itco,
			    struct efi_pci_io_protocol_access *access,
			    enum efi_pci_protocol_width width,
			    u64 offset)
{
	u32 val;
	efi_status_t efiret = access->read(itco->protocol, width,
					   EFI_PCI_IO_PASS_THROUGH_BAR,
					   offset, 1, &val);
	if (EFI_ERROR(efiret))
		pr_err("EFI PCI read @%llx failed: %s\n",
		       offset, efi_strerror(efiret));
	return val;
}

static inline void itco_write(struct itco_priv *itco,
			      struct efi_pci_io_protocol_access *access,
			      enum efi_pci_protocol_width width,
			      u64 offset, u32 val)
{
	efi_status_t efiret = access->write(itco->protocol, width,
					    EFI_PCI_IO_PASS_THROUGH_BAR,
					    offset, 1, &val);
	if (EFI_ERROR(efiret))
		pr_err("EFI PCI write @%llx failed: %s\n",
		       offset, efi_strerror(efiret));
}

static inline void itco_smi_outl(struct itco_priv *itco, u32 val)
{
	itco_write(itco, &itco->protocol->io, EFI_PCI_WIDTH_U32,
		   itco->smibase, val);
}

static inline u32 itco_smi_inl(struct itco_priv *itco)
{
	return itco_read(itco, &itco->protocol->io, EFI_PCI_WIDTH_U32,
			 itco->smibase);
}

static inline void itco_outl(struct itco_priv *itco, u32 val, u16 port)
{
	itco_write(itco, &itco->protocol->io, EFI_PCI_WIDTH_U32,
		   itco->tcobase + port, val);
}

static inline void itco_outw(struct itco_priv *itco, u16 val, u16 port)
{
	itco_write(itco, &itco->protocol->io, EFI_PCI_WIDTH_U16,
		   itco->tcobase + port, val);
}

static inline u16 itco_inw(struct itco_priv *itco, u16 port)
{
	return itco_read(itco, &itco->protocol->io, EFI_PCI_WIDTH_U16,
			 itco->tcobase + port);
}

static inline void itco_outb(struct itco_priv *itco, u16 val, u16 port)
{
	itco_write(itco, &itco->protocol->io, EFI_PCI_WIDTH_U8,
		   itco->tcobase + port, val);
}

static inline u8 itco_inb(struct itco_priv *itco, u16 port)
{
	return itco_read(itco, &itco->protocol->io, EFI_PCI_WIDTH_U8,
			 itco->tcobase + port);
}

static inline void itco_writel(struct itco_priv *itco, u32 val, void __iomem *addr)
{
	itco_write(itco, &itco->protocol->mem, EFI_PCI_WIDTH_U32,
		   (unsigned long)addr, val);
}

static inline u32 itco_readl(struct itco_priv *itco, void __iomem *addr)
{
	return itco_read(itco, &itco->protocol->mem, EFI_PCI_WIDTH_U32,
			 (unsigned long)addr);
}

static inline void itco_pci_read_config_byte(struct itco_priv *itco, int where, u8 *val)
{
	u32 _val;
	efi_status_t efiret = itco->protocol->pci.read(itco->protocol,
						       EFI_PCI_WIDTH_U8,
						       where,
						       1,
						       &_val);
	if (EFI_ERROR(efiret))
		pr_err("EFI PCII read @%x failed: %s\n",
		       where, efi_strerror(efiret));
	*val = _val;
}

static inline void itco_pci_write_config_byte(struct itco_priv *itco, int where, u8 val)
{
	u32 _val = val;
	efi_status_t efiret = itco->protocol->pci.write(itco->protocol,
						       EFI_PCI_WIDTH_U8,
						       where,
						       1,
						       &_val);
	if (EFI_ERROR(efiret))
		pr_err("EFI PCII write @%x failed: %s\n",
		       where, efi_strerror(efiret));
}

static inline void itco_pci_read_config_dword(struct itco_priv *itco, int where, u32 *val)
{
	u32 _val;
	efi_status_t efiret = itco->protocol->pci.read(itco->protocol,
						       EFI_PCI_WIDTH_U32,
						       where,
						       1,
						       &_val);
	if (EFI_ERROR(efiret))
		pr_err("EFI PCII read @%x failed: %s\n",
		       where, efi_strerror(efiret));
	*val = _val;
}

static u32 itco_get_pmbase(struct itco_priv *itco)
{
	u32 pmbase = itco->info->pmbase;

	if (!pmbase)
		itco_pci_read_config_dword(itco, ACPIBASE, &pmbase);

	return pmbase & PMBASE_ADDRMASK;
}

static inline struct itco_priv *to_itco_priv(struct watchdog *wdd)
{
	return container_of(wdd, struct itco_priv, wdd);
}

static void itco_wdt_ping(struct itco_priv *itco)
{
	/* Reload the timer by writing to the TCO Timer Counter register */
	if (itco->info->version >= 2) {
		itco_outw(itco, 0x01, TCO_RLD);
	} else if (itco->info->version == 1) {
		/* Reset the timeout status bit so that the timer
		 * needs to count down twice again before rebooting */
		itco_outw(itco, 0x0008, TCO1_STS);	/* write 1 to clear bit */

		itco_outb(itco, 0x01, TCO_RLD);
	}
}

static inline unsigned int seconds_to_ticks(struct itco_priv *itco, int secs)
{
	return itco->info->version == 3 ? secs : (secs * 10) / 6;
}

static inline unsigned int ticks_to_seconds(struct itco_priv *itco, int ticks)
{
	return itco->info->version == 3 ? ticks : (ticks * 6) / 10;
}


static int itco_wdt_start(struct itco_priv *itco, unsigned int timeout)
{
	unsigned tmrval;
	u32 value;
	int ret;

	tmrval = seconds_to_ticks(itco, timeout);

	/* from the specs: */
	/* "Values of 0h-3h are ignored and should not be attempted" */
	if (tmrval < 0x04)
		return -EINVAL;

	if (tmrval > 0x3ff)
		return -EINVAL;

	/* Clear NO_REBOOT flag */
	if (itco->info->update_no_reboot_bit) {
		ret = itco->info->update_no_reboot_bit(itco, false);
		if (ret)
			return ret;
	}

	/* Enable TCO SMIs */
	value = itco_smi_inl(itco) | SMI_TCO_MASK;
	itco_smi_outl(itco, value);

	/* Set timer value */
	value = itco_inw(itco, TCOv2_TMR);

	value &= 0xfc00;
	value |= tmrval & 0x3ff;

	itco_outw(itco, value, TCOv2_TMR);
	value = itco_inw(itco, TCOv2_TMR);

	if ((value & 0x3ff) != tmrval)
		return -EINVAL;

	/* Force reloading of timer value */
	if (itco->info->version >= 2)
		itco_outw(itco, 0x01, TCO_RLD);
	else if (itco->info->version == 1)
		itco_outb(itco, 0x01, TCO_RLD);

	/* Clear HLT flag to start timer */
	value = itco_inw(itco, TCO1_CNT) & ~TCO_TMR_HLT_MASK;
	itco_outw(itco, value, TCO1_CNT);
	value = itco_inw(itco, TCO1_CNT);

	if (value & 0x0800)
		return -EIO;

	return 0;
}

static int itco_wdt_stop(struct itco_priv *itco)
{
	u32 val;

	/* Bit 11: TCO Timer Halt -> 1 = The TCO timer is disabled */
	val = itco_inw(itco, TCO1_CNT) | 0x0800;
	itco_outw(itco, val, TCO1_CNT);
	val = itco_inb(itco, TCO1_CNT);

	/* Set the NO_REBOOT bit to prevent later reboots, just for sure */
	itco->info->update_no_reboot_bit(itco, true);

	if ((val & 0x0800) == 0)
		return -EIO;
	return 0;
}

static int itco_wdt_set_timeout(struct watchdog *wdd, unsigned int timeout)
{
	struct itco_priv *itco = to_itco_priv(wdd);
	int ret;

	if (!timeout)
		return itco_wdt_stop(itco);

	if (timeout > wdd->timeout_max)
		return -EINVAL;

	if (itco->timeout != timeout) {
		ret = itco_wdt_start(itco, timeout);
		if (ret) {
			dev_err(wdd->hwdev, "Fail to (re)start watchdog\n");
			return ret;
		}
	}

	itco_wdt_ping(itco);
	return 0;
}

static inline u32 no_reboot_bit(unsigned version)
{
	switch (version) {
	case 5:
	case 3:
		return 0x00000010;
	case 2:
		return 0x00000020;
	case 4:
	case 1:
	default:
		return 0x00000002;
	}
}

static int update_no_reboot_bit_mem(struct itco_priv *itco, bool set)
{
	u32 val32 = 0, newval32 = 0;

	val32 = itco_readl(itco, itco->gcs_pmc);
	if (set)
		val32 |= no_reboot_bit(itco->info->version);
	else
		val32 &= ~no_reboot_bit(itco->info->version);
	itco_writel(itco, val32, itco->gcs_pmc);
	newval32 = itco_readl(itco, itco->gcs_pmc);

	/* make sure the update is successful */
	if (val32 != newval32)
		return -EPERM;

	return 0;
}

static void lpc_ich_enable_acpi_space(struct itco_priv *itco)
{
	u8 reg;

	switch (itco->info->version) {
	case 3:
		/*
		 * Some chipsets (eg Avoton) enable the ACPI space in the
		 * ACPI BASE register.
		 */
		itco_pci_read_config_byte(itco, ACPIBASE, &reg);
		itco_pci_write_config_byte(itco, ACPIBASE, reg | 0x02);
		break;
	default:
		/*
		 * Most chipsets enable the ACPI space in the ACPI control
		 * register.
		 */
		itco_pci_read_config_byte(itco, ACPICTRL_PMCBASE, &reg);
		itco_pci_write_config_byte(itco, ACPICTRL_PMCBASE, reg | 0x80);
		break;
	}
}

static void lpc_ich_enable_pmc_space(struct itco_priv *itco)
{
        u8 reg_save;

        itco_pci_read_config_byte(itco, ACPICTRL_PMCBASE, &reg_save);
        itco_pci_write_config_byte(itco, ACPICTRL_PMCBASE, reg_save | 0x2);
}

enum itco_chipsets {
	ITCO_INTEL_ICH9,
	ITCO_INTEL_BAYTRAIL,
};

/* XXX version 1 not supported! */
static struct itco_info itco_chipset_info[] = {
	[ITCO_INTEL_ICH9] = {
		.pci_id = PCI_ID_ITCO_INTEL_ICH9,
		.name = "ICH9", /* QEmu machine q35 */
		.update_no_reboot_bit = update_no_reboot_bit_mem,
		.version = 2,
	},
	[ITCO_INTEL_BAYTRAIL] =
	{
		.pci_id = PCI_ID_ITCO_INTEL_BAYTRAIL,
		.name = "Bay Trail",
		.update_no_reboot_bit = update_no_reboot_bit_mem,
		.version = 3,
	},
};

static DEFINE_PCI_DEVICE_TABLE(itco_wdt_pci_tbl) = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_ID_ITCO_INTEL_ICH9),
		.driver_data = (unsigned long)&itco_chipset_info[ITCO_INTEL_ICH9]
	},
	{
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_ID_ITCO_INTEL_BAYTRAIL),
		.driver_data = (unsigned long)&itco_chipset_info[ITCO_INTEL_BAYTRAIL]
	},
	{ /* sentinel */ },
};

static int efi_itco_wdt_probe(struct efi_device *efidev)
{
	struct itco_priv *itco;
	struct watchdog *wdd;
	u32 gcs_pmc;
	u32 pmbase;
	int ret;
	const struct pci_device_id *id;
	efi_status_t efiret;
	struct efi_pci_io_protocol *protocol;

	efiret = BS->open_protocol(efidev->handle, &EFI_PCI_IO_PROTOCOL_GUID,
			    (void **)&protocol, efi_parent_image,
			    NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(efiret))
		return -ENODEV;

	for (id = itco_wdt_pci_tbl; id->vendor; id++) {
		u32 vendor_id = 0, device_id = 0;

		protocol->pci.read(protocol, EFI_PCI_WIDTH_U16, PCI_VENDOR_ID, 1, &vendor_id);
		protocol->pci.read(protocol, EFI_PCI_WIDTH_U16, PCI_DEVICE_ID, 1, &device_id);

		if (id->vendor == vendor_id && id->device == device_id)
			break;

		dev_dbg(&efidev->dev, "Skipping PCI device %04x:%04x\n",
			vendor_id, device_id);
	}

	if (!id->vendor) {
		BS->close_protocol(efidev->handle, &EFI_PCI_IO_PROTOCOL_GUID,
				   efi_parent_image, NULL);
		return -ENODEV;
	}

	itco = xzalloc(sizeof(*itco));

	itco->protocol = protocol;
	itco->info = (void*)id->driver_data;

	pmbase = itco_get_pmbase(itco);
	if (!pmbase) {
		dev_notice(&efidev->dev, "I/O space for ACPI uninitialized\n");
		return -ENODEV;
	}

	itco->smibase = pmbase + ACPIBASE_SMI_OFF;
	itco->tcobase = pmbase + ACPIBASE_TCO_OFF;

	lpc_ich_enable_acpi_space(itco);

	switch (itco->info->version) {
	case 1:
		itco->gcs_pmc = (void __iomem *)NULL;
		break;
	case 2:
		itco_pci_read_config_dword(itco, RCBABASE, &gcs_pmc);
		if (!(gcs_pmc & 1)) {
			dev_notice(&efidev->dev, "RCBA is disabled by hardware/BIOS, device disabled\n");
			return -ENODEV;
		}
		gcs_pmc &= 0xffffc000;
		itco->gcs_pmc = IOMEM((unsigned long)gcs_pmc) + ACPIBASE_GCS_OFF;
		break;
	case 3:
		lpc_ich_enable_pmc_space(itco);
		itco_pci_read_config_dword(itco, ACPICTRL_PMCBASE, &gcs_pmc);

		itco->gcs_pmc = IOMEM(gcs_pmc & 0xfffffe00UL) + ACPIBASE_PMC_OFF;
		break;
	}

	dev_dbg(&efidev->dev, "gcs_pmc = 0x%p, smibase = 0x%x, tcobase = 0x%x\n",
		itco->gcs_pmc, itco->smibase, itco->tcobase);

	wdd = &itco->wdd;
	wdd->hwdev = &efidev->dev;
	wdd->set_timeout = itco_wdt_set_timeout;

	wdd->timeout_max = 614;

	switch (itco->info->version) {
	case 4 ... 6:
		itco_outw(itco, 0x0008, TCO1_STS); /* Clear the Time Out Status bit */
		itco_outw(itco, 0x0002, TCO2_STS); /* Clear SECOND_TO_STS bit */
		break;
	case 3:
		itco_outl(itco, 0x20008, TCO1_STS);
		break;
	case 1 ... 2:
	default:
		itco_outw(itco, 0x0008, TCO1_STS); /* Clear the Time Out Status bit */
		itco_outw(itco, 0x0002, TCO2_STS); /* Clear SECOND_TO_STS bit */
		itco_outw(itco, 0x0004, TCO2_STS); /* Clear BOOT_STS bit */
		break;
	}

	ret = watchdog_register(wdd);
	if (ret) {
		dev_err(&efidev->dev, "Failed to register watchdog device\n");
		return ret;
	}

	dev_info(&efidev->dev, "probed Intel TCO %s watchdog\n", itco->info->name);

	return 0;
}

static struct efi_driver efi_itco_wdt_driver = {
        .driver = {
		.name  = "efi-itco-wdt",
	},
        .probe = efi_itco_wdt_probe,
	.guid = EFI_PCI_IO_PROTOCOL_GUID,
};
device_efi_driver(efi_itco_wdt_driver);
