/* SPDX-License-Identifier: GPL-2.0 */
/* SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/linux/platform_data/cros_ec_proto.h?id=56cb557279d70397cefb497e0f06bdd6fd685f8e */
/*
 * ChromeOS Embedded Controller protocol interface.
 *
 * Copyright (C) 2012 Google, Inc
 */

#ifndef __LINUX_CROS_EC_PROTO_H
#define __LINUX_CROS_EC_PROTO_H

#include <linux/device.h>
#include <linux/mutex.h>

#include <linux/platform_data/cros_ec_commands.h>

#define CROS_EC_DEV_NAME	"cros_ec"
#define CROS_EC_DEV_PD_NAME	"cros_pd"

#define CROS_EC_DEV_EC_INDEX 0
#define CROS_EC_DEV_PD_INDEX 1

/*
 * The EC is unresponsive for a time after a reboot command.  Add a
 * simple delay to make sure that the bus stays locked.
 */
#define EC_REBOOT_DELAY_MS		50

/*
 * Max bus-specific overhead incurred by request/responses.
 *
 * Request:
 * - I2C requires 1 byte (see struct ec_host_request_i2c).
 * - ISHTP requires 4 bytes (see struct cros_ish_out_msg).
 *
 * Response:
 * - I2C requires 2 bytes (see struct ec_host_response_i2c).
 * - ISHTP requires 4 bytes (see struct cros_ish_in_msg).
 * - SPI requires 32 bytes (see EC_MSG_PREAMBLE_COUNT).
 */
#define EC_PROTO_VERSION_UNKNOWN	0
#define EC_MAX_REQUEST_OVERHEAD		4
#define EC_MAX_RESPONSE_OVERHEAD	32

/*
 * Command interface between EC and AP, for LPC, I2C and SPI interfaces.
 */
enum {
	EC_MSG_TX_HEADER_BYTES	= 3,
	EC_MSG_TX_TRAILER_BYTES	= 1,
	EC_MSG_TX_PROTO_BYTES	= EC_MSG_TX_HEADER_BYTES +
				  EC_MSG_TX_TRAILER_BYTES,
	EC_MSG_RX_PROTO_BYTES	= 3,

	/* Max length of messages for proto 2*/
	EC_PROTO2_MSG_BYTES	= EC_PROTO2_MAX_PARAM_SIZE +
				  EC_MSG_TX_PROTO_BYTES,

	EC_MAX_MSG_BYTES	= 64 * 1024,
};

/**
 * struct cros_ec_command - Information about a ChromeOS EC command.
 * @version: Command version number (often 0).
 * @command: Command to send (EC_CMD_...).
 * @outsize: Outgoing length in bytes.
 * @insize: Max number of bytes to accept from the EC.
 * @result: EC's response to the command (separate from communication failure).
 * @data: Where to put the incoming data from EC and outgoing data to EC.
 */
struct cros_ec_command {
	uint32_t version;
	uint32_t command;
	uint32_t outsize;
	uint32_t insize;
	uint32_t result;
	uint8_t data[];
};

struct device;
struct slice;

/**
 * struct cros_ec_device - Information about a ChromeOS EC device.
 * @phys_name: Name of physical comms layer (e.g. 'i2c-4').
 * @dev: Device pointer for physical comms device
 * @ec: Virtual Chrome EC device
 * @max_request: Max size of message requested.
 * @max_response: Max size of message response.
 * @max_passthru: Max sice of passthru message.
 * @proto_version: The protocol version used for this device.
 * @priv: Private data.
 * @din: Input buffer (for data from EC). This buffer will always be
 *       dword-aligned and include enough space for up to 7 word-alignment
 *       bytes also, so we can ensure that the body of the message is always
 *       dword-aligned (64-bit). We use this alignment to keep ARM and x86
 *       happy. Probably word alignment would be OK, there might be a small
 *       performance advantage to using dword.
 * @dout: Output buffer (for data to EC). This buffer will always be
 *        dword-aligned and include enough space for up to 7 word-alignment
 *        bytes also, so we can ensure that the body of the message is always
 *        dword-aligned (64-bit). We use this alignment to keep ARM and x86
 *        happy. Probably word alignment would be OK, there might be a small
 *        performance advantage to using dword.
 * @din_size: Size of din buffer to allocate (zero to use static din).
 * @dout_size: Size of dout buffer to allocate (zero to use static dout).
 * @registered: True if this device had been registered.
 * @cmd_xfer: Send command to EC and get response.
 *            Returns the number of bytes received if the communication
 *            succeeded, but that doesn't mean the EC was happy with the
 *            command. The caller should check msg.result for the EC's result
 *            code.
 * @pkt_xfer: Send packet to EC and get response.
 * @lock: One transaction at a time.
 * @slice: Optional transport slice used to avoid reentrant polling.
 */
struct cros_ec_device {
	const char *phys_name;
	struct device *dev;
	struct device *ec;

	u16 max_request;
	u16 max_response;
	u16 max_passthru;
	u16 proto_version;
	void *priv;
	u8 *din;
	u8 *dout;
	int din_size;
	int dout_size;
	bool registered;
	int (*cmd_xfer)(struct cros_ec_device *ec,
			struct cros_ec_command *msg);
	int (*pkt_xfer)(struct cros_ec_device *ec,
			struct cros_ec_command *msg);
	struct mutex lock;
	struct slice *slice;
};

int cros_ec_prepare_tx(struct cros_ec_device *ec_dev,
		       struct cros_ec_command *msg);

int cros_ec_check_result(struct cros_ec_device *ec_dev,
			 struct cros_ec_command *msg);

int cros_ec_cmd_xfer(struct cros_ec_device *ec_dev,
		     struct cros_ec_command *msg);

int cros_ec_cmd_xfer_status(struct cros_ec_device *ec_dev,
			    struct cros_ec_command *msg);

int cros_ec_rwsig_continue(struct cros_ec_device *ec_dev);

int cros_ec_query_all(struct cros_ec_device *ec_dev);

int cros_ec_cmd(struct cros_ec_device *ec_dev, unsigned int version, int command, const void *outdata,
		    size_t outsize, void *indata, size_t insize);

bool cros_ec_device_registered(struct cros_ec_device *ec_dev);
struct cros_ec_device *dev_to_cros_ec(struct device *dev);

#endif /* __LINUX_CROS_EC_PROTO_H */
