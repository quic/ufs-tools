// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "ufs_bsg.h"
#include "lsufs.h"

int ufs_bsg_io(int fd, struct ufs_bsg_request *req, struct ufs_bsg_reply *reply,
	       __u32 buf_len, __u8 *buf, enum bsg_ioctl_dir dir)
{
	struct sg_io_v4 sg_io = {0};
	int ret;

	sg_io.guard = 'Q';
	sg_io.protocol = BSG_PROTOCOL_SCSI;
	sg_io.subprotocol = BSG_SUB_PROTOCOL_SCSI_TRANSPORT;

	sg_io.request = (__u64)req;
	sg_io.request_len = sizeof(*req);
	sg_io.response = (__u64)reply;
	sg_io.max_response_len = sizeof(*reply);

	if (dir == BSG_IOCTL_DIR_FROM_DEV) {
	        sg_io.din_xferp = (__u64)(buf);
	        sg_io.din_xfer_len = buf_len;
	} else {
	        sg_io.dout_xferp = (__u64)(buf);
	        sg_io.dout_xfer_len = buf_len;
	}

	ret = ioctl(fd, SG_IO, &sg_io);
	if (ret)
		pr_err("%s: Error from sg_io ioctl (return value: %d, error no: %d)",
				__func__, ret, errno);

	if (sg_io.info || reply->result) {
		pr_err("Error from sg_io - device_status: 0x%x, transport_status: 0x%x, driver_status: 0x%x, bsg reply result: 0x%x\n",
				sg_io.device_status, sg_io.transport_status, sg_io.driver_status,
				reply->result);
		ret = -EIO;
	}

	return ret;
}
