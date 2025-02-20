// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __UFS_BSG_H__
#define __UFS_BSG_H__

#ifndef SG_IO
#define SG_IO 0x2285
#endif

enum bsg_ioctl_dir {
	BSG_IOCTL_DIR_TO_DEV,
	BSG_IOCTL_DIR_FROM_DEV,
};

#define UPIU_RSP_CODE_OFFSET		8

enum {
	MASK_SCSI_STATUS		= 0xFF,
	MASK_TASK_RESPONSE              = 0xFF00,
	MASK_RSP_UPIU_RESULT            = 0xFFFF,
	MASK_QUERY_DATA_SEG_LEN         = 0xFFFF,
	MASK_RSP_UPIU_DATA_SEG_LEN	= 0xFFFF,
	MASK_RSP_EXCEPTION_EVENT        = 0x10000,
};

#ifndef SCSI_BSG_UFS_H
#include <linux/types.h>

#define UFS_CDB_SIZE	16
/* uic commands are 4DW long, per UFSHCI V2.1 paragraph 5.6.1 */
#define UIC_CMD_SIZE (sizeof(__u32) * 4)

enum ufs_bsg_msg_code {
	UPIU_TRANSACTION_UIC_CMD = 0x1F,
	UPIU_TRANSACTION_ARPMB_CMD,
};

/* UFS RPMB Request Message Types */
enum ufs_rpmb_op_type {
	UFS_RPMB_WRITE_KEY		= 0x01,
	UFS_RPMB_READ_CNT		= 0x02,
	UFS_RPMB_WRITE			= 0x03,
	UFS_RPMB_READ			= 0x04,
	UFS_RPMB_READ_RESP		= 0x05,
	UFS_RPMB_SEC_CONF_WRITE		= 0x06,
	UFS_RPMB_SEC_CONF_READ		= 0x07,
	UFS_RPMB_PURGE_ENABLE		= 0x08,
	UFS_RPMB_PURGE_STATUS_READ	= 0x09,
};

/**
 * struct utp_upiu_header - UPIU header structure
 * @dword_0: UPIU header DW-0
 * @dword_1: UPIU header DW-1
 * @dword_2: UPIU header DW-2
 */
struct utp_upiu_header {
	__be32 dword_0;
	__be32 dword_1;
	__be32 dword_2;
};

/**
 * struct utp_upiu_query - upiu request buffer structure for
 * query request.
 * @opcode: command to perform B-0
 * @idn: a value that indicates the particular type of data B-1
 * @index: Index to further identify data B-2
 * @selector: Index to further identify data B-3
 * @reserved_osf: spec reserved field B-4,5
 * @length: number of descriptor bytes to read/write B-6,7
 * @value: Attribute value to be written DW-5
 * @reserved: spec reserved DW-6,7
 */
struct utp_upiu_query {
	__u8 opcode;
	__u8 idn;
	__u8 index;
	__u8 selector;
	__be16 reserved_osf;
	__be16 length;
	__be32 value;
	__be32 reserved[2];
};

/**
 * struct utp_upiu_query_v4_0 - upiu request buffer structure for
 * query request >= UFS 4.0 spec.
 * @opcode: command to perform B-0
 * @idn: a value that indicates the particular type of data B-1
 * @index: Index to further identify data B-2
 * @selector: Index to further identify data B-3
 * @osf4: spec field B-5
 * @osf5: spec field B 6,7
 * @osf6: spec field DW 8,9
 * @osf7: spec field DW 10,11
 */
struct utp_upiu_query_v4_0 {
	__u8 opcode;
	__u8 idn;
	__u8 index;
	__u8 selector;
	__u8 osf3;
	__u8 osf4;
	__be16 osf5;
	__be32 osf6;
	__be32 osf7;
	__be32 reserved;
};

struct utp_upiu_query_attr {
	__u8 opcode;
	__u8 idn;
	__u8 index;
	__u8 selector;
	__be64 value;
} __attribute__((__packed__));

/**
 * struct utp_upiu_cmd - Command UPIU structure
 * @data_transfer_len: Data Transfer Length DW-3
 * @cdb: Command Descriptor Block CDB DW-4 to DW-7
 */
struct utp_upiu_cmd {
	__be32 exp_data_transfer_len;
	__u8 cdb[UFS_CDB_SIZE];
};

/**
 * struct utp_upiu_req - general upiu request structure
 * @header:UPIU header structure DW-0 to DW-2
 * @sc: fields structure for scsi command DW-3 to DW-7
 * @qr: fields structure for query request DW-3 to DW-7
 * @uc: use utp_upiu_query to host the 4 dwords of uic command
 */
struct utp_upiu_req {
	struct utp_upiu_header header;
	union {
		struct utp_upiu_cmd		sc;
		struct utp_upiu_query		qr;
		struct utp_upiu_query		uc;
	};
};

struct ufs_arpmb_meta {
	__be16	req_resp_type;
	__u8	nonce[16];
	__be32	write_counter;
	__be16	addr_lun;
	__be16	block_count;
	__be16	result;
} __attribute__((__packed__));

struct ufs_ehs {
	__u8	length;
	__u8	ehs_type;
	__be16	ehssub_type;
	struct ufs_arpmb_meta meta;
	__u8	mac_key[32];
} __attribute__((__packed__));

/* request (CDB) structure of the sg_io_v4 */
struct ufs_bsg_request {
	__u32 msgcode;
	struct utp_upiu_req upiu_req;
};

/* response (request sense data) structure of the sg_io_v4 */
struct ufs_bsg_reply {
	/*
	 * The completion result. Result exists in two forms:
	 * if negative, it is an -Exxx system errno value. There will
	 * be no further reply information supplied.
	 * else, it's the 4-byte scsi error result, with driver, host,
	 * msg and status fields. The per-msgcode reply structure
	 * will contain valid data.
	 */
	int result;

	/* If there was reply_payload, how much was received? */
	__u32 reply_payload_rcv_len;

	struct utp_upiu_req upiu_rsp;
};

struct ufs_rpmb_request {
	struct ufs_bsg_request bsg_request;
	struct ufs_ehs ehs_req;
};

struct ufs_rpmb_reply {
	struct ufs_bsg_reply bsg_reply;
	struct ufs_ehs ehs_rsp;
};
#endif /* SCSI_BSG_UFS_H */

#ifndef _UAPIBSG_H
#include <linux/types.h>

#define BSG_PROTOCOL_SCSI		0

#define BSG_SUB_PROTOCOL_SCSI_CMD	0
#define BSG_SUB_PROTOCOL_SCSI_TMF	1
#define BSG_SUB_PROTOCOL_SCSI_TRANSPORT	2

/*
 * For flag constants below:
 * sg.h sg_io_hdr also has bits defined for it's flags member. These
 * two flag values (0x10 and 0x20) have the same meaning in sg.h . For
 * bsg the BSG_FLAG_Q_AT_HEAD flag is ignored since it is the deafult.
 */
#define BSG_FLAG_Q_AT_TAIL 0x10 /* default is Q_AT_HEAD */
#define BSG_FLAG_Q_AT_HEAD 0x20

struct sg_io_v4 {
	__s32 guard;		/* [i] 'Q' to differentiate from v3 */
	__u32 protocol;		/* [i] 0 -> SCSI , .... */
	__u32 subprotocol;	/* [i] 0 -> SCSI command, 1 -> SCSI task
				   management function, .... */

	__u32 request_len;	/* [i] in bytes */
	__u64 request;		/* [i], [*i] {SCSI: cdb} */
	__u64 request_tag;	/* [i] {SCSI: task tag (only if flagged)} */
	__u32 request_attr;	/* [i] {SCSI: task attribute} */
	__u32 request_priority;	/* [i] {SCSI: task priority} */
	__u32 request_extra;	/* [i] {spare, for padding} */
	__u32 max_response_len;	/* [i] in bytes */
	__u64 response;		/* [i], [*o] {SCSI: (auto)sense data} */

	/* "dout_": data out (to device); "din_": data in (from device) */
	__u32 dout_iovec_count;	/* [i] 0 -> "flat" dout transfer else
				   dout_xfer points to array of iovec */
	__u32 dout_xfer_len;	/* [i] bytes to be transferred to device */
	__u32 din_iovec_count;	/* [i] 0 -> "flat" din transfer */
	__u32 din_xfer_len;	/* [i] bytes to be transferred from device */
	__u64 dout_xferp;	/* [i], [*i] */
	__u64 din_xferp;	/* [i], [*o] */

	__u32 timeout;		/* [i] units: millisecond */
	__u32 flags;		/* [i] bit mask */
	__u64 usr_ptr;		/* [i->o] unused internally */
	__u32 spare_in;		/* [i] */

	__u32 driver_status;	/* [o] 0 -> ok */
	__u32 transport_status;	/* [o] 0 -> ok */
	__u32 device_status;	/* [o] {SCSI: command completion status} */
	__u32 retry_delay;	/* [o] {SCSI: status auxiliary information} */
	__u32 info;		/* [o] additional information */
	__u32 duration;		/* [o] time to complete, in milliseconds */
	__u32 response_len;	/* [o] bytes of response actually written */
	__s32 din_resid;	/* [o] din_xfer_len - actual_din_xfer_len */
	__s32 dout_resid;	/* [o] dout_xfer_len - actual_dout_xfer_len */
	__u64 generated_tag;	/* [o] {SCSI: transport generated task tag} */
	__u32 spare_out;	/* [o] */

	__u32 padding;
};
#endif /* _UAPIBSG_H */

int ufs_bsg_io(int fd, struct ufs_bsg_request *req, struct ufs_bsg_reply *reply,
	       __u32 buf_len, __u8 *buf, enum bsg_ioctl_dir dir);
#endif /* __UFS_BSG_H__ */
