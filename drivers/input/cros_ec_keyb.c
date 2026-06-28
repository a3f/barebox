// SPDX-License-Identifier: GPL-2.0
// SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/input/keyboard/cros_ec_keyb.c?id=21a60fcd24bae2ecdb96351463618647e1edf871
// ChromeOS EC keyboard driver
//
// Copyright (C) 2012 Google, Inc.
//
// This driver uses the ChromeOS EC byte-level message-based protocol for
// communicating the keyboard state (which keys are pressed) from a keyboard EC
// to the AP over some bus (such as i2c, lpc, spi).  The EC does debouncing,
// but everything else (including deghosting) is done here.  The main
// motivation for this is to keep the EC firmware as simple as possible, since
// it cannot be easily upgraded and EC flash/IRAM space is relatively
// expensive.

#include <module.h>
#include <linux/bitops.h>
#include <i2c/i2c.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/input/matrix_keypad.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>

/*
 * Maximum size of the normal key matrix, this is limited by the host command
 * key_matrix field defined in ec_response_get_next_data_v3
 */
#define CROS_EC_KEYBOARD_COLS_MAX 18
#define CROS_EC_KEYB_POLL_NS		(100 * MSECOND)

/**
 * struct cros_ec_keyb - Structure representing EC keyboard device
 *
 * @rows: Number of rows in the keypad
 * @cols: Number of columns in the keypad
 * @row_shift: log2 or number of rows, rounded up
 * @ghost_filter: true to enable the matrix key-ghosting filter
 * @valid_keys: bitmap of existing keys for each matrix column
 * @old_kb_state: bitmap of keys pressed last scan
 * @dev: Device pointer
 * @ec: Top level ChromeOS device to use to talk to EC
 * @idev: The input device for the matrix keys.
 * @poller: Asynchronous poller used to read EC events
 */
struct cros_ec_keyb {
	unsigned int rows;
	unsigned int cols;
	int row_shift;
	bool ghost_filter;
	u8 valid_keys[CROS_EC_KEYBOARD_COLS_MAX];
	u8 old_kb_state[CROS_EC_KEYBOARD_COLS_MAX];

	struct device *dev;
	struct cros_ec_device *ec;

	struct input_dev idev;

	struct poller_async poller;
};

/*
 * Returns true when there is at least one combination of pressed keys that
 * results in ghosting.
 */
static bool cros_ec_keyb_has_ghosting(struct cros_ec_keyb *ckdev, u8 *buf)
{
	int col1, col2, buf1, buf2;
	struct device *dev = ckdev->dev;
	u8 *valid_keys = ckdev->valid_keys;

	/*
	 * Ghosting happens if for any pressed key X there are other keys
	 * pressed both in the same row and column of X as, for instance,
	 * in the following diagram:
	 *
	 * . . Y . g .
	 * . . . . . .
	 * . . . . . .
	 * . . X . Z .
	 *
	 * In this case only X, Y, and Z are pressed, but g appears to be
	 * pressed too (see Wikipedia).
	 */
	for (col1 = 0; col1 < ckdev->cols; col1++) {
		buf1 = buf[col1] & valid_keys[col1];
		for (col2 = col1 + 1; col2 < ckdev->cols; col2++) {
			buf2 = buf[col2] & valid_keys[col2];
			if (hweight8(buf1 & buf2) > 1) {
				dev_dbg(dev, "ghost found at: B[%02d]:0x%02x & B[%02d]:0x%02x",
					col1, buf1, col2, buf2);
				return true;
			}
		}
	}

	return false;
}

static void cros_ec_keyb_process_key_plain(struct cros_ec_keyb *ckdev,
					   int row, int col, bool state)
{
	struct input_dev *idev = &ckdev->idev;
	const unsigned short *keycodes = idev->keycode;
	int pos = MATRIX_SCAN_CODE(row, col, ckdev->row_shift);
	unsigned int code;

	code = keycodes[pos];
	if (code == KEY_RESERVED)
		return;

	input_report_key_event(idev, code, state);
}

static void cros_ec_keyb_process_col(struct cros_ec_keyb *ckdev, int col,
				     u8 col_state, u8 changed)
{
	for (int row = 0; row < ckdev->rows; row++) {
		if (changed & BIT(row)) {
			u8 key_state = col_state & BIT(row);

			dev_dbg(ckdev->dev, "changed: [r%d c%d]: byte %02x\n",
				row, col, key_state);

			cros_ec_keyb_process_key_plain(ckdev, row, col,
						       key_state);
		}
	}
}

/*
 * Compares the new keyboard state to the old one and produces key
 * press/release events accordingly.  The keyboard state is one byte
 * per column.
 */
static void cros_ec_keyb_process(struct cros_ec_keyb *ckdev, u8 *kb_state, int len)
{
	if (ckdev->ghost_filter && cros_ec_keyb_has_ghosting(ckdev, kb_state)) {
		/*
		 * Simple-minded solution: ignore this state. The obvious
		 * improvement is to only ignore changes to keys involved in
		 * the ghosting, but process the other changes.
		 */
		dev_dbg(ckdev->dev, "ghosting found\n");
		return;
	}

	for (int col = 0; col < ckdev->cols; col++) {
		u8 changed = kb_state[col] ^ ckdev->old_kb_state[col];

		if (changed)
			cros_ec_keyb_process_col(ckdev, col, kb_state[col],
						 changed);
	}

	memcpy(ckdev->old_kb_state, kb_state, sizeof(ckdev->old_kb_state));
}

/*
 * Walks keycodes flipping bit in buffer COLUMNS deep where bit is ROW.  Used by
 * ghosting logic to ignore NULL or virtual keys.
 */
static void cros_ec_keyb_compute_valid_keys(struct cros_ec_keyb *ckdev)
{
	int row, col;
	int row_shift = ckdev->row_shift;
	unsigned short *keymap = ckdev->idev.keycode;
	unsigned short code;

	BUG_ON(ckdev->idev.keycodesize != sizeof(*keymap));

	for (col = 0; col < ckdev->cols; col++) {
		for (row = 0; row < ckdev->rows; row++) {
			code = keymap[MATRIX_SCAN_CODE(row, col, row_shift)];
			if (code != KEY_RESERVED && code != KEY_BATTERY)
				ckdev->valid_keys[col] |= BIT(row);
		}
		dev_dbg(ckdev->dev, "valid_keys[%02d] = 0x%02x\n",
			col, ckdev->valid_keys[col]);
	}
}

static int cros_ec_keyb_get_state(struct cros_ec_keyb *ckdev, u8 *state)
{
	struct ec_params_mkbp_info params = {
		.info_type = EC_MKBP_INFO_CURRENT,
		.event_type = EC_MKBP_EVENT_KEY_MATRIX,
	};
	int ret;

	ret = cros_ec_cmd(ckdev->ec, 1, EC_CMD_MKBP_INFO, &params,
			  sizeof(params), state, ckdev->cols);
	if (ret == ckdev->cols)
		return 0;

	ret = cros_ec_cmd(ckdev->ec, 0, EC_CMD_MKBP_STATE, NULL, 0,
			  state, ckdev->cols);
	if (ret == ckdev->cols)
		return 0;

	if (ret >= 0)
		return -EMSGSIZE;

	return ret;
}

static void cros_ec_keyb_poll(void *arg)
{
	struct cros_ec_keyb *ckdev = arg;
	int ret;
	u8 state[CROS_EC_KEYBOARD_COLS_MAX];

	if (slice_acquired(ckdev->ec->slice))
		goto out;

	ret = cros_ec_keyb_get_state(ckdev, state);
	if (!ret)
		cros_ec_keyb_process(ckdev, state, ckdev->cols);

out:
	poller_call_async(&ckdev->poller, CROS_EC_KEYB_POLL_NS,
			  cros_ec_keyb_poll, ckdev);
}

/**
 * cros_ec_keyb_register_matrix - Register matrix keys
 *
 * Handles all the bits of the keyboard driver related to matrix keys.
 *
 * @ckdev: The keyboard device
 *
 * Returns 0 if no error or -error upon error.
 */
static int cros_ec_keyb_register_matrix(struct cros_ec_keyb *ckdev)
{
	struct device *dev = ckdev->dev;
	struct device_node *np = dev_of_node(dev);
	struct input_dev *idev = &ckdev->idev;
	u8 state[CROS_EC_KEYBOARD_COLS_MAX];
	int err;

	err = matrix_keypad_parse_properties(dev, &ckdev->rows, &ckdev->cols);
	if (err)
		return err;

	if (ckdev->cols > CROS_EC_KEYBOARD_COLS_MAX) {
		dev_err(dev, "keypad,num-columns too large: %d (max: %d)\n",
			ckdev->cols, CROS_EC_KEYBOARD_COLS_MAX);
		return -EINVAL;
	}

	ckdev->ghost_filter = of_property_read_bool(np, "google,needs-ghost-filter");

	err = matrix_keypad_build_keymap(NULL, NULL, ckdev->rows * 2, ckdev->cols,
					 NULL, idev);
	if (err) {
		dev_err(dev, "cannot build key matrix\n");
		return err;
	}

	ckdev->row_shift = get_count_order(ckdev->cols);

	cros_ec_keyb_compute_valid_keys(ckdev);

	err = input_device_register(&ckdev->idev);
	if (err) {
		dev_err(dev, "cannot register input device\n");
		return err;
	}

	err = cros_ec_keyb_get_state(ckdev, state);
	if (err)
		dev_dbg(dev, "initial keyboard state failed: %d\n", err);
	else
		cros_ec_keyb_process(ckdev, state, ckdev->cols);

	return 0;
}

static int cros_ec_keyb_probe(struct device *dev)
{
	struct cros_ec_device *ec;
	struct cros_ec_keyb *ckdev;
	int err;

	if (!dev->parent)
		return -ENODEV;

	/*
	 * If the parent ec device has not been probed yet, defer the probe of
	 * this keyboard/button driver until later.
	 */
	ec = dev_to_cros_ec(dev->parent);
	if (!ec)
		return -EPROBE_DEFER;
	/*
	 * Even if the cros_ec_device pointer is available, still need to check
	 * if the device is fully registered before using it.
	 */
	if (!cros_ec_device_registered(ec))
		return -EPROBE_DEFER;

	ckdev = devm_kzalloc(dev, sizeof(*ckdev), GFP_KERNEL);
	if (!ckdev)
		return -ENOMEM;

	ckdev->ec = ec;
	ckdev->dev = dev;

	ckdev->idev.parent = dev;

	err = cros_ec_keyb_register_matrix(ckdev);
	if (err) {
		dev_err(dev, "cannot register matrix inputs: %d\n",
			err);
		return err;
	}

	err = poller_async_register(&ckdev->poller, "cros-ec-keyb");
	if (err)
		return err;

	poller_call_async(&ckdev->poller, CROS_EC_KEYB_POLL_NS,
			  cros_ec_keyb_poll, ckdev);

	dev_info(dev, "Chrome EC keyboard: %ux%u matrix\n",
		 ckdev->rows, ckdev->cols);

	return 0;
}

static const struct of_device_id cros_ec_keyb_of_match[] = {
	{ .compatible = "google,cros-ec-keyb" },
	{}
};
MODULE_DEVICE_TABLE(of, cros_ec_keyb_of_match);

static struct driver cros_ec_keyb_driver = {
	.probe = cros_ec_keyb_probe,
	.name = "cros-ec-keyb",
	.of_match_table = of_match_ptr(cros_ec_keyb_of_match),
};

device_platform_driver(cros_ec_keyb_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ChromeOS EC keyboard driver");
MODULE_ALIAS("platform:cros-ec-keyb");
