// SPDX-License-Identifier: GPL-2.0-only
/*
 * efi-initrd.c - barebox EFI payload support
 *
 * Copyright (c) 2025 Anis Chali <chalianis1@gmail.com>
 */
#include <common.h>
#include <driver.h>
#include <init.h>
#include <linux/hw_random.h>
#include <efi.h>
#include <efi/efi-device.h>
#include <efi/device-path.h>
#include <efi/efi-payload.h>

static efi_status_t EFIAPI efi_initrd_load_file2(
	struct efi_load_file_protocol *this, struct efi_device_path *file_path,
	bool boot_policy, unsigned long *buffer_size, void *buffer);

static const struct {
	struct efi_device_path_vendor vendor;
	struct efi_device_path end;
} __packed initrd_dev_path = { { {
					 DEVICE_PATH_TYPE_MEDIA_DEVICE,
					 DEVICE_PATH_SUB_TYPE_VENDOR_PATH,
					 sizeof(initrd_dev_path.vendor),
				 },
				 EFI_LINUX_INITRD_MEDIA_GUID },
			       { DEVICE_PATH_TYPE_END, DEVICE_PATH_SUB_TYPE_END,
				 DEVICE_PATH_END_LENGTH } };

static struct efi_device_path_memory *initrd_dev;
static efi_handle_t lf2_handle;
static struct efi_load_file_protocol efi_lf2_p = {
	.load_file = efi_initrd_load_file2
};

static efi_status_t EFIAPI efi_initrd_load_file2(
	struct efi_load_file_protocol *this, struct efi_device_path *file_path,
	bool boot_policy, unsigned long *buffer_size, void *buffer)
{
	size_t initrd_size;

	if (!this || this != &efi_lf2_p || !buffer_size)
		return EFI_INVALID_PARAMETER;

	if (file_path->type != initrd_dev_path.end.type ||
	    file_path->sub_type != initrd_dev_path.end.sub_type)
		return EFI_INVALID_PARAMETER;

	if (boot_policy)
		return EFI_UNSUPPORTED;

	initrd_size = initrd_dev->ending_address - initrd_dev->starting_address;
	if (!buffer || *buffer_size < initrd_size) {
		*buffer_size = initrd_size;
		return EFI_BUFFER_TOO_SMALL;
	}

	else {
		memcpy(buffer, (void *)(uintptr_t)initrd_dev->starting_address,
		       initrd_size);
		*buffer_size = initrd_size;
	}

	return EFI_SUCCESS;
}

int efi_initrd_register(void *initrd, size_t initrd_sz)
{
	efi_physical_addr_t mem;
	efi_status_t efiret;
	size_t sz;
	int ret;

	sz = sizeof(struct efi_device_path_memory) +
	     sizeof(struct efi_device_path);
	efiret = BS->allocate_pool(EFI_BOOT_SERVICES_DATA, sz, (void **)&mem);
	if (EFI_ERROR(efiret)) {
		pr_err("Failed to allocate memory for INITRD %s\n",
		       efi_strerror(efiret));
		ret = -efi_errno(efiret);
		return ret;
	}

	initrd_dev = efi_phys_to_virt(mem);
	initrd_dev->header.type = DEVICE_PATH_TYPE_HARDWARE_DEVICE;
	initrd_dev->header.sub_type = DEVICE_PATH_SUB_TYPE_MEMORY;
	initrd_dev->header.length = sizeof(struct efi_device_path_memory);
	initrd_dev->memory_type = EFI_LOADER_DATA;
	initrd_dev->starting_address = efi_virt_to_phys(initrd);
	initrd_dev->ending_address = efi_virt_to_phys(initrd) + initrd_sz;

	memcpy(&initrd_dev[1], &initrd_dev_path.end, DEVICE_PATH_END_LENGTH);

	efiret = BS->install_multiple_protocol_interfaces(
		&lf2_handle, &efi_load_file2_protocol_guid, &efi_lf2_p,
		&efi_device_path_protocol_guid, &initrd_dev_path, NULL);
	if (EFI_ERROR(efiret)) {
		pr_err("Failed to install protocols for INITRD %s\n",
		       efi_strerror(efiret));
		ret = -efi_errno(efiret);
		goto out;
	}

	return 0;
out:
	BS->free_pool(efi_phys_to_virt(mem));
	return ret;
}

void efi_initrd_unregister(void)
{
	if (!initrd_dev)
		return;

	BS->uninstall_multiple_protocol_interfaces(
		lf2_handle, &efi_device_path_protocol_guid, &initrd_dev_path,
		&efi_load_file2_protocol_guid, &efi_lf2_p, NULL);

	BS->free_pool(initrd_dev);
	initrd_dev = NULL;
}
