// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: © 2025 Ahmad Fatoum

#include <common.h>
#include <command.h>
#include <getopt.h>
#include <video/mipi_dsi.h>
#include <video/mipi_display.h>

static u8 val[2];

static int mipi_dsi_command_show(struct mipi_dsi_device *dsi, int cmd, bool prefix)
{
	int ret;

	ret = mipi_dsi_dcs_read(dsi, cmd, &val,
				sizeof(val));
	if (ret < 0) {
		printf("DCS read for command 0x%02x failed: %pe\n",
		       cmd, ERR_PTR(ret));
		return ret;
	}

	if (prefix)
		printf("%d: ", cmd);
	printf("%*phN\n", (int)ret, val);

	return 0;
}

static int do_mipi_dsi(int argc, char *argv[])
{
	struct mipi_dsi_device *dsi;
	int opt, ret, i;
	bool write = false, info = false;
	u8 cmd, val[4];
	struct device *dev;

	dev = list_first_entry_or_null(&mipi_dsi_bus_type.device_list,
				       struct device, bus_list);

	while ((opt = getopt(argc, argv, "iwld:")) > 0) {
		struct device *tmp;
		switch (opt) {
		case 'i':
			info = true;
			break;
		case 'w':
			write = true;
			break;
		case 'l':
			bus_for_each_device(&mipi_dsi_bus_type, tmp)
				printf("%s\n", to_mipi_dsi_device(tmp)->name);
			return 0;
		case 'd':
			dev = bus_find_device(&mipi_dsi_bus_type, NULL,
					      optarg, device_match_name);
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	if (!dev)
		return -ENODEV;

	dsi = to_mipi_dsi_device(dev);
	if (!dsi->attached)
		return -EIO;

	mipi_dsi_set_maximum_return_packet_size(dsi, sizeof(val));

	if (info) {
		if (write || optind != argc)
			return COMMAND_ERROR_USAGE;
		return mipi_dsi_report_panel_id(dsi);
	}

	if (optind == argc) {
		for (cmd = 0; cmd < 255; cmd++) {
			mipi_dsi_command_show(dsi, cmd, true);
			if (ctrlc())
				return -EINTR;
		}
		return 0;
	}

	ret = kstrtou8(argv[optind++], 16, &cmd);
	if (ret < 0)
		return ret;

	if (optind == argc && !write)
		return mipi_dsi_command_show(dsi, cmd, false);

	if (argc > 6) {
		printf("Error: can only write up to 4 byte at once!\n");
		return -EOVERFLOW;
	}

	for (i = 0; i + optind < argc; i++) {
		ret = kstrtou8(argv[optind + i], 16, &val[i]);
		if (ret < 0)
			return ret;
	}

	return mipi_dsi_dcs_write(dsi, cmd, val, argc - optind);
}

BAREBOX_CMD_HELP_START(mipi_dsi)
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-w",  "issue write command")
BAREBOX_CMD_HELP_OPT ("-i",  "get display id")
BAREBOX_CMD_HELP_OPT ("-l\t",  "list all MIPI DSI devices")
BAREBOX_CMD_HELP_OPT ("-d DEVICE",  "select specific device (default is first registered)")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(mipi_dsi)
	.cmd		= do_mipi_dsi,
	BAREBOX_CMD_DESC("interact with MIPI DSI device")
	BAREBOX_CMD_OPTS("[-wild]")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_HELP(cmd_mipi_dsi_help)
BAREBOX_CMD_END
