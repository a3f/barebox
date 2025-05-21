/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __BAREBOX_SCONFIG_H
#define __BAREBOX_SCONFIG_H

#include <linux/types.h>
#include <linux/errno.h>
#include <notifier.h>

#ifdef CONFIG_SECURITY_POLICY
# include <generated/security_autoconf.h>
#else
#define SCONFIG_NUM 0
enum security_config_option { SCONFIG__DUMMY__ };
#endif

/*
 * It's recommended to use the following names for the
 * "standard" policies
 */
#define POLICY_DEVEL		"devel"
#define POLICY_FACTORY		"factory"
#define POLICY_LOCKDOWN		"lockdown"
#define POLICY_TAMPER		"tamper"
#define POLICY_FIELD_RETURN	"return"

struct security_policy {
	const char *name;
	bool chained;
	unsigned char policy[SCONFIG_NUM];
};

extern const char *sconfig_names[SCONFIG_NUM];

int sconfig_lookup(const char *name);

extern struct notifier_head sconfig_notifier_list;

extern const struct security_policy *active_policy;

const struct security_policy *security_policy_get(const char *name);

int security_policy_activate(const struct security_policy *policy);
int security_policy_select(const char *name);
void security_policy_list(void);

bool is_allowed(const struct security_policy *policy, unsigned option);

#define IF_ALLOWABLE(opt, then, else) \
	({ __if_defined(opt##_DEFINED, then, else); })

#ifdef CONFIG_SECURITY_POLICY
#define IS_ALLOWED(opt)		IF_ALLOWABLE(opt, is_allowed(NULL, (opt)), 0)
#define ALLOWABLE_VALUE(opt)	IF_ALLOWABLE(opt, opt, -1)
int __security_policy_register(const struct security_policy policy[]);
#else
#define IS_ALLOWED(opt)		1
#define ALLOWABLE_VALUE(opt)	(-1)
static inline int __security_policy_register(const struct security_policy policy[])
{
	return -ENOSYS;
}
#endif

#define security_policy_add(name) ({				\
	extern const struct security_policy __policy_##name[];	\
	__security_policy_register(__policy_##name);		\
})

static inline int sconfig_register_handler(struct notifier_block *nb,
					   int (*cb)(struct notifier_block *,
						     unsigned long, void *))
{
	if (!IS_ENABLED(CONFIG_SECURITY_POLICY))
		return -ENOSYS;

	nb->notifier_call = cb;
	return notifier_chain_register(&sconfig_notifier_list, nb);
}

static inline int sconfig_unregister_handler(struct notifier_block *nb)
{
	if (!IS_ENABLED(CONFIG_SECURITY_POLICY))
		return -ENOSYS;
	return notifier_chain_unregister(&sconfig_notifier_list, nb);
}

struct sconfig_notifier_block;
typedef void (*sconfig_notifier_callback_t)(struct sconfig_notifier_block *,
					    enum security_config_option,
					    bool val);

struct sconfig_notifier_block {
	struct notifier_block nb;
	enum security_config_option opt;
	sconfig_notifier_callback_t cb_filtered;
};

int __sconfig_register_handler_filtered(struct sconfig_notifier_block *nb,
					sconfig_notifier_callback_t cb,
					enum security_config_option);

#define sconfig_register_handler_filtered(nb, cb, opt) ({ \
	int __sopt = ALLOWABLE_VALUE(opt); \
	__sopt != -1	?  __sconfig_register_handler_filtered((nb), (cb), __sopt) \
			: -ENOSYS; \
})

#ifdef CONFIG_CMD_SCONFIG
void security_policy_unregister_one(const struct security_policy *policy);
#endif


#endif /* __BAREBOX_SCONFIG_H */
