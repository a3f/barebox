/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __EFI_PROTOCOL_RISCV_BOOT_H_
#define __EFI_PROTOCOL_RISCV_BOOT_H_

#include <efi/types.h>

#define RISCV_EFI_BOOT_PROTOCOL_REVISION	0x00010000

/*
 * RISC-V UEFI Boot Protocol: allows an EFI application to determine
 * the ID of the hart it's executing on.
 */
struct riscv_efi_boot_protocol {
	u64 revision;
	efi_status_t (EFIAPI *get_boot_hartid)(struct riscv_efi_boot_protocol *this,
					       unsigned long *boot_hartid);
};

#endif
