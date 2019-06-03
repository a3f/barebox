#include <common.h>
#include <init.h>
#include <io.h>
#include <of.h>
#include <pinctrl.h>
#include <malloc.h>

#define MAX_PINS_ONE_IP			70
#define MODE_BITS_MASK			3
#define OSPEED_MASK			3
#define PUPD_MASK			3
#define OTYPE_MSK			1
#define AFR_MASK			0xF

struct stm32_pinctrl_priv {
	struct hwspinlock hws;
	int pinctrl_ngpios;
};

static int stm32_gpio_config(struct gpio_desc *desc,
			     const struct stm32_gpio_ctl *ctl)
{
	struct stm32_gpio_priv *priv = dev_get_priv(desc->dev);
	struct stm32_gpio_regs *regs = priv->regs;
	struct stm32_pinctrl_priv *pinctrl_priv;
	int ret;
	u32 index;

	if (!ctl || ctl->af > 15 || ctl->mode > 3 || ctl->otype > 1 ||
	    ctl->pupd > 2 || ctl->speed > 3)
		return -EINVAL;

	pinctrl_priv = dev_get_priv(dev_get_parent(desc->dev));

	ret = hwspinlock_lock_timeout(&pinctrl_priv->hws, 10);
	if (ret == -ETIME) {
		dev_err(desc->dev, "HWSpinlock timeout\n");
		return ret;
	}

	index = (desc->offset & 0x07) * 4;
	clrsetbits_le32(&regs->afr[desc->offset >> 3], AFR_MASK << index,
			ctl->af << index);

	index = desc->offset * 2;
	clrsetbits_le32(&regs->moder, MODE_BITS_MASK << index,
			ctl->mode << index);
	clrsetbits_le32(&regs->ospeedr, OSPEED_MASK << index,
			ctl->speed << index);
	clrsetbits_le32(&regs->pupdr, PUPD_MASK << index, ctl->pupd << index);

	index = desc->offset;
	clrsetbits_le32(&regs->otyper, OTYPE_MSK << index, ctl->otype << index);

	hwspinlock_unlock(&pinctrl_priv->hws);

	return 0;
}

static int prep_gpio_dsc(struct stm32_gpio_dsc *gpio_dsc, u32 port_pin)
{
	gpio_dsc->port = (port_pin & 0x1F000) >> 12;
	gpio_dsc->pin = (port_pin & 0x0F00) >> 8;
	debug("%s: GPIO:port= %d, pin= %d\n", __func__, gpio_dsc->port,
	      gpio_dsc->pin);

	return 0;
}

static int prep_gpio_ctl(struct stm32_gpio_ctl *gpio_ctl, u32 gpio_fn, int node)
{
	gpio_fn &= 0x00FF;
	gpio_ctl->af = 0;

	switch (gpio_fn) {
	case 0:
		gpio_ctl->mode = STM32_GPIO_MODE_IN;
		break;
	case 1 ... 16:
		gpio_ctl->mode = STM32_GPIO_MODE_AF;
		gpio_ctl->af = gpio_fn - 1;
		break;
	case 17:
		gpio_ctl->mode = STM32_GPIO_MODE_AN;
		break;
	default:
		gpio_ctl->mode = STM32_GPIO_MODE_OUT;
		break;
	}

	gpio_ctl->speed = ofnode_read_u32_default(gd->fdt_blob, node, "slew-rate", 0);

	if (ofnode_read_bool(gd->fdt_blob, node, "drive-open-drain"))
		gpio_ctl->otype = STM32_GPIO_OTYPE_OD;
	else
		gpio_ctl->otype = STM32_GPIO_OTYPE_PP;

	if (ofnode_read_bool(gd->fdt_blob, node, "bias-pull-up"))
		gpio_ctl->pupd = STM32_GPIO_PUPD_UP;
	else if (ofnode_read_bool(gd->fdt_blob, node, "bias-pull-down"))
		gpio_ctl->pupd = STM32_GPIO_PUPD_DOWN;
	else
		gpio_ctl->pupd = STM32_GPIO_PUPD_NO;

	debug("%s: gpio fn= %d, slew-rate= %x, op type= %x, pull-upd is = %x\n",
	      __func__,  gpio_fn, gpio_ctl->speed, gpio_ctl->otype,
	     gpio_ctl->pupd);

	return 0;
}
static int stm32_pinctrl_set_state(int offset)
static int stm32_pinctrl_set_state(struct pinctrl_device *pdev, struct device_node *np)
{
	u32 pin_mux[MAX_PINS_ONE_IP];
	int rv, len;

	/*
	 * check for "pinmux" property in each subnode of pin controller
	 * phandle "pinctrl-0" (e.g. pins1 and pins2 for usart1)
	 */
	fdt_for_each_subnode(offset, gd->fdt_blob, offset) {
		struct stm32_gpio_dsc gpio_dsc;
		struct stm32_gpio_ctl gpio_ctl;
		int i;

		len = ofnode_read_u32_array(np, "pinmux", pin_mux,
					    ARRAY_SIZE(pin_mux));
		if (len < 0)
			return -EINVAL;

		for (i = 0; i < len; i++) {
			struct gpio_desc desc;

			dev_dbg("%s: pinmux = %x\n", pdev->dev,  __func__, *(pin_mux + i));
			prep_gpio_dsc(&gpio_dsc, *(pin_mux + i));
			prep_gpio_ctl(&gpio_ctl, *(pin_mux + i), offset);
			rv = uclass_get_device_by_seq(UCLASS_GPIO,
						      gpio_dsc.port,
						      &desc.dev);
			if (rv)
				return rv;
			desc.offset = gpio_dsc.pin;
			rv = stm32_gpio_config(&desc, &gpio_ctl);
			dev_dbg("%s: rv = %d\n\n", pdev->dev, __func__, rv);
			if (rv)
				return rv;
		}
	}

	return 0;
}

static struct pinctrl_ops stm32_pinctrl_ops = {
	.set_state = stm32_pinctrl_set_state,
};

static int stm32_pinctrl_probe(struct device_d *dev)
{
	struct stm32_pinctrl *iomux;
	int ret = 0;

	iomux = xzalloc(sizeof(*iomux));

	iomux->base = dev_get_mem_region(dev, 0);

	iomux->pinctrl.dev = dev;
	iomux->pinctrl.ops = &stm32_pinctrl_ops;

#if 0
	/* hwspinlock property is optional, just log the error */
	err = hwspinlock_get_by_index(dev, 0, &priv->hws);
	if (err)
		debug("%s: hwspinlock_get_by_index may have failed (%d)\n",
		      __func__, err);
#endif

	ret = pinctrl_register(&iomux->pinctrl);
	if (ret)
		free(iomux);

	return ret;
}

static __maybe_unused struct of_device_id stm32_pinctrl_dt_ids[] = {
	{ .compatible = "st,stm32f429-pinctrl" },
	{ .compatible = "st,stm32f469-pinctrl" },
	{ .compatible = "st,stm32f746-pinctrl" },
	{ .compatible = "st,stm32h743-pinctrl" },
	{ .compatible = "st,stm32mp157-pinctrl" },
	{ .compatible = "st,stm32mp157-z-pinctrl" },
	{ /* sentinel */ }
};

static struct driver_d stm32_pinctrl_driver = {
	.name		= "stm32-pinctrl",
	.probe		= stm32_pinctrl_probe,
	.of_compatible	= DRV_OF_COMPAT(stm32_pinctrl_dt_ids),
};

static int stm32_pinctrl_init(void)
{
	return platform_driver_register(&stm32_pinctrl_driver);
}
core_initcall(stm32_pinctrl_init);
