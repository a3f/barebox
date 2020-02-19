#ifndef _LINUX_RESET_H_
#define _LINUX_RESET_H_

struct device_d;
struct reset_control;

#ifdef CONFIG_RESET_CONTROLLER

int reset_control_reset(struct reset_control *rstc);
int reset_control_assert(struct reset_control *rstc);
int reset_control_deassert(struct reset_control *rstc);

struct reset_control *reset_control_get(struct device_d *dev, const char *id);
struct reset_control *of_reset_control_array_get(struct device_node *np, bool optional);
void reset_control_put(struct reset_control *rstc);

int __must_check device_reset(struct device_d *dev);

int __must_check device_reset_us(struct device_d *dev, int us);

#else

static inline int reset_control_reset(struct reset_control *rstc)
{
	WARN_ON(1);
	return 0;
}

static inline int reset_control_assert(struct reset_control *rstc)
{
	WARN_ON(1);
	return 0;
}

static inline int reset_control_deassert(struct reset_control *rstc)
{
	WARN_ON(1);
	return 0;
}

static inline struct reset_control *
reset_control_get(struct device_d *dev, const char *id)
{
	WARN_ON(1);
	return NULL;
}

static inline struct reset_control *
of_reset_control_array_get(struct device_node *np, bool optional)
{
	WARN_ON(!optional);
	return optional ? NULL : ERR_PTR(-ENOENT);
}

static inline void reset_control_put(struct reset_control *rstc)
{
	WARN_ON(1);
}

static inline int device_reset_us(struct device_d *dev, int us)
{
	WARN_ON(1);
	return 0;
}

static inline int device_reset(struct device_d *dev)
{
	WARN_ON(1);
	return 0;
}

#endif /* CONFIG_RESET_CONTROLLER */

static inline struct reset_control *
of_reset_control_array_get_optional(struct device_node *np)
{
	return of_reset_control_array_get(np, true);
}

#endif
