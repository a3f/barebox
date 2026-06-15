// SPDX-License-Identifier: GPL-2.0
// SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/pinctrl/mediatek/pinctrl-paris.c?id=203a83112e097a501fbe12722b6342787497efe0
/*
 * MediaTek Pinctrl Paris Driver, which implement the vendor per-pin
 * bindings for MediaTek SoC.
 *
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Sean Wang <sean.wang@mediatek.com>
 *	   Zhiyong Tao <zhiyong.tao@mediatek.com>
 *	   Hongzhou.Yang <hongzhou.yang@mediatek.com>
 */

#include <gpio.h>
#include <linux/module.h>
#include <linux/sprintf.h>

#include <linux/pinctrl/consumer.h>

#include <dt-bindings/pinctrl/mt65xx.h>

#include "pinctrl-paris.h"

/* Custom pinconf parameters */
#define MTK_PIN_CONFIG_TDSEL	(PIN_CONFIG_END + 1)
#define MTK_PIN_CONFIG_RDSEL	(PIN_CONFIG_END + 2)
#define MTK_PIN_CONFIG_PU_ADV	(PIN_CONFIG_END + 3)
#define MTK_PIN_CONFIG_PD_ADV	(PIN_CONFIG_END + 4)
#define MTK_PIN_CONFIG_DRV_ADV	(PIN_CONFIG_END + 5)

static const struct pinconf_generic_params mtk_custom_bindings[] = {
	{"mediatek,tdsel",	MTK_PIN_CONFIG_TDSEL,		0},
	{"mediatek,rdsel",	MTK_PIN_CONFIG_RDSEL,		0},
	{"mediatek,pull-up-adv", MTK_PIN_CONFIG_PU_ADV,		1},
	{"mediatek,pull-down-adv", MTK_PIN_CONFIG_PD_ADV,	1},
	{"mediatek,drive-strength-adv", MTK_PIN_CONFIG_DRV_ADV,	2},
};

static const char * const mtk_gpio_functions[] = {
	"func0", "func1", "func2", "func3",
	"func4", "func5", "func6", "func7",
	"func8", "func9", "func10", "func11",
	"func12", "func13", "func14", "func15",
};

static struct mtk_pinctrl *to_mtk_pinctrl(struct pinctrl_device *pctldev)
{
	return container_of(pctldev, struct mtk_pinctrl, pctrl);
}

static int mtk_pctrl_find_pin_by_number(struct mtk_pinctrl *hw, u32 pin_num)
{
	int i;

	for (i = 0; i < hw->soc->npins; i++)
		if (hw->soc->pins[i].number == pin_num)
			return i;

	return -EINVAL;
}

static int mtk_pctrl_get_pin_desc(struct mtk_pinctrl *hw, unsigned int pin,
				  const struct mtk_pin_desc **desc)
{
	int index;

	index = mtk_pctrl_find_pin_by_number(hw, pin);
	if (index < 0)
		return index;

	*desc = &hw->soc->pins[index];

	return 0;
}

/*
 * This section supports converting standard PIN_CONFIG_DRIVE_STRENGTH_UA
 * pin configs to custom MTK_PIN_CONFIG_DRV_ADV values.
 *
 * The custom value encodes three hardware bits as follows:
 *
 *   |           Bits           |
 *   | 2 (E1) | 1 (E0) | 0 (EN) | drive strength (uA)
 *   ------------------------------------------------
 *   |    x   |    x   |    0   | disabled, use standard drive strength
 *   -------------------------------------
 *   |    0   |    0   |    1   |  125 uA
 *   |    0   |    1   |    1   |  250 uA
 *   |    1   |    0   |    1   |  500 uA
 *   |    1   |    1   |    1   | 1000 uA
 */

static int mtk_drv_uA_to_adv(int val)
{
	switch (val) {
	case 125:
		return 0x1;
	case 250:
		return 0x3;
	case 500:
		return 0x5;
	case 1000:
		return 0x7;
	}

	return -EINVAL;
}

static int mtk_pinctrl_set_gpio_mode(struct mtk_pinctrl *hw, unsigned int pin)
{
	const struct mtk_pin_desc *desc;
	int ret;

	ret = mtk_pctrl_get_pin_desc(hw, pin, &desc);
	if (ret)
		return ret;

	return mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_MODE,
				hw->soc->gpio_m);
}

static int mtk_pinctrl_set_direction(struct pinctrl_device *pctldev,
				     unsigned int pin, bool input)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	const struct mtk_pin_desc *desc;
	int ret;

	ret = mtk_pctrl_get_pin_desc(hw, pin, &desc);
	if (ret)
		return ret;

	ret = mtk_pinctrl_set_gpio_mode(hw, pin);
	if (ret)
		return ret;

	/* hardware would take 0 as input direction */
	return mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DIR, !input);
}

static int mtk_pinctrl_get_direction(struct pinctrl_device *pctldev,
				     unsigned int pin)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	const struct mtk_pin_desc *desc;
	int value, ret;

	ret = mtk_pctrl_get_pin_desc(hw, pin, &desc);
	if (ret)
		return ret;

	ret = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DIR, &value);
	if (ret)
		return ret;

	return value ? GPIOF_DIR_OUT : GPIOF_DIR_IN;
}

static int mtk_pinconf_set(struct pinctrl_device *pctldev, unsigned int pin,
			   enum pin_config_param param, u32 arg)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	const struct mtk_pin_desc *desc;
	int err = -ENOTSUPP;
	u32 reg;

	err = mtk_pctrl_get_pin_desc(hw, pin, &desc);
	if (err)
		return err;

	switch ((u32)param) {
	case PIN_CONFIG_BIAS_DISABLE:
		if (!hw->soc->bias_set_combo)
			break;
		err = hw->soc->bias_set_combo(hw, desc, 0, MTK_DISABLE);
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		if (!hw->soc->bias_set_combo)
			break;
		err = hw->soc->bias_set_combo(hw, desc, 1, arg);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		if (!hw->soc->bias_set_combo)
			break;
		err = hw->soc->bias_set_combo(hw, desc, 0, arg);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		/* regard all non-zero value as enable */
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_IES, !!arg);
		break;
	case PIN_CONFIG_SLEW_RATE:
		/* regard all non-zero value as enable */
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_SR, !!arg);
		break;
	case PIN_CONFIG_LEVEL:
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DO,
				       arg);
		if (err)
			break;

		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DIR,
				       MTK_OUTPUT);
		break;
	case PIN_CONFIG_INPUT_SCHMITT:
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		/* arg = 1: Input mode & SMT enable ;
		 * arg = 0: Output mode & SMT disable
		 */
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DIR, !arg);
		if (err)
			break;

		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_SMT, !!arg);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		if (!hw->soc->drive_set)
			break;
		err = hw->soc->drive_set(hw, desc, arg);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH_UA:
		if (!hw->soc->adv_drive_set)
			break;

		err = mtk_drv_uA_to_adv(arg);
		if (err < 0)
			break;
		err = hw->soc->adv_drive_set(hw, desc, err);
		break;
	case MTK_PIN_CONFIG_TDSEL:
	case MTK_PIN_CONFIG_RDSEL:
		reg = (param == MTK_PIN_CONFIG_TDSEL) ?
		       PINCTRL_PIN_REG_TDSEL : PINCTRL_PIN_REG_RDSEL;
		err = mtk_hw_set_value(hw, desc, reg, arg);
		break;
	case MTK_PIN_CONFIG_PU_ADV:
	case MTK_PIN_CONFIG_PD_ADV:
		if (!hw->soc->adv_pull_set)
			break;
		err = hw->soc->adv_pull_set(hw, desc,
					    (param == MTK_PIN_CONFIG_PU_ADV),
					    arg);
		break;
	case MTK_PIN_CONFIG_DRV_ADV:
		if (!hw->soc->adv_drive_set)
			break;
		err = hw->soc->adv_drive_set(hw, desc, arg);
		break;
	}

	return err;
}

static const struct mtk_func_desc *
mtk_pctrl_find_function(const struct mtk_pin_desc *desc,
			unsigned int function)
{
	const struct mtk_func_desc *func = desc->funcs;

	while (func && func->name) {
		if (func->muxval == function)
			return func;
		func++;
	}

	return NULL;
}

static int mtk_pmx_set_pin_mode(struct pinctrl_device *pctldev,
				unsigned int pin, unsigned int function)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	const struct mtk_func_desc *desc_func;
	const struct mtk_pin_desc *desc;
	int ret;

	ret = mtk_pctrl_get_pin_desc(hw, pin, &desc);
	if (ret) {
		dev_err(hw->dev, "invalid pinmux value\n");
		return ret;
	}

	if (function >= ARRAY_SIZE(mtk_gpio_functions)) {
		dev_err(hw->dev, "invalid function %u on pin %u\n",
			function, desc->number);
		return -EINVAL;
	}

	if (!desc->funcs)
		return -ENOTSUPP;

	desc_func = mtk_pctrl_find_function(desc, function);
	if (!desc_func) {
		dev_err(hw->dev, "invalid function %u on pin %u\n",
			function, desc->number);
		return -EINVAL;
	}

	return mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_MODE,
				desc_func->muxval);
}

static bool mtk_pmx_is_soc_function(struct mtk_pinctrl *hw,
				    unsigned int function)
{
	return hw->soc->funcs && function < hw->soc->nfuncs;
}

static int mtk_pmx_get_raw_function(struct mtk_pinctrl *hw,
				    unsigned int function)
{
	if (!hw->soc->funcs)
		return function;

	if (function < hw->soc->nfuncs)
		return -EINVAL;

	return function - hw->soc->nfuncs;
}

static bool mtk_pmx_group_has_function(struct mtk_pinctrl *hw,
				       unsigned int function,
				       struct mtk_pinctrl_group *grp)
{
	const struct pinfunction *func;
	size_t i;

	if (!hw->soc->funcs)
		return true;

	if (function >= hw->soc->nfuncs)
		return false;

	func = &hw->soc->funcs[function];

	for (i = 0; i < func->ngroups; i++)
		if (!strcmp(func->groups[i], grp->name))
			return true;

	return false;
}

static int mtk_pmx_set_group_modes(struct pinctrl_device *pctldev,
				   struct mtk_pinctrl_group *grp)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	const int *pin_modes = grp->data;
	unsigned int i;

	if (!pin_modes)
		return -EINVAL;

	for (i = 0; i < grp->npins; i++) {
		const struct mtk_pin_desc *desc;
		unsigned int pin = grp->pins[i];
		int ret;

		ret = mtk_pctrl_get_pin_desc(hw, pin, &desc);
		if (ret)
			return ret;
		if (!desc->name)
			return -ENOTSUPP;

		ret = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_MODE,
				       pin_modes[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int mtk_pmx_set_mux(struct pinctrl_device *pctldev,
			   unsigned int function,
			   unsigned int group)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	struct mtk_pinctrl_group *grp;
	unsigned int i;
	int ret;

	if (group >= hw->ngroups)
		return -EINVAL;

	grp = hw->groups + group;

	if (mtk_pmx_is_soc_function(hw, function)) {
		if (!mtk_pmx_group_has_function(hw, function, grp)) {
			dev_err(hw->dev, "invalid function %u on group %u\n",
				function, group);
			return -EINVAL;
		}

		return mtk_pmx_set_group_modes(pctldev, grp);
	}

	function = mtk_pmx_get_raw_function(hw, function);
	if (function >= ARRAY_SIZE(mtk_gpio_functions)) {
		dev_err(hw->dev, "invalid raw function %u on group %u\n",
			function, group);
		return -EINVAL;
	}

	for (i = 0; i < grp->npins; i++) {
		ret = mtk_pmx_set_pin_mode(pctldev, grp->pins[i], function);
		if (ret)
			return ret;
	}

	return 0;
}

static int mtk_pmx_dt_pinmux_to_group(struct pinctrl_device *pctldev,
				      u32 pinfunc,
				      unsigned int *func_selector,
				      unsigned int *group_selector)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	unsigned int pin_num = MTK_GET_PIN_NO(pinfunc);
	unsigned int function = MTK_GET_PIN_FUNC(pinfunc);
	int pin;

	pin = mtk_pctrl_find_pin_by_number(hw, pin_num);
	if (pin < 0) {
		dev_err(hw->dev, "invalid pin number %u\n", pin_num);
		return pin;
	}

	if (hw->soc->funcs)
		function += hw->soc->nfuncs;

	*func_selector = function;
	*group_selector = (hw->soc->grps ? hw->soc->ngrps : 0) + pin;

	return 0;
}

static int mtk_pctrl_get_pins_count(struct pinctrl_device *pctldev)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);

	return hw->soc->npins;
}

static const char *mtk_pctrl_get_pin_name(struct pinctrl_device *pctldev,
					  unsigned int index)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);

	if (index >= hw->soc->npins)
		return NULL;

	return hw->soc->pins[index].name;
}

static int mtk_pctrl_get_pin_selector(struct pinctrl_device *pctldev,
				      unsigned int index,
				      unsigned int *selector)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);

	if (index >= hw->soc->npins)
		return -EINVAL;

	*selector = hw->soc->pins[index].number;

	return 0;
}

static int mtk_pctrl_get_groups_count(struct pinctrl_device *pctldev)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);

	return hw->ngroups;
}

static const char *mtk_pctrl_get_group_name(struct pinctrl_device *pctldev,
					    unsigned group)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);

	if (group >= hw->ngroups)
		return NULL;

	return hw->groups[group].name;
}

static int mtk_pctrl_get_group_pins(struct pinctrl_device *pctldev,
				    unsigned int group,
				    const unsigned int **pins,
				    unsigned int *npins)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);

	if (group >= hw->ngroups)
		return -EINVAL;

	*pins = hw->groups[group].pins;
	*npins = hw->groups[group].npins;

	return 0;
}

static int mtk_hw_get_value_wrap(struct mtk_pinctrl *hw, unsigned int gpio, int field)
{
	const struct mtk_pin_desc *desc;
	int value, err;

	err = mtk_pctrl_get_pin_desc(hw, gpio, &desc);
	if (err)
		return err;

	err = mtk_hw_get_value(hw, desc, field, &value);
	if (err)
		return err;

	return value;
}

#define mtk_pctrl_get_pinmux(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_MODE)

#define mtk_pctrl_get_direction(hw, gpio)		\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DIR)

#define mtk_pctrl_get_out(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DO)

#define mtk_pctrl_get_in(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DI)

#define mtk_pctrl_get_smt(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_SMT)

#define mtk_pctrl_get_ies(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_IES)

#define mtk_pctrl_get_driving(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DRV)

ssize_t mtk_pctrl_show_one_pin(struct mtk_pinctrl *hw,
	unsigned int gpio, char *buf, unsigned int buf_len)
{
	int pinmux, pullup = 0, pullen = 0, len = 0, r1 = -1, r0 = -1, rsel = -1;
	const struct mtk_pin_desc *desc;
	u32 try_all_type = 0;
	int ret;

	if (mtk_is_virt_gpio(hw, gpio))
		return -EINVAL;

	ret = mtk_pctrl_get_pin_desc(hw, gpio, &desc);
	if (ret)
		return ret;

	pinmux = mtk_pctrl_get_pinmux(hw, gpio);
	if (pinmux >= hw->soc->nfuncs)
		pinmux -= hw->soc->nfuncs;

	mtk_pinconf_bias_get_combo(hw, desc, &pullup, &pullen);

	if (hw->soc->pull_type)
		try_all_type = hw->soc->pull_type[desc->number];

	if (hw->rsel_si_unit && (try_all_type & MTK_PULL_RSEL_TYPE)) {
		rsel = pullen;
		pullen = 1;
	} else {
		/* Case for: R1R0 */
		if (pullen == MTK_PUPD_SET_R1R0_00) {
			pullen = 0;
			r1 = 0;
			r0 = 0;
		} else if (pullen == MTK_PUPD_SET_R1R0_01) {
			pullen = 1;
			r1 = 0;
			r0 = 1;
		} else if (pullen == MTK_PUPD_SET_R1R0_10) {
			pullen = 1;
			r1 = 1;
			r0 = 0;
		} else if (pullen == MTK_PUPD_SET_R1R0_11) {
			pullen = 1;
			r1 = 1;
			r0 = 1;
		}

		/* Case for: RSEL */
		if (pullen >= MTK_PULL_SET_RSEL_000 &&
		    pullen <= MTK_PULL_SET_RSEL_111) {
			rsel = pullen - MTK_PULL_SET_RSEL_000;
			pullen = 1;
		}
	}
	len += scnprintf(buf + len, buf_len - len,
			"%03d: %1d%1d%1d%1d%02d%1d%1d%1d%1d",
			gpio,
			pinmux,
			mtk_pctrl_get_direction(hw, gpio),
			mtk_pctrl_get_out(hw, gpio),
			mtk_pctrl_get_in(hw, gpio),
			mtk_pctrl_get_driving(hw, gpio),
			mtk_pctrl_get_smt(hw, gpio),
			mtk_pctrl_get_ies(hw, gpio),
			pullen,
			pullup);

	if (r1 != -1)
		len += scnprintf(buf + len, buf_len - len, " (%1d %1d)", r1, r0);
	else if (rsel != -1)
		len += scnprintf(buf + len, buf_len - len, " (%1d)", rsel);

	return len;
}
EXPORT_SYMBOL_GPL(mtk_pctrl_show_one_pin);

static int mtk_pmx_get_funcs_cnt(struct pinctrl_device *pctldev)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);

	if (hw->soc->funcs)
		return hw->soc->nfuncs;

	return ARRAY_SIZE(mtk_gpio_functions);
}

static const char *mtk_pmx_get_func_name(struct pinctrl_device *pctldev,
					 unsigned selector)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);

	if (hw->soc->funcs) {
		if (selector >= hw->soc->nfuncs)
			return NULL;

		return hw->soc->funcs[selector].name;
	}

	if (selector >= ARRAY_SIZE(mtk_gpio_functions))
		return NULL;

	return mtk_gpio_functions[selector];
}

static int mtk_pmx_get_function_groups(struct pinctrl_device *pctldev,
				       unsigned int selector,
				       const char * const **groups,
				       unsigned int *num_groups)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	const struct pinfunction *func;

	if (!hw->soc->funcs)
		return -ENOTSUPP;
	if (selector >= hw->soc->nfuncs)
		return -EINVAL;

	func = &hw->soc->funcs[selector];
	*groups = func->groups;
	*num_groups = func->ngroups;

	return 0;
}

static int mtk_pconf_set(struct pinctrl_device *pctldev, unsigned int pin,
			 unsigned long *configs, unsigned int num_configs)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	const struct mtk_pin_desc *desc;
	bool drive_strength_uA_found = false;
	bool adv_drve_strength_found = false;
	int i, ret;

	ret = mtk_pctrl_get_pin_desc(hw, pin, &desc);
	if (ret)
		return ret;

	for (i = 0; i < num_configs; i++) {
		ret = mtk_pinconf_set(pctldev, pin,
				      pinconf_to_config_param(configs[i]),
				      pinconf_to_config_argument(configs[i]));
		if (ret < 0)
			return ret;

		if (pinconf_to_config_param(configs[i]) == PIN_CONFIG_DRIVE_STRENGTH_UA)
			drive_strength_uA_found = true;
		if (pinconf_to_config_param(configs[i]) == MTK_PIN_CONFIG_DRV_ADV)
			adv_drve_strength_found = true;
	}

	/*
	 * Disable advanced drive strength mode if drive-strength-microamp
	 * is not set. However, mediatek,drive-strength-adv takes precedence
	 * as its value can explicitly request the mode be enabled or not.
	 */
	if (hw->soc->adv_drive_set && !drive_strength_uA_found &&
	    !adv_drve_strength_found)
		hw->soc->adv_drive_set(hw, desc, 0);

	return 0;
}

static int mtk_pconf_group_set(struct pinctrl_device *pctldev, unsigned int group,
			       unsigned long *configs, unsigned int num_configs)
{
	struct mtk_pinctrl *hw = to_mtk_pinctrl(pctldev);
	struct mtk_pinctrl_group *grp;
	unsigned int i;
	int ret;

	if (group >= hw->ngroups)
		return -EINVAL;

	grp = &hw->groups[group];

	for (i = 0; i < grp->npins; i++) {
		ret = mtk_pconf_set(pctldev, grp->pins[i], configs,
				    num_configs);
		if (ret)
			return ret;
	}

	return 0;
}

static int mtk_pinctrl_set_state(struct pinctrl_device *pctldev,
				 struct device_node *np)
{
	return pinctrl_generic_set_state(pctldev, np);
}

static const struct pinctrl_ops mtk_pctrl_ops = {
	.set_state		= mtk_pinctrl_set_state,
	.set_direction		= mtk_pinctrl_set_direction,
	.get_direction		= mtk_pinctrl_get_direction,
	.get_pins_count		= mtk_pctrl_get_pins_count,
	.get_pin_name		= mtk_pctrl_get_pin_name,
	.get_pin_selector	= mtk_pctrl_get_pin_selector,
	.get_groups_count	= mtk_pctrl_get_groups_count,
	.get_group_name		= mtk_pctrl_get_group_name,
	.get_group_pins		= mtk_pctrl_get_group_pins,
};

static const struct pinmux_ops mtk_pmx_ops = {
	.get_functions_count	= mtk_pmx_get_funcs_cnt,
	.get_function_name	= mtk_pmx_get_func_name,
	.get_function_groups	= mtk_pmx_get_function_groups,
	.set_mux		= mtk_pmx_set_mux,
	.dt_pinmux_to_group	= mtk_pmx_dt_pinmux_to_group,
};

static const struct pinconf_ops mtk_pconf_ops = {
	.custom_params		= mtk_custom_bindings,
	.num_custom_params	= ARRAY_SIZE(mtk_custom_bindings),
	.pin_config_set		= mtk_pconf_set,
	.pin_config_group_set	= mtk_pconf_group_set,
};

static int mtk_gpio_get_direction(struct gpio_chip *chip, unsigned int gpio)
{
	struct mtk_pinctrl *hw = container_of(chip, struct mtk_pinctrl, chip);

	/*
	 * "Virtual" GPIOs are always and only used for interrupts
	 * Since they are only used for interrupts, they are always inputs
	 */
	if (mtk_is_virt_gpio(hw, gpio))
		return GPIOF_DIR_IN;

	return mtk_pinctrl_get_direction(&hw->pctrl, gpio);
}

static int mtk_gpio_get(struct gpio_chip *chip, unsigned int gpio)
{
	struct mtk_pinctrl *hw = container_of(chip, struct mtk_pinctrl, chip);
	const struct mtk_pin_desc *desc;
	int value, err;

	err = mtk_pctrl_get_pin_desc(hw, gpio, &desc);
	if (err)
		return err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DI, &value);
	if (err)
		return err;

	return !!value;
}

static int mtk_gpio_set(struct gpio_chip *chip, unsigned int gpio, int value)
{
	struct mtk_pinctrl *hw = container_of(chip, struct mtk_pinctrl, chip);
	const struct mtk_pin_desc *desc;
	int ret;

	ret = mtk_pctrl_get_pin_desc(hw, gpio, &desc);
	if (ret)
		return ret;

	return mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DO, !!value);
}

static int mtk_gpio_direction_input(struct gpio_chip *chip, unsigned int gpio)
{
	struct mtk_pinctrl *hw = container_of(chip, struct mtk_pinctrl, chip);

	return mtk_pinctrl_set_direction(&hw->pctrl, gpio, true);
}

static int mtk_gpio_direction_output(struct gpio_chip *chip, unsigned int gpio,
				     int value)
{
	struct mtk_pinctrl *hw = container_of(chip, struct mtk_pinctrl, chip);
	int ret;

	ret = mtk_gpio_set(chip, gpio, value);
	if (ret)
		return ret;

	return mtk_pinctrl_set_direction(&hw->pctrl, gpio, false);
}

static int mtk_gpio_set_config(struct gpio_chip *chip, unsigned int offset,
			       unsigned long config)
{
	struct mtk_pinctrl *hw = container_of(chip, struct mtk_pinctrl, chip);
	unsigned long configs[] = { config };

	return mtk_pconf_set(&hw->pctrl, offset, configs, ARRAY_SIZE(configs));
}

static int mtk_build_gpiochip(struct mtk_pinctrl *hw)
{
	struct gpio_chip *chip = &hw->chip;

	static struct gpio_ops mtk_gpio_ops = {
		.direction_input = mtk_gpio_direction_input,
		.direction_output = mtk_gpio_direction_output,
		.get_direction = mtk_gpio_get_direction,
		.get = mtk_gpio_get,
		.set = mtk_gpio_set,
		.set_config = IS_ENABLED(CONFIG_GPIO_PINCONF) ?
			mtk_gpio_set_config : NULL,
	};

	chip->dev = hw->dev;
	chip->ops = &mtk_gpio_ops;
	chip->base = -1;
	chip->ngpio = hw->npins;

	return gpiochip_add(chip);
}

static int mtk_pctrl_build_state(struct mtk_pinctrl *hw)
{
	struct device *dev = hw->dev;
	unsigned int legacy_group_offset;
	unsigned int max_pin = 0;
	int i;

	if (hw->soc->grps)
		legacy_group_offset = hw->soc->ngrps;
	else
		legacy_group_offset = 0;

	hw->ngroups = legacy_group_offset + hw->soc->npins;

	/* Allocate groups */
	hw->groups = devm_kmalloc_array(dev, hw->ngroups,
					sizeof(*hw->groups), GFP_KERNEL);
	if (!hw->groups)
		return -ENOMEM;

	if (hw->soc->grps) {
		for (i = 0; i < hw->soc->ngrps; i++) {
			const struct group_desc *src = &hw->soc->grps[i];
			struct mtk_pinctrl_group *group = &hw->groups[i];

			if (!src->grp.name || !src->grp.pins || !src->grp.npins)
				return -EINVAL;

			group->name = src->grp.name;
			group->pins = src->grp.pins;
			group->npins = src->grp.npins;
			group->data = src->data;
		}
	}

	for (i = 0; i < hw->soc->npins; i++) {
		const struct mtk_pin_desc *pin = hw->soc->pins + i;
		struct mtk_pinctrl_group *group;

		group = hw->groups + legacy_group_offset + i;

		group->name = pin->name;
		group->pin = pin->number;
		group->pins = &group->pin;
		group->npins = 1;
		group->data = NULL;

		if (pin->number > max_pin)
			max_pin = pin->number;
	}

	hw->npins = hw->soc->npins ? max_pin + 1 : 0;

	return 0;
}

int mtk_paris_pinctrl_probe(struct device *dev)
{
	struct mtk_pinctrl *hw;
	int err, i;

	hw = devm_kzalloc(dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	dev_set_drvdata(dev, hw);

	hw->soc = device_get_match_data(dev);
	if (!hw->soc)
		return -ENOENT;

	hw->dev = dev;

	if (!hw->soc->nbase_names)
		return dev_err_probe(dev, -EINVAL,
			"SoC should be assigned at least one register base\n");

	hw->base = devm_kmalloc_array(dev, hw->soc->nbase_names,
				      sizeof(*hw->base), GFP_KERNEL);
	if (!hw->base)
		return -ENOMEM;

	for (i = 0; i < hw->soc->nbase_names; i++) {
		hw->base[i] = dev_request_mem_region_by_name(dev,
					hw->soc->base_names[i]);
		if (IS_ERR(hw->base[i]))
			return PTR_ERR(hw->base[i]);
	}

	hw->nbase = hw->soc->nbase_names;

	hw->rsel_si_unit = of_property_read_bool(hw->dev->of_node,
						 "mediatek,rsel-resistance-in-si-unit");

	spin_lock_init(&hw->lock);

	err = mtk_pctrl_build_state(hw);
	if (err)
		return dev_err_probe(dev, err, "build state failed\n");

	hw->pctrl.dev = dev;
	hw->pctrl.ops = &mtk_pctrl_ops;
	hw->pctrl.pmxops = &mtk_pmx_ops;
	hw->pctrl.confops = &mtk_pconf_ops;
	hw->pctrl.base = 0;
	hw->pctrl.npins = hw->npins;

	err = pinctrl_register(&hw->pctrl);
	if (err)
		return err;

	/* Build gpiochip after pinctrl registration so default states apply first. */
	err = mtk_build_gpiochip(hw);
	if (err)
		return dev_err_probe(dev, err, "Failed to add gpio_chip\n");

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_paris_pinctrl_probe);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek Pinctrl Common Driver V2 Paris");
