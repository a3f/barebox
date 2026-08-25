// SPDX-License-Identifier: GPL-2.0-only
/*
 * RISC-V specific bits of barebox running as EFI payload
 */
#define pr_fmt(fmt) "efi-riscv: " fmt

#include <common.h>
#include <init.h>
#include <efi/payload.h>
#include <efi/error.h>
#include <efi/guid.h>
#include <efi/mode.h>
#include <efi/protocol/riscv-boot.h>
#include <asm/system.h>

/*
 * barebox expects the ID of the hart it's running on in the tp register.
 * It's normally passed to the entry point in a0 by the previous boot stage,
 * but an EFI application needs to ask the firmware instead.
 */
static int riscv_efi_boot_hartid(void)
{
	struct riscv_efi_boot_protocol *proto;
	unsigned long hartid;
	efi_status_t efiret;

	if (!efi_is_payload())
		return 0;

	efiret = BS->locate_protocol(&efi_riscv_boot_protocol_guid, NULL,
				     (void **)&proto);
	if (EFI_ERROR(efiret)) {
		pr_warn("RISC-V boot protocol unavailable: %s\n",
			efi_strerror(efiret));
		return 0;
	}

	efiret = proto->get_boot_hartid(proto, &hartid);
	if (EFI_ERROR(efiret)) {
		pr_warn("failed to determine boot hartid: %s\n",
			efi_strerror(efiret));
		return 0;
	}

	pr_debug("running on hart %lu\n", hartid);

	__asm__ volatile("mv tp, %0\n" : : "r"(hartid));

	return 0;
}
core_initcall(riscv_efi_boot_hartid);
