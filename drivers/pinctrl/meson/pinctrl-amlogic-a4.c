// SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/pinctrl/meson/pinctrl-amlogic-a4.c?id=5fb024931949f3475260c84a0e4b0997af9c5530
// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Copyright (c) 2024 Amlogic, Inc. All rights reserved.
 * Author: Xianwei Zhao <xianwei.zhao@amlogic.com>
 *
 *
 *
 *
 *
 *
 *
 *
 *
 * Ignore this file & didn't port yet.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

#include <driver.h>
#include <gpio.h>
#include <init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <of.h>
#include <of_address.h>
#include <of_device.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <pinctrl.h>
#include <dt-bindings/pinctrl/amlogic,pinctrl.h>

#define gpio_chip_to_bank(chip) \
		container_of(chip, struct aml_gpio_bank, gpio_chip)

#define AML_REG_PULLEN		0
#define AML_REG_PULL		1
#define AML_REG_DIR		2
#define AML_REG_OUT		3
#define AML_REG_IN		4
#define AML_REG_DS		5
#define AML_NUM_REG		6

enum aml_pinconf_drv {
	PINCONF_DRV_500UA,
	PINCONF_DRV_2500UA,
	PINCONF_DRV_3000UA,
	PINCONF_DRV_4000UA,
};

struct aml_pio_control {
	u32 gpio_offset;
	u32 reg_offset[AML_NUM_REG];
	u32 bit_offset[AML_NUM_REG];
};

/*
 * partial bank(subordinate) pins mux config use other bank(main) mux registers
 * m_bank_id:	the main bank which pin_id from 0, but register bit not from bit 0
 * m_bit_offs:	bit offset the main bank mux register
 * sid:         start pin_id of subordinate bank
 * eid:         end pin_id of subordinate bank
 */
struct multi_mux {
	unsigned int m_bank_id;
	unsigned int m_bit_offs;
	unsigned int sid;
	unsigned int eid;
};

struct aml_pctl_data {
	unsigned int number;
	const struct multi_mux *p_mux;
};

struct aml_gpio_bank {
	struct gpio_chip		gpio_chip;
	struct aml_pio_control		pc;
	u32				bank_id;
	u32				mux_bit_offs;
	unsigned int			pin_base;
	struct regmap			*reg_mux;
	struct regmap			*reg_gpio;
	struct regmap			*reg_ds;
	const struct multi_mux		*p_mux;
};

struct aml_pinctrl {
	struct device			*dev;
	struct pinctrl_device		pctl;
	struct aml_gpio_bank		*banks;
	int				nbanks;
	const struct aml_pctl_data	*data;
};

static const unsigned int aml_bit_strides[AML_NUM_REG] = {
	1, 1, 1, 1, 1, 2
};

static const unsigned int aml_def_regoffs[AML_NUM_REG] = {
	3, 4, 2, 1, 0, 7
};


static const struct multi_mux multi_mux_s7[] = {
	{
		.m_bank_id = AMLOGIC_GPIO_CC,
		.m_bit_offs = 24,
		.sid = (AMLOGIC_GPIO_X << 8) + 16,
		.eid = (AMLOGIC_GPIO_X << 8) + 19,
	},
};

static const struct aml_pctl_data s7_priv_data = {
	.number = ARRAY_SIZE(multi_mux_s7),
	.p_mux = multi_mux_s7,
};

static const struct multi_mux multi_mux_s6[] = {
	{
		.m_bank_id = AMLOGIC_GPIO_CC,
		.m_bit_offs = 24,
		.sid = (AMLOGIC_GPIO_X << 8) + 16,
		.eid = (AMLOGIC_GPIO_X << 8) + 19,
	}, {
		.m_bank_id = AMLOGIC_GPIO_F,
		.m_bit_offs = 4,
		.sid = (AMLOGIC_GPIO_D << 8) + 6,
		.eid = (AMLOGIC_GPIO_D << 8) + 6,
	},
};

static const struct aml_pctl_data s6_priv_data = {
	.number = ARRAY_SIZE(multi_mux_s6),
	.p_mux = multi_mux_s6,
};

/* Find the bank that owns a global pin_id (bank_id << 8 | local_offset) */
static struct aml_gpio_bank *aml_find_bank(struct aml_pinctrl *info,
					   unsigned int pin_id)
{
	unsigned int bank_id = pin_id >> 8;
	int i;

	for (i = 0; i < info->nbanks; i++) {
		if (info->banks[i].bank_id == bank_id)
			return &info->banks[i];
	}
	return NULL;
}

/* Calculate register byte offset and bit position for a pin within a bank */
static void aml_calc_reg_and_bit(struct aml_gpio_bank *bank,
				 unsigned int local_pin,
				 unsigned int reg_type,
				 unsigned int *reg, unsigned int *bit)
{
	*bit = local_pin * aml_bit_strides[reg_type] + bank->pc.bit_offset[reg_type];
	*reg = (bank->pc.reg_offset[reg_type] + (*bit / 32)) * 4;
	*bit &= 0x1f;
}

/* Set pinmux function for a global pin_id */
static int aml_pctl_set_function(struct aml_pinctrl *info,
				 unsigned int pin_id, unsigned int func)
{
	struct aml_gpio_bank *bank;
	const struct multi_mux *p_mux;
	unsigned int shift, reg, offset;
	unsigned int local_pin;
	int i;

	bank = aml_find_bank(info, pin_id);
	if (!bank)
		return -EINVAL;

	/* peculiar mux reg set: subordinate bank pins use main bank's registers */
	if (bank->p_mux) {
		p_mux = bank->p_mux;
		if (pin_id >= p_mux->sid && pin_id <= p_mux->eid) {
			struct aml_gpio_bank *main_bank = NULL;

			for (i = 0; i < info->nbanks; i++) {
				if (info->banks[i].bank_id == p_mux->m_bank_id) {
					main_bank = &info->banks[i];
					break;
				}
			}

			if (!main_bank || !main_bank->reg_mux)
				return -EINVAL;

			shift = (pin_id - p_mux->sid) << 2;
			reg = (shift / 32) * 4;
			offset = shift % 32;
			return regmap_update_bits(main_bank->reg_mux, reg,
					0xf << offset, (func & 0xf) << offset);
		}
	}

	/* normal mux reg set */
	if (!bank->reg_mux)
		return 0;

	local_pin = pin_id - bank->pin_base;
	shift = (local_pin << 2) + bank->mux_bit_offs;
	reg = (shift / 32) * 4;
	offset = shift % 32;
	return regmap_update_bits(bank->reg_mux, reg,
				  0xf << offset, (func & 0xf) << offset);
}

/* --- Pin configuration helpers --- */

static int aml_pinconf_disable_bias(struct aml_pinctrl *info,
				    unsigned int pin_id)
{
	struct aml_gpio_bank *bank;
	unsigned int reg, bit = 0;

	bank = aml_find_bank(info, pin_id);
	if (!bank)
		return -EINVAL;

	aml_calc_reg_and_bit(bank, pin_id - bank->pin_base,
			     AML_REG_PULLEN, &reg, &bit);
	return regmap_update_bits(bank->reg_gpio, reg, BIT(bit), 0);
}

static int aml_pinconf_enable_bias(struct aml_pinctrl *info,
				   unsigned int pin_id, bool pull_up)
{
	struct aml_gpio_bank *bank;
	unsigned int reg, bit, val = 0;
	int ret;

	bank = aml_find_bank(info, pin_id);
	if (!bank)
		return -EINVAL;

	aml_calc_reg_and_bit(bank, pin_id - bank->pin_base,
			     AML_REG_PULL, &reg, &bit);
	if (pull_up)
		val = BIT(bit);

	ret = regmap_update_bits(bank->reg_gpio, reg, BIT(bit), val);
	if (ret)
		return ret;

	aml_calc_reg_and_bit(bank, pin_id - bank->pin_base,
			     AML_REG_PULLEN, &reg, &bit);
	return regmap_update_bits(bank->reg_gpio, reg, BIT(bit), BIT(bit));
}

static int aml_pinconf_set_drive_strength(struct aml_pinctrl *info,
					  unsigned int pin_id,
					  u32 drive_strength_ua)
{
	struct aml_gpio_bank *bank;
	unsigned int reg, bit, ds_val;

	bank = aml_find_bank(info, pin_id);
	if (!bank)
		return -EINVAL;

	if (!bank->reg_ds) {
		dev_err(info->dev, "drive-strength not supported\n");
		return -EOPNOTSUPP;
	}

	aml_calc_reg_and_bit(bank, pin_id - bank->pin_base,
			     AML_REG_DS, &reg, &bit);

	if (drive_strength_ua <= 500) {
		ds_val = PINCONF_DRV_500UA;
	} else if (drive_strength_ua <= 2500) {
		ds_val = PINCONF_DRV_2500UA;
	} else if (drive_strength_ua <= 3000) {
		ds_val = PINCONF_DRV_3000UA;
	} else if (drive_strength_ua <= 4000) {
		ds_val = PINCONF_DRV_4000UA;
	} else {
		dev_warn_once(info->dev,
			      "pin %u: invalid drive-strength : %d , default to 4mA\n",
			      pin_id, drive_strength_ua);
		ds_val = PINCONF_DRV_4000UA;
	}

	return regmap_update_bits(bank->reg_ds, reg, 0x3 << bit, ds_val << bit);
}

/* Apply optional bias/drive-strength config from a DT node to a single pin */
static void aml_apply_pin_config(struct aml_pinctrl *info,
				 unsigned int pin_id,
				 struct device_node *np)
{
	u32 ua;

	if (of_find_property(np, "bias-disable", NULL))
		aml_pinconf_disable_bias(info, pin_id);
	else if (of_find_property(np, "bias-pull-up", NULL))
		aml_pinconf_enable_bias(info, pin_id, true);
	else if (of_find_property(np, "bias-pull-down", NULL))
		aml_pinconf_enable_bias(info, pin_id, false);

	if (!of_property_read_u32(np, "drive-strength-microamp", &ua))
		aml_pinconf_set_drive_strength(info, pin_id, ua);
}

/*
 * Apply mux (and optional pin config) from a node that carries a "pinmux"
 * property.  Each entry is encoded as AML_PINMUX(bank, offset, mode):
 *   bits [31:8] = global pin_id = (bank_id << 8) + local_offset
 *   bits  [7:0] = mux function value
 */
static int aml_apply_pinmux_node(struct aml_pinctrl *info,
				 struct device_node *np)
{
	const __be32 *list;
	int size, i, ret;

	list = of_get_property(np, "pinmux", &size);
	if (!list)
		return 0;

	size /= sizeof(u32);
	for (i = 0; i < size; i++) {
		u32 val = be32_to_cpup(list + i);
		unsigned int pin_id = val >> 8;
		unsigned int func   = val & 0xff;

		ret = aml_pctl_set_function(info, pin_id, func);
		if (ret) {
			dev_err(info->dev,
				"failed to set mux for pin 0x%x func %u\n",
				pin_id, func);
			return ret;
		}
		aml_apply_pin_config(info, pin_id, np);
	}

	return 0;
}

/*
 * barebox pinctrl set_state callback.
 *
 * np is the pin-state node referenced by the device (e.g. pinctrl-0 = <&node>).
 * For the Amlogic a4 binding this node is a function node whose children carry
 * "pinmux" properties (nested format).  Flat format (pinmux directly on np) is
 * also supported.
 */
static int aml_pctl_set_state(struct pinctrl_device *pdev,
			      struct device_node *np)
{
	struct aml_pinctrl *info =
		container_of(pdev, struct aml_pinctrl, pctl);
	struct device_node *child;
	int ret;

	/* flat format: the state node itself carries "pinmux" */
	if (of_find_property(np, "pinmux", NULL))
		return aml_apply_pinmux_node(info, np);

	/* nested format: traverse child nodes */
	for_each_child_of_node(np, child) {
		ret = aml_apply_pinmux_node(info, child);
		if (ret)
			return ret;
	}

	return 0;
}

static struct pinctrl_ops aml_pctrl_ops = {
	.set_state = aml_pctl_set_state,
};

/* --- GPIO chip operations --- */

static void aml_gpio_set(struct gpio_chip *chip, unsigned int gpio, int value)
{
	struct aml_gpio_bank *bank = gpio_chip_to_bank(chip);
	unsigned int bit, reg;

	aml_calc_reg_and_bit(bank, gpio, AML_REG_OUT, &reg, &bit);
	regmap_update_bits(bank->reg_gpio, reg, BIT(bit),
			   value ? BIT(bit) : 0);
}

static int aml_gpio_get(struct gpio_chip *chip, unsigned int gpio)
{
	struct aml_gpio_bank *bank = gpio_chip_to_bank(chip);
	unsigned int reg, bit, val;

	aml_calc_reg_and_bit(bank, gpio, AML_REG_IN, &reg, &bit);
	regmap_read(bank->reg_gpio, reg, &val);

	return !!(val & BIT(bit));
}

static int aml_gpio_direction_input(struct gpio_chip *chip, unsigned int gpio)
{
	struct aml_gpio_bank *bank = gpio_chip_to_bank(chip);
	unsigned int bit, reg;

	aml_calc_reg_and_bit(bank, gpio, AML_REG_DIR, &reg, &bit);
	return regmap_update_bits(bank->reg_gpio, reg, BIT(bit), BIT(bit));
}

static int aml_gpio_direction_output(struct gpio_chip *chip,
				     unsigned int gpio, int value)
{
	struct aml_gpio_bank *bank = gpio_chip_to_bank(chip);
	unsigned int bit, reg;
	int ret;

	aml_calc_reg_and_bit(bank, gpio, AML_REG_DIR, &reg, &bit);
	ret = regmap_update_bits(bank->reg_gpio, reg, BIT(bit), 0);
	if (ret < 0)
		return ret;

	aml_calc_reg_and_bit(bank, gpio, AML_REG_OUT, &reg, &bit);
	return regmap_update_bits(bank->reg_gpio, reg, BIT(bit),
				  value ? BIT(bit) : 0);
}

static int aml_gpio_get_direction(struct gpio_chip *chip, unsigned int gpio)
{
	struct aml_gpio_bank *bank = gpio_chip_to_bank(chip);
	unsigned int bit, reg, val;
	int ret;

	aml_calc_reg_and_bit(bank, gpio, AML_REG_DIR, &reg, &bit);
	ret = regmap_read(bank->reg_gpio, reg, &val);
	if (ret)
		return ret;

	return BIT(bit) & val ? GPIOF_DIR_IN : GPIOF_DIR_OUT;
}

static struct gpio_ops aml_gpio_ops = {
	.direction_input  = aml_gpio_direction_input,
	.direction_output = aml_gpio_direction_output,
	.get_direction    = aml_gpio_get_direction,
	.get              = aml_gpio_get,
	.set              = aml_gpio_set,
};

/* --- Bank initialisation helpers --- */

static void init_bank_register_bit(struct aml_pinctrl *info,
				   struct aml_gpio_bank *bank)
{
	const struct aml_pctl_data *data = info->data;
	const struct multi_mux *p_mux;
	int i;

	for (i = 0; i < AML_NUM_REG; i++) {
		bank->pc.reg_offset[i] = aml_def_regoffs[i];
		bank->pc.bit_offset[i] = 0;
	}

	bank->mux_bit_offs = 0;

	if (data) {
		for (i = 0; i < data->number; i++) {
			p_mux = &data->p_mux[i];
			if (bank->bank_id == p_mux->m_bank_id) {
				bank->mux_bit_offs = p_mux->m_bit_offs;
				break;
			}
			if (p_mux->sid >> 8 == bank->bank_id) {
				bank->p_mux = p_mux;
				break;
			}
		}
	}
}

static u32 aml_bank_pins(struct device_node *np)
{
	struct of_phandle_args of_args;

	if (__of_parse_phandle_with_args(np, "gpio-ranges", NULL, 3,
					 0, &of_args))
		return 0;
	return of_args.args[2];
}

static int aml_bank_number(struct device_node *np)
{
	struct of_phandle_args of_args;

	if (__of_parse_phandle_with_args(np, "gpio-ranges", NULL, 3,
					 0, &of_args))
		return -EINVAL;
	return of_args.args[1] >> 8;
}

static struct regmap *aml_map_resource(struct device *dev,
				       struct device_node *node, char *name)
{
	struct resource res;
	void __iomem *base;
	int i;

	struct regmap_config aml_regmap_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
	};

	i = of_property_match_string(node, "reg-names", name);
	if (i < 0)
		return NULL;
	if (of_address_to_resource(node, i, &res))
		return NULL;

	base = devm_ioremap(dev, res.start, resource_size(&res));
	if (IS_ERR(base))
		return ERR_CAST(base);

	aml_regmap_config.max_register = resource_size(&res) - 4;
	aml_regmap_config.name = name;

	return regmap_init_mmio(dev, base, &aml_regmap_config);
}

static int aml_gpiolib_register_bank(struct aml_pinctrl *info,
				     int bank_nr, struct device_node *np)
{
	struct aml_gpio_bank *bank = &info->banks[bank_nr];
	struct device *dev = info->dev;
	struct device *gpio_dev;
	int ret;

	ret = aml_bank_number(np);
	if (ret < 0) {
		dev_err(dev, "get num=%d bank identity fail\n", bank_nr);
		return -EINVAL;
	}
	bank->bank_id = ret;

	bank->reg_mux = aml_map_resource(dev, np, "mux");
	if (IS_ERR_OR_NULL(bank->reg_mux)) {
		if (bank->bank_id == AMLOGIC_GPIO_TEST_N ||
		    bank->bank_id == AMLOGIC_GPIO_ANALOG)
			bank->reg_mux = NULL;
		else
			return dev_err_probe(dev,
					     bank->reg_mux ? PTR_ERR(bank->reg_mux) : -ENOENT,
					     "mux registers not found\n");
	}

	bank->reg_gpio = aml_map_resource(dev, np, "gpio");
	if (IS_ERR_OR_NULL(bank->reg_gpio))
		return dev_err_probe(dev,
				     bank->reg_gpio ? PTR_ERR(bank->reg_gpio) : -ENOENT,
				     "gpio registers not found\n");

	bank->reg_ds = aml_map_resource(dev, np, "ds");
	if (IS_ERR_OR_NULL(bank->reg_ds)) {
		dev_dbg(dev, "ds registers not found - skipping\n");
		bank->reg_ds = bank->reg_gpio;
	}

	init_bank_register_bit(info, bank);
	bank->pin_base = bank->bank_id << 8;

	gpio_dev = of_platform_device_create(np, dev);
	if (!gpio_dev) {
		dev_err(dev, "failed to create gpio child device\n");
		return -ENODEV;
	}
	of_platform_device_dummy_drv(gpio_dev);

	bank->gpio_chip.dev   = gpio_dev;
	bank->gpio_chip.ops   = &aml_gpio_ops;
	bank->gpio_chip.base  = -1;
	bank->gpio_chip.ngpio = aml_bank_pins(np);

	ret = gpiochip_add(&bank->gpio_chip);
	if (ret)
		return dev_err_probe(dev, ret,
				     "Failed to add gpiochip for bank %u\n",
				     bank->bank_id);

	return 0;
}

static int aml_pctl_probe(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *child;
	struct aml_pinctrl *info;
	int bank = 0, ret;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev  = dev;
	info->data = of_device_get_match_data(dev);
	dev_set_drvdata(dev, info);

	/* Count GPIO bank child nodes */
	for_each_child_of_node(np, child) {
		if (of_property_read_bool(child, "gpio-controller"))
			info->nbanks++;
	}

	if (!info->nbanks)
		return dev_err_probe(dev, -EINVAL,
				     "you need at least one gpio bank\n");

	info->banks = devm_kcalloc(dev, info->nbanks,
				   sizeof(*info->banks), GFP_KERNEL);
	if (!info->banks)
		return -ENOMEM;

	for_each_child_of_node(np, child) {
		if (!of_property_read_bool(child, "gpio-controller"))
			continue;
		ret = aml_gpiolib_register_bank(info, bank++, child);
		if (ret)
			return ret;
	}

	info->pctl.dev = dev;
	info->pctl.ops = &aml_pctrl_ops;

	ret = pinctrl_register(&info->pctl);
	if (ret)
		return dev_err_probe(dev, ret, "Failed pinctrl registration\n");

	return 0;
}

static const struct of_device_id aml_pctl_of_match[] = {
	{ .compatible = "amlogic,pinctrl-a4", },
	{ .compatible = "amlogic,pinctrl-s7", .data = &s7_priv_data, },
	{ .compatible = "amlogic,pinctrl-s6", .data = &s6_priv_data, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aml_pctl_of_match);

static struct driver aml_pctl_driver = {
	.name = "amlogic-pinctrl",
	.of_match_table = aml_pctl_of_match,
	.probe = aml_pctl_probe,
};
core_platform_driver(aml_pctl_driver);

MODULE_AUTHOR("Xianwei Zhao <xianwei.zhao@amlogic.com>");
MODULE_DESCRIPTION("Pin controller and GPIO driver for Amlogic SoC");
MODULE_LICENSE("Dual BSD/GPL");
