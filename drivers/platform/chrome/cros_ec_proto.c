// SPDX-License-Identifier: GPL-2.0
// SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/platform/chrome/cros_ec_proto.c?id=56cb557279d70397cefb497e0f06bdd6fd685f8e
// ChromeOS EC communication protocol helper functions
//
// Copyright (C) 2015 Google, Inc

#include <linux/cleanup.h>
#include <clock.h>
#include <linux/device.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#define EC_COMMAND_RETRIES	50
#define RWSIG_CONTINUE_RETRIES	8
#define RWSIG_CONTINUE_MAX_ERRORS_IN_ROW	3

#define trace_cros_ec_request_start(msg)	((void)0)
#define trace_cros_ec_request_done(msg, ret)	((void)0)

static const int cros_ec_error_map[] = {
	[EC_RES_INVALID_COMMAND] = -EOPNOTSUPP,
	[EC_RES_ERROR] = -EIO,
	[EC_RES_INVALID_PARAM] = -EINVAL,
	[EC_RES_ACCESS_DENIED] = -EACCES,
	[EC_RES_INVALID_RESPONSE] = -EPROTO,
	[EC_RES_INVALID_VERSION] = -ENOPROTOOPT,
	[EC_RES_INVALID_CHECKSUM] = -EBADMSG,
	[EC_RES_IN_PROGRESS] = -EINPROGRESS,
	[EC_RES_UNAVAILABLE] = -ENODATA,
	[EC_RES_TIMEOUT] = -ETIMEDOUT,
	[EC_RES_OVERFLOW] = -EOVERFLOW,
	[EC_RES_INVALID_HEADER] = -EBADR,
	[EC_RES_REQUEST_TRUNCATED] = -EBADR,
	[EC_RES_RESPONSE_TOO_BIG] = -EFBIG,
	[EC_RES_BUS_ERROR] = -EFAULT,
	[EC_RES_BUSY] = -EBUSY,
	[EC_RES_INVALID_HEADER_VERSION] = -EBADMSG,
	[EC_RES_INVALID_HEADER_CRC] = -EBADMSG,
	[EC_RES_INVALID_DATA_CRC] = -EBADMSG,
	[EC_RES_DUP_UNAVAILABLE] = -ENODATA,
};

static int cros_ec_map_error(uint32_t result)
{
	int ret = 0;

	if (result != EC_RES_SUCCESS) {
		if (result < ARRAY_SIZE(cros_ec_error_map) && cros_ec_error_map[result])
			ret = cros_ec_error_map[result];
		else
			ret = -EPROTO;
	}

	return ret;
}

static int prepare_tx(struct cros_ec_device *ec_dev,
		      struct cros_ec_command *msg)
{
	struct ec_host_request *request;
	u8 *out;
	int i;
	u8 csum = 0;

	if (msg->outsize + sizeof(*request) > ec_dev->dout_size)
		return -EINVAL;

	out = ec_dev->dout;
	request = (struct ec_host_request *)out;
	request->struct_version = EC_HOST_REQUEST_VERSION;
	request->checksum = 0;
	request->command = msg->command;
	request->command_version = msg->version;
	request->reserved = 0;
	request->data_len = msg->outsize;

	for (i = 0; i < sizeof(*request); i++)
		csum += out[i];

	/* Copy data and update checksum */
	memcpy(out + sizeof(*request), msg->data, msg->outsize);
	for (i = 0; i < msg->outsize; i++)
		csum += msg->data[i];

	request->checksum = -csum;

	return sizeof(*request) + msg->outsize;
}

static int prepare_tx_legacy(struct cros_ec_device *ec_dev,
			     struct cros_ec_command *msg)
{
	u8 *out;
	u8 csum;
	int i;

	if (msg->outsize > EC_PROTO2_MAX_PARAM_SIZE)
		return -EINVAL;

	out = ec_dev->dout;
	out[0] = EC_CMD_VERSION0 + msg->version;
	out[1] = msg->command;
	out[2] = msg->outsize;
	csum = out[0] + out[1] + out[2];
	for (i = 0; i < msg->outsize; i++)
		csum += out[EC_MSG_TX_HEADER_BYTES + i] = msg->data[i];
	out[EC_MSG_TX_HEADER_BYTES + msg->outsize] = csum;

	return EC_MSG_TX_PROTO_BYTES + msg->outsize;
}

static int cros_ec_xfer_command(struct cros_ec_device *ec_dev, struct cros_ec_command *msg)
{
	int ret;
	int (*xfer_fxn)(struct cros_ec_device *ec, struct cros_ec_command *msg);

	if (ec_dev->proto_version > 2)
		xfer_fxn = ec_dev->pkt_xfer;
	else
		xfer_fxn = ec_dev->cmd_xfer;

	if (!xfer_fxn) {
		/*
		 * This error can happen if a communication error happened and
		 * the EC is trying to use protocol v2, on an underlying
		 * communication mechanism that does not support v2.
		 */
		dev_err_once(ec_dev->dev, "missing EC transfer API, cannot send command\n");
		return -EIO;
	}

	trace_cros_ec_request_start(msg);
	ret = (*xfer_fxn)(ec_dev, msg);
	trace_cros_ec_request_done(msg, ret);

	return ret;
}

static int cros_ec_wait_until_complete(struct cros_ec_device *ec_dev, uint32_t *result)
{
	DEFINE_RAW_FLEX(struct cros_ec_command, msg, data,
			sizeof(struct ec_response_get_comms_status));
	struct ec_response_get_comms_status *status =
			(struct ec_response_get_comms_status *)msg->data;
	int ret = 0, i;

	msg->version = 0;
	msg->command = EC_CMD_GET_COMMS_STATUS;
	msg->insize = sizeof(*status);
	msg->outsize = 0;

	/* Query the EC's status until it's no longer busy or we encounter an error. */
	for (i = 0; i < EC_COMMAND_RETRIES; ++i) {
		mdelay(10);

		ret = cros_ec_xfer_command(ec_dev, msg);
		if (ret == -EAGAIN)
			continue;
		if (ret < 0)
			return ret;

		*result = msg->result;
		if (msg->result != EC_RES_SUCCESS)
			return ret;

		if (ret == 0) {
			ret = -EPROTO;
			break;
		}

		if (!(status->flags & EC_COMMS_STATUS_PROCESSING))
			return ret;
	}

	if (i >= EC_COMMAND_RETRIES)
		ret = -EAGAIN;

	return ret;
}

static int cros_ec_send_command(struct cros_ec_device *ec_dev, struct cros_ec_command *msg)
{
	int ret = cros_ec_xfer_command(ec_dev, msg);

	if (msg->result == EC_RES_IN_PROGRESS)
		ret = cros_ec_wait_until_complete(ec_dev, &msg->result);

	return ret;
}

/**
 * cros_ec_prepare_tx() - Prepare an outgoing message in the output buffer.
 * @ec_dev: Device to register.
 * @msg: Message to write.
 *
 * This is used by all ChromeOS EC drivers to prepare the outgoing message
 * according to different protocol versions.
 *
 * Return: number of prepared bytes on success or negative error code.
 */
int cros_ec_prepare_tx(struct cros_ec_device *ec_dev,
		       struct cros_ec_command *msg)
{
	if (ec_dev->proto_version > 2)
		return prepare_tx(ec_dev, msg);

	return prepare_tx_legacy(ec_dev, msg);
}
EXPORT_SYMBOL(cros_ec_prepare_tx);

/**
 * cros_ec_check_result() - Check ec_msg->result.
 * @ec_dev: EC device.
 * @msg: Message to check.
 *
 * This is used by ChromeOS EC drivers to check the ec_msg->result for
 * EC_RES_IN_PROGRESS and to warn about them.
 *
 * The function should not check for furthermore error codes.  Otherwise,
 * it would break the ABI.
 *
 * Return: -EAGAIN if ec_msg->result == EC_RES_IN_PROGRESS.  Otherwise, 0.
 */
int cros_ec_check_result(struct cros_ec_device *ec_dev,
			 struct cros_ec_command *msg)
{
	switch (msg->result) {
	case EC_RES_SUCCESS:
		return 0;
	case EC_RES_IN_PROGRESS:
		dev_dbg(ec_dev->dev, "command 0x%02x in progress\n",
			msg->command);
		return -EAGAIN;
	default:
		dev_dbg(ec_dev->dev, "command 0x%02x returned %d\n",
			msg->command, msg->result);
		return 0;
	}
}
EXPORT_SYMBOL(cros_ec_check_result);

int cros_ec_rwsig_continue(struct cros_ec_device *ec_dev)
{
	struct cros_ec_command *msg;
	struct ec_params_rwsig_action *rwsig_action;
	int ret = 0;
	int error_count = 0;

	ec_dev->proto_version = 3;

	msg = kmalloc(sizeof(*msg) + sizeof(*rwsig_action), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->version = 0;
	msg->command = EC_CMD_RWSIG_ACTION;
	msg->insize = 0;
	msg->outsize = sizeof(*rwsig_action);

	rwsig_action = (struct ec_params_rwsig_action *)msg->data;
	rwsig_action->action = RWSIG_ACTION_CONTINUE;

	for (int i = 0; i < RWSIG_CONTINUE_RETRIES; i++) {
		ret = cros_ec_send_command(ec_dev, msg);

		if (ret < 0) {
			if (++error_count >= RWSIG_CONTINUE_MAX_ERRORS_IN_ROW)
				break;
		} else if (msg->result == EC_RES_INVALID_COMMAND) {
			/*
			 * If EC_RES_INVALID_COMMAND is retured, it means RWSIG
			 * is not supported or EC is already in RW, so there is
			 * nothing left to do.
			 */
			break;
		} else if (msg->result != EC_RES_SUCCESS) {
			/* Unexpected command error. */
			ret = cros_ec_map_error(msg->result);
			break;
		} else {
			/*
			 * The EC_CMD_RWSIG_ACTION succeed. Send the command
			 * more times, to make sure EC is in RW. A following
			 * command can timeout, because EC may need some time to
			 * initialize after jump to RW.
			 */
			error_count = 0;
		}

		if (ret != -ETIMEDOUT)
			mdelay(100);
	}

	kfree(msg);

	return ret;
}
EXPORT_SYMBOL(cros_ec_rwsig_continue);

static int cros_ec_get_proto_info(struct cros_ec_device *ec_dev, int devidx)
{
	struct cros_ec_command *msg;
	struct ec_response_get_protocol_info *info;
	int ret, mapped;

	ec_dev->proto_version = 3;
	if (devidx > 0)
		ec_dev->max_passthru = 0;

	msg = kzalloc(sizeof(*msg) + sizeof(*info), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->command = EC_CMD_PASSTHRU_OFFSET(devidx) | EC_CMD_GET_PROTOCOL_INFO;
	msg->insize = sizeof(*info);

	ret = cros_ec_send_command(ec_dev, msg);

	if (ret < 0) {
		dev_dbg(ec_dev->dev,
			"failed to check for EC[%d] protocol version: %d\n",
			devidx, ret);
		goto exit;
	}

	mapped = cros_ec_map_error(msg->result);
	if (mapped) {
		ret = mapped;
		goto exit;
	}

	if (ret == 0) {
		ret = -EPROTO;
		goto exit;
	}

	info = (struct ec_response_get_protocol_info *)msg->data;

	switch (devidx) {
	case CROS_EC_DEV_EC_INDEX:
		ec_dev->max_request = info->max_request_packet_size -
						sizeof(struct ec_host_request);
		ec_dev->max_response = info->max_response_packet_size -
						sizeof(struct ec_host_response);
		ec_dev->proto_version = min(EC_HOST_REQUEST_VERSION,
					    fls(info->protocol_versions) - 1);
		ec_dev->din_size = info->max_response_packet_size + EC_MAX_RESPONSE_OVERHEAD;
		ec_dev->dout_size = info->max_request_packet_size + EC_MAX_REQUEST_OVERHEAD;

		dev_dbg(ec_dev->dev, "using proto v%u\n", ec_dev->proto_version);
		break;
	case CROS_EC_DEV_PD_INDEX:
		ec_dev->max_passthru = info->max_request_packet_size -
						sizeof(struct ec_host_request);

		dev_dbg(ec_dev->dev, "found PD chip\n");
		break;
	default:
		dev_dbg(ec_dev->dev, "unknown passthru index: %d\n", devidx);
		break;
	}

	ret = 0;
exit:
	kfree(msg);
	return ret;
}

static int cros_ec_get_proto_info_legacy(struct cros_ec_device *ec_dev)
{
	struct cros_ec_command *msg;
	struct ec_params_hello *params;
	struct ec_response_hello *response;
	int ret, mapped;

	ec_dev->proto_version = 2;

	msg = kzalloc(sizeof(*msg) + max(sizeof(*params), sizeof(*response)), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->command = EC_CMD_HELLO;
	msg->insize = sizeof(*response);
	msg->outsize = sizeof(*params);

	params = (struct ec_params_hello *)msg->data;
	params->in_data = 0xa0b0c0d0;

	ret = cros_ec_send_command(ec_dev, msg);
	if (ret < 0) {
		dev_dbg(ec_dev->dev, "EC failed to respond to v2 hello: %d\n", ret);
		goto exit;
	}

	mapped = cros_ec_map_error(msg->result);
	if (mapped) {
		ret = mapped;
		dev_err(ec_dev->dev, "EC responded to v2 hello with error: %d\n", msg->result);
		goto exit;
	}

	if (ret == 0) {
		ret = -EPROTO;
		goto exit;
	}

	response = (struct ec_response_hello *)msg->data;
	if (response->out_data != 0xa1b2c3d4) {
		dev_err(ec_dev->dev,
			"EC responded to v2 hello with bad result: %u\n",
			response->out_data);
		ret = -EBADMSG;
		goto exit;
	}

	ec_dev->max_request = EC_PROTO2_MAX_PARAM_SIZE;
	ec_dev->max_response = EC_PROTO2_MAX_PARAM_SIZE;
	ec_dev->max_passthru = 0;
	ec_dev->pkt_xfer = NULL;
	ec_dev->din_size = EC_PROTO2_MSG_BYTES;
	ec_dev->dout_size = EC_PROTO2_MSG_BYTES;

	dev_dbg(ec_dev->dev, "falling back to proto v2\n");
	ret = 0;
exit:
	kfree(msg);
	return ret;
}

/**
 * cros_ec_query_all() -  Query the protocol version supported by the
 *         ChromeOS EC.
 * @ec_dev: Device to register.
 *
 * Return: 0 on success or negative error code.
 */
int cros_ec_query_all(struct cros_ec_device *ec_dev)
{
	struct device *dev = ec_dev->dev;
	int ret;

	/* First try sending with proto v3. */
	if (!cros_ec_get_proto_info(ec_dev, CROS_EC_DEV_EC_INDEX)) {
		/* Check for PD. */
		cros_ec_get_proto_info(ec_dev, CROS_EC_DEV_PD_INDEX);
	} else {
		/* Try querying with a v2 hello message. */
		ret = cros_ec_get_proto_info_legacy(ec_dev);
		if (ret) {
			/*
			 * It's possible for a test to occur too early when
			 * the EC isn't listening. If this happens, we'll
			 * test later when the first command is run.
			 */
			ec_dev->proto_version = EC_PROTO_VERSION_UNKNOWN;
			dev_dbg(ec_dev->dev, "EC query failed: %d\n", ret);
			return ret;
		}
	}

	devm_kfree(dev, ec_dev->din);
	devm_kfree(dev, ec_dev->dout);

	ec_dev->din = devm_kzalloc(dev, ec_dev->din_size, GFP_KERNEL);
	if (!ec_dev->din) {
		ret = -ENOMEM;
		goto exit;
	}

	ec_dev->dout = devm_kzalloc(dev, ec_dev->dout_size, GFP_KERNEL);
	if (!ec_dev->dout) {
		devm_kfree(dev, ec_dev->din);
		ret = -ENOMEM;
		goto exit;
	}

	ret = 0;

exit:
	return ret;
}
EXPORT_SYMBOL(cros_ec_query_all);

/**
 * cros_ec_cmd_xfer() - Send a command to the ChromeOS EC.
 * @ec_dev: EC device.
 * @msg: Message to write.
 *
 * Call this to send a command to the ChromeOS EC. This should be used instead
 * of calling the EC's cmd_xfer() callback directly. This function does not
 * convert EC command execution error codes to Linux error codes. Most
 * in-kernel users will want to use cros_ec_cmd_xfer_status() instead since
 * that function implements the conversion.
 *
 * Return:
 * >0 - EC command was executed successfully. The return value is the number
 *      of bytes returned by the EC (excluding the header).
 * =0 - EC communication was successful. EC command execution results are
 *      reported in msg->result. The result will be EC_RES_SUCCESS if the
 *      command was executed successfully or report an EC command execution
 *      error.
 * <0 - EC communication error. Return value is the Linux error code.
 */
int cros_ec_cmd_xfer(struct cros_ec_device *ec_dev, struct cros_ec_command *msg)
{
	int ret;

	mutex_lock(&ec_dev->lock);
	if (ec_dev->proto_version == EC_PROTO_VERSION_UNKNOWN) {
		ret = cros_ec_query_all(ec_dev);
		if (ret) {
			dev_err(ec_dev->dev,
				"EC version unknown and query failed; aborting command\n");
			mutex_unlock(&ec_dev->lock);
			return ret;
		}
	}

	if (msg->insize > ec_dev->max_response) {
		dev_dbg(ec_dev->dev, "clamping message receive buffer\n");
		msg->insize = ec_dev->max_response;
	}

	if (msg->command < EC_CMD_PASSTHRU_OFFSET(CROS_EC_DEV_PD_INDEX)) {
		if (msg->outsize > ec_dev->max_request) {
			dev_err(ec_dev->dev,
				"request of size %u is too big (max: %u)\n",
				msg->outsize,
				ec_dev->max_request);
			mutex_unlock(&ec_dev->lock);
			return -EMSGSIZE;
		}
	} else {
		if (msg->outsize > ec_dev->max_passthru) {
			dev_err(ec_dev->dev,
				"passthru rq of size %u is too big (max: %u)\n",
				msg->outsize,
				ec_dev->max_passthru);
			mutex_unlock(&ec_dev->lock);
			return -EMSGSIZE;
		}
	}

	ret = cros_ec_send_command(ec_dev, msg);
	mutex_unlock(&ec_dev->lock);

	return ret;
}
EXPORT_SYMBOL(cros_ec_cmd_xfer);

/**
 * cros_ec_cmd_xfer_status() - Send a command to the ChromeOS EC.
 * @ec_dev: EC device.
 * @msg: Message to write.
 *
 * Call this to send a command to the ChromeOS EC. This should be used instead of calling the EC's
 * cmd_xfer() callback directly. It returns success status only if both the command was transmitted
 * successfully and the EC replied with success status.
 *
 * Return:
 * >=0 - The number of bytes transferred.
 * <0 - Linux error code
 */
int cros_ec_cmd_xfer_status(struct cros_ec_device *ec_dev,
			    struct cros_ec_command *msg)
{
	int ret, mapped;

	ret = cros_ec_cmd_xfer(ec_dev, msg);
	if (ret < 0)
		return ret;

	mapped = cros_ec_map_error(msg->result);
	if (mapped) {
		dev_dbg(ec_dev->dev, "Command result (err: %d [%d])\n",
			msg->result, mapped);
		ret = mapped;
	}

	return ret;
}
EXPORT_SYMBOL(cros_ec_cmd_xfer_status);

/**
 * cros_ec_cmd - Send a command to the EC.
 *
 * @ec_dev: EC device
 * @version: EC command version
 * @command: EC command
 * @outdata: EC command output data
 * @outsize: Size of outdata
 * @indata: EC command input data
 * @insize: Size of indata
 *
 * Return: >= 0 on success, negative error number on failure.
 */
int cros_ec_cmd(struct cros_ec_device *ec_dev,
		unsigned int version,
		int command,
		const void *outdata,
		size_t outsize,
		void *indata,
		size_t insize)
{
	struct cros_ec_command *msg;
	int ret;

	msg = kzalloc(sizeof(*msg) + max(insize, outsize), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->version = version;
	msg->command = command;
	msg->outsize = outsize;
	msg->insize = insize;

	if (outsize)
		memcpy(msg->data, outdata, outsize);

	ret = cros_ec_cmd_xfer_status(ec_dev, msg);
	if (ret < 0)
		goto error;

	if (insize)
		memcpy(indata, msg->data, insize);
error:
	kfree(msg);
	return ret;
}
EXPORT_SYMBOL_GPL(cros_ec_cmd);

/**
 * cros_ec_device_registered - Return if the ec_dev is registered.
 *
 * @ec_dev: EC device
 *
 * Return: true if registered.  Otherwise, false.
 */
bool cros_ec_device_registered(struct cros_ec_device *ec_dev)
{
	guard(mutex)(&ec_dev->lock);

	return ec_dev->registered;
}
EXPORT_SYMBOL_GPL(cros_ec_device_registered);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ChromeOS EC communication protocol helpers");
