/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef PINCTRL_H
#define PINCTRL_H

#include <linux/types.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinmux.h>

struct pinctrl_device;
struct pinconf_param;
struct pinconf_ops;
struct device_node;

/**
 * struct pinctrl_ops - pin control operations, to be implemented by pin
 * controller drivers.
 * @set_state: apply a device tree pinctrl state node
 * @set_direction: set GPIO direction for a pin
 * @get_direction: get GPIO direction for a pin
 * @get_pins_count: return the number of named pins accepted by the pins property
 * @get_pin_name: return the pin name for a selector
 * @get_groups_count: returns the count of total number of groups registered
 * @get_group_name: return the group name of the pin group
 * @get_group_pins: return the pins belonging to a pin group
 */
struct pinctrl_ops {
	/* barebox-specific */
	int (*set_state)(struct pinctrl_device *, struct device_node *);
	int (*set_direction)(struct pinctrl_device *, unsigned int, bool);
	int (*get_direction)(struct pinctrl_device *, unsigned int);

	int (*get_pins_count)(struct pinctrl_device *pdev);
	const char *(*get_pin_name)(struct pinctrl_device *pdev,
				    unsigned int selector);
	/* Linux ops */
	int (*get_groups_count)(struct pinctrl_device *pdev);
	const char *(*get_group_name)(struct pinctrl_device *pdev,
				      unsigned int selector);
	int (*get_group_pins)(struct pinctrl_device *pdev,
			      unsigned int selector,
			      const unsigned int **pins,
			      unsigned int *npins);
};

struct pinctrl_device {
	struct device *dev;
	const struct pinctrl_ops *ops;
	const struct pinmux_ops *pmxops;
	const struct pinconf_ops *confops;
	struct list_head list;
	struct device_node *node;
	unsigned int base, npins;
};

int pinctrl_register(struct pinctrl_device *pdev);
void pinctrl_unregister(struct pinctrl_device *pdev);
int pinctrl_generic_set_state(struct pinctrl_device *pdev,
			      struct device_node *np);

#ifdef CONFIG_PINCTRL_STATE_PARAM
void of_pinctrl_register_consumer(struct device *dev, struct device_node *np);
void of_pinctrl_unregister_consumer(struct device *dev);
#else
static inline void of_pinctrl_register_consumer(struct device *dev, struct device_node *np)
{
}
static inline void of_pinctrl_unregister_consumer(struct device *dev) {}
#endif

#endif /* PINCTRL_H */
