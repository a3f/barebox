// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "deep-probe: " fmt

#if defined(CONFIG_DEBUG_INITCALLS) || defined(CONFIG_DEBUG_PROBES)
#define DEBUG
#endif

#include <common.h>
#include <deep-probe.h>
#include <of.h>

enum deep_probe_state {
	DEEP_PROBE_UNKNOWN = -1,
	DEEP_PROBE_NOT_SUPPORTED,
	DEEP_PROBE_SUPPORTED
};

static enum deep_probe_state boardstate = DEEP_PROBE_UNKNOWN;

bool deep_probe_is_supported(void)
{
	bool deep_probe_default = IS_ENABLED(CONFIG_DEEP_PROBE_DEFAULT);
	struct deep_probe_entry *board;
	struct device_node *root;

	if (boardstate > DEEP_PROBE_UNKNOWN)
		return boardstate;

	/* deep probe requires resources to be described in DT */
	root = of_get_root_node();
	if (!root)
		return false;

	/* determine boardstate */
	for (board = __barebox_deep_probe_start;
	     board != __barebox_deep_probe_end; board++) {
		const struct of_device_id *matches = board->device_id;

		for (; matches->compatible; matches++) {
			if (of_machine_is_compatible(matches->compatible)) {
				boardstate = DEEP_PROBE_SUPPORTED;
				pr_debug("supported due to %s\n", matches->compatible);
				return true;
			}
		}
	}

	if (of_property_read_bool(root, "barebox,disable-deep-probe")) {
		boardstate = DEEP_PROBE_NOT_SUPPORTED;
		pr_info("disabled in device tree\n");
	} else if (of_property_read_bool(root, "barebox,deep-probe")) {
		boardstate = DEEP_PROBE_SUPPORTED;
		pr_debug("enabled in device tree\n");
	} else if (deep_probe_default) {
		boardstate = DEEP_PROBE_SUPPORTED;
		pr_debug("activated by default\n");
	} else {
		boardstate = DEEP_PROBE_NOT_SUPPORTED;
		pr_warn("DT missing barebox,deep-probe or barebox,disable-deep-probe property\n");
		pr_info("not activated by default\n");
	}

	return boardstate;
}
EXPORT_SYMBOL_GPL(deep_probe_is_supported);
