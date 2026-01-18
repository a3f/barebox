// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2025 Ahmad Fatoum

#define pr_fmt(fmt) "bootm-mock: " fmt

#include <common.h>
#include <bootm.h>
#include <filetype.h>
#include <fs.h>
#include <fcntl.h>
#include <init.h>
#include <globalvar.h>
#include <magicvar.h>
#include <libfile.h>
#include <linux/string_choices.h>
#include <memory.h>
#include <of.h>
#include <bobject.h>
#include <param.h>
#include <xfuncs.h>

#define LASTBOOT_DIR "/tmp/lastboot"

static int bootm_mock_enabled;

static int write_region_to_file(const char *filename, struct resource *res)
{
	if (!res)
		return 0;

	return write_file(filename, (void *)(ulong)res->start, resource_size(res));
}

static char *format_loadable_json(struct loadable *loadable)
{
	struct loadable_info info;
	char *buf;

	if (!loadable || !loadable->ops || !loadable->name)
		return xstrdup("null");

	buf = xasprintf("{\n"
			"      \"name\": \"%s\"",
			loadable->name);

	if (!loadable_get_info(loadable, &info) &&
	    info.final_size != LOADABLE_SIZE_UNKNOWN)
		buf = xrasprintf(buf,
				 ",\n"
				 "      \"size\": %zu\n",
				 info.final_size);

	return xrasprintf(buf, "}");
}

static char *format_section_json(const char *name, struct bobject *bobj,
				 const char *loadable_json)
{
	char *params_json, *result;

	params_json = bobject_format_json_params(bobj);

	/* Remove closing brace from params, add loadable field, then close */
	params_json[strlen(params_json) - 2] = '\0'; /* Remove " }" */

	result = basprintf("  \"%s\": %s,\n"
			   "    \"loadable\": %s\n"
			   "  }",
			   name, params_json, loadable_json);
	free(params_json);
	return result;
}

static char *format_section_json_with_loadables(const char *name,
						struct bobject *bobj,
						const char *loadables_json)
{
	char *params_json, *result;

	params_json = bobject_format_json_params(bobj);

	/* Remove closing brace from params, add loadables field, then close */
	params_json[strlen(params_json) - 2] = '\0'; /* Remove " }" */

	result = basprintf("  \"%s\": %s,\n"
			   "    \"loadables\": %s\n"
			   "  }",
			   name, params_json, loadables_json);
	free(params_json);
	return result;
}

static int write_manifest_json(struct image_data *data)
{
	struct bobject *os_bobj, *initrd_bobj, *oftree_bobj, *top_bobj;
	char *json;
	char *os_json = NULL, *initrd_json = NULL, *oftree_json = NULL,
	     *top_json;
	char *loadable_json;
	int ret;

	/* Populate OS parameters */
	if (data->os) {
		os_bobj = bobject_alloc("os");
		os_bobj->local = true;

		if (data->os_file)
			bobject_add_param_string_fixed(os_bobj, "file",
						       data->os_file);
		if (data->os_part)
			bobject_add_param_string_fixed(os_bobj, "part",
						       data->os_part);
		if (UIMAGE_IS_ADDRESS_VALID(data->os_address))
			bobject_add_param_fixed(os_bobj, "address", "0x%lx",
						data->os_address);
		if (UIMAGE_IS_ADDRESS_VALID(data->os_entry))
			bobject_add_param_fixed(os_bobj, "entry", "0x%lx",
						data->os_entry);
		if (UIMAGE_IS_ADDRESS_VALID(data->os_address_hint))
			bobject_add_param_fixed(os_bobj, "address_hint",
						"0x%lx", data->os_address_hint);
		if (UIMAGE_IS_ADDRESS_VALID(data->os_entry_hint))
			bobject_add_param_fixed(os_bobj, "entry_hint", "0x%lx",
						data->os_entry_hint);
		bobject_add_param_string_fixed(
			os_bobj, "image-type",
			file_type_to_string(data->image_type));
		bobject_add_param_string_fixed(
			os_bobj, "kernel-type",
			file_type_to_string(data->kernel_type));
		bobject_add_param_string_fixed(
			os_bobj, "is_override",
			str_true_false(data->is_override.os));

		loadable_json = format_loadable_json(data->os);
		os_json = format_section_json("os", os_bobj, loadable_json);
		free(loadable_json);

		bobject_free(os_bobj);
	}

	/* Populate initrd parameters */
	if (data->initrd) {
		struct loadable *l;
		char *loadables_array_json = NULL;
		char *single_loadable_json;

		initrd_bobj = bobject_alloc("initrd");
		initrd_bobj->local = true;

		if (data->initrd_file)
			bobject_add_param_string_fixed(initrd_bobj, "file",
						       data->initrd_file);
		if (data->initrd_part)
			bobject_add_param_string_fixed(initrd_bobj, "part",
						       data->initrd_part);
		if (UIMAGE_IS_ADDRESS_VALID(data->initrd_address))
			bobject_add_param_fixed(initrd_bobj, "address", "0x%lx",
						data->initrd_address);
		bobject_add_param_string_fixed(
			initrd_bobj, "is_override",
			str_true_false(data->is_override.initrd));

		/* Build array of loadables */
		loadables_array_json = xstrdup("[\n");

		single_loadable_json = format_loadable_json(data->initrd);
		loadables_array_json = xrasprintf(
			loadables_array_json, "      %s", single_loadable_json);
		free(single_loadable_json);

		list_for_each_entry(l, &data->initrd->chained_loadables, list) {
			loadables_array_json =
				xrasprintf(loadables_array_json, ",\n");

			single_loadable_json = format_loadable_json(l);
			loadables_array_json = xrasprintf(loadables_array_json,
							  "      %s",
							  single_loadable_json);
			free(single_loadable_json);
		}
		loadables_array_json =
			xrasprintf(loadables_array_json, "\n    ]");

		initrd_json = format_section_json_with_loadables(
			"initrd", initrd_bobj, loadables_array_json);
		free(loadables_array_json);

		bobject_free(initrd_bobj);
	}

	/* Populate oftree parameters */
	if (data->oftree) {
		oftree_bobj = bobject_alloc("oftree");
		oftree_bobj->local = true;

		if (data->oftree_file)
			bobject_add_param_string_fixed(oftree_bobj, "file",
						       data->oftree_file);
		if (data->oftree_part)
			bobject_add_param_string_fixed(oftree_bobj, "part",
						       data->oftree_part);
		bobject_add_param_string_fixed(
			oftree_bobj, "is_override",
			str_true_false(data->is_override.oftree));

		loadable_json = format_loadable_json(data->oftree);
		oftree_json = format_section_json("oftree", oftree_bobj,
						  loadable_json);
		free(loadable_json);

		bobject_free(oftree_bobj);
	}

	/* Populate top-level parameters */
	top_bobj = bobject_alloc("top");
	top_bobj->local = true;

	if (bootm_verify_tostr(data->verify))
		bobject_add_param_string_fixed(
			top_bobj, "verify", bootm_verify_tostr(data->verify));
	else
		bobject_add_param_uint32_fixed(top_bobj, "verify", data->verify,
					       "%u");

	if (bootm_efi_loader_mode_tostr(data->efi_boot))
		bobject_add_param_string_fixed(
			top_bobj, "efi_boot",
			bootm_efi_loader_mode_tostr(data->efi_boot));
	else
		bobject_add_param_uint32_fixed(top_bobj, "efi_boot",
					       data->efi_boot, "%u");

	bobject_add_param_string_fixed(top_bobj, "verbose",
				       str_true_false(data->verbose));
	bobject_add_param_string_fixed(top_bobj, "force",
				       str_true_false(data->force));
	bobject_add_param_string_fixed(top_bobj, "dryrun",
				       str_true_false(data->dryrun));

	top_json = bobject_format_json_params(top_bobj);

	/* Build final JSON using xrasprintf to avoid memory leaks */
	json = xasprintf("{\n");
	json = xrasprintf(json, "%s,\n", os_json);
	if (initrd_json)
		json = xrasprintf(json, "%s,\n", initrd_json);
	if (oftree_json)
		json = xrasprintf(json, "%s,\n", oftree_json);
	if (data->tee_file)
		json = xrasprintf(json, "  \"tee\": { \"file\": \"%s\" },\n",
				  data->tee_file);
	json = xrasprintf(json, "%s\n", top_json + 1); /* Skip "{" */

	ret = write_file(LASTBOOT_DIR "/manifest.json", json, strlen(json));

	/* Cleanup */
	bobject_free(top_bobj);
	free(json);
	free(os_json);
	free(initrd_json);
	free(oftree_json);
	free(top_json);

	if (ret)
		pr_err("Failed to write manifest.json: %pe\n", ERR_PTR(ret));

	return ret;
}

static int do_bootm_mock(struct image_data *data)
{
	const struct resource *os_res, *initrd_res;
	resource_size_t start, end;
	unsigned long load_address;
	void *fdt;
	int ret;

	ret = memory_bank_first_find_space(&start, &end);
	if (ret)
		return ret;

	load_address = PAGE_ALIGN(start);

	os_res = bootm_load_os(data, load_address, end);
	if (IS_ERR(os_res))
		return PTR_ERR(os_res);

	initrd_res = bootm_load_initrd(data, PAGE_ALIGN(os_res->end + 1), end);
	if (IS_ERR(initrd_res))
		return PTR_ERR(initrd_res);

	fdt = bootm_get_devicetree(data);
	if (IS_ERR(fdt))
		return PTR_ERR(fdt);

	if (data->dryrun) {
		free(fdt);
		return 0;
	}

	ret = make_directory(LASTBOOT_DIR);
	if (ret) {
		pr_err("Failed to create %s: %pe\n", LASTBOOT_DIR,
		       ERR_PTR(ret));
		free(fdt);
		return ret;
	}

	ret = write_region_to_file(LASTBOOT_DIR "/image", data->os_res);
	if (ret) {
		pr_err("Failed to write image: %pe\n", ERR_PTR(ret));
		goto out;
	}

	if (fdt) {
		size_t fdt_size =
			be32_to_cpu(((struct fdt_header *)fdt)->totalsize);

		ret = write_file(LASTBOOT_DIR "/oftree", fdt, fdt_size);
		if (ret) {
			pr_err("Failed to write oftree: %pe\n", ERR_PTR(ret));
			goto out;
		}
	}

	ret = write_region_to_file(LASTBOOT_DIR "/initrd", data->initrd_res);
	if (ret) {
		pr_err("Failed to write initrd: %pe\n", ERR_PTR(ret));
		goto out;
	}

	ret = write_manifest_json(data);
	if (ret)
		goto out;

	pr_info("Boot data written to %s\n", LASTBOOT_DIR);

out:
	free(fdt);
	return ret;
}

static bool bootm_mock_check_image(struct image_handler *handler,
				   struct image_data *data,
				   enum filetype detected_filetype)
{
	return bootm_mock_enabled;
}

static struct image_handler bootm_mock_handler = {
	.name = "Mock image handler",
	.bootm = do_bootm_mock,
	.check_image = bootm_mock_check_image,
};

static int bootm_mock_init(void)
{
	globalvar_add_simple_bool("bootm.mock", &bootm_mock_enabled);
	register_image_handler_head(&bootm_mock_handler);
	return 0;
}
late_initcall(bootm_mock_init);

BAREBOX_MAGICVAR(global.bootm.mock, "Enable mock image handler that writes boot data to /tmp/lastboot/");
