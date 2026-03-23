// SPDX-License-Identifier: GPL-2.0
// SPDX-FileCopyrightText: Copyright (c) 2025, Sohaib Mohamed <sohaib.amhmd@gmail.com>
#include <filetype.h>
#include <common.h>
#include <init.h>
//#include <asm/memory.h>
//#include <mach/stm32mp/bbu.h>
//#include <bootsource.h>
#include <of.h>

static int aml_s905d3_cc_probe(struct device *dev)
{
	return 0;
}

static const struct of_device_id aml_s905d3_cc_of_match[] = {
	{ .compatible = "libretech,aml-s905d3-cc" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, aml_s905d3_cc_of_match);
BAREBOX_DEEP_PROBE_ENABLE(aml_s905d3_cc_of_match);

static struct driver aml_s905d3_cc_driver = {
	.name = "aml_s905d3_cc",
	.probe = aml_s905d3_cc_probe,
	.of_compatible = aml_s905d3_cc_of_match,
};
device_platform_driver(aml_s905d3_cc_driver);
