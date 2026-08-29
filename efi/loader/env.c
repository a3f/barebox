// SPDX-License-Identifier: GPL-2.0-only
/*
 * env.c - hand the barebox environment over to a barebox EFI payload
 */

#define pr_fmt(fmt) "efi-loader: env: " fmt

#include <efi/types.h>
#include <efi/error.h>
#include <efi/guid.h>
#include <efi/loader.h>
#include <efi/runtime.h>
#include <efi/mode.h>
#include <efi/variable.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <envfs.h>
#include <fs.h>
#include <init.h>
#include <libfile.h>
#include <magicvar.h>
#include <malloc.h>
#include <param.h>
#include <xfuncs.h>

#define EFI_ENV_TMPFILE	"/tmp/.efi-env.tmp"

static uint32_t efi_env_export;

/**
 * efi_export_env() - publish the barebox environment for a barebox payload
 *
 * A barebox EFI payload reads its environment out of the barebox-env EFI
 * variable, which nothing creates when barebox is itself the firmware.
 * Store the environment we are running with, so the payload starts with the
 * same configuration.
 *
 * Return:	status code
 */
static efi_status_t efi_export_env(void *data)
{
	efi_status_t efiret;
	size_t size;
	void *buf;
	u16 *name;
	int ret;

	if (!IS_ENABLED(CONFIG_ENV_HANDLING) || !efi_env_export)
		return EFI_SUCCESS;

	/* the payload merges this into a default environment of its own */
	ret = envfs_save(EFI_ENV_TMPFILE, "/env", 0, true);
	if (ret) {
		pr_warn("Failed saving environment: %pe\n", ERR_PTR(ret));
		return EFI_SUCCESS;
	}

	buf = read_file(EFI_ENV_TMPFILE, &size);
	unlink(EFI_ENV_TMPFILE);
	if (!buf) {
		pr_warn("Failed reading back environment: %m\n");
		return EFI_SUCCESS;
	}

	name = xstrdup_char_to_wchar(EFI_BAREBOX_ENV_VAR_NAME);

	/*
	 * A payload that saved its environment left a non-volatile variable
	 * behind and SetVariable() refuses to change a variable's attributes,
	 * so remove it first: what the loader hands over takes precedence.
	 */
	efi_set_variable_int(name, &efi_barebox_vendor_guid, 0, 0, NULL, false);

	efiret = efi_set_variable_int(name, &efi_barebox_vendor_guid,
				      EFI_VARIABLE_BOOTSERVICE_ACCESS |
				      EFI_VARIABLE_RUNTIME_ACCESS,
				      size, buf, false);
	if (efiret != EFI_SUCCESS)
		pr_warn("Failed exporting %zu byte environment: %pe\n",
			size, ERR_PTR(-efi_errno(efiret)));

	free(name);
	free(buf);

	/* An environment the payload can do without is not worth failing on */
	return EFI_SUCCESS;
}

static int efi_env_export_set(struct param_d *param, void *priv)
{
	/*
	 * The environment is exported when the EFI subsystem is initialized,
	 * which is on the way to starting an image. Should that have happened
	 * already, export it now.
	 */
	if (efi_is_loader())
		efi_export_env(NULL);

	return 0;
}

static int efi_env_init(void)
{
	if (efi_is_payload() || !IS_ENABLED(CONFIG_ENV_HANDLING))
		return 0;

	dev_add_param_bool(&efidev, "env.export", efi_env_export_set, NULL,
			   &efi_env_export, NULL);

	efi_register_deferred_init(efi_export_env, NULL);

	return 0;
}
late_initcall(efi_env_init);

BAREBOX_MAGICVAR(efi.env.export,
		 "efiloader: hand the barebox environment to a barebox EFI payload");
