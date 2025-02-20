// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "lsufs.h"
#include "ufs_bsg.h"
#include "upiu.h"
#include "query.h"

static char *query_short_options = "o:i:I:s:v:d:";

static struct option query_long_options[] = {
	{"opcode", required_argument, NULL, 'o'},
	{"idn", required_argument, NULL, 'i'},
	{"index", required_argument, NULL, 'I'},
	{"selector", required_argument, NULL, 's'},
	{"value", required_argument, NULL, 'v'},
	{"device", required_argument, NULL, 'd'}, /* UFS BSG device path. For example: /dev/ufs-bsg0 */
	{NULL, 0, NULL, 0}
};

static int init_opcode(struct lsufs_operation *lsufs_op)
{
	int opcode, ret;

	ret = get_value_from_cli(&opcode);
	if (ret || opcode >= QUERY_REQ_OP_MAX) {
		pr_err("Invalid opcode.\n");
		return ERROR;
	}

	lsufs_op->query_op.opcode = opcode;

	return SUCCESS;
}

static int init_idn(struct lsufs_operation *lsufs_op)
{
	int idn, ret;

	ret = get_value_from_cli(&idn);
	if (ret) {
		pr_err("Invalid IDN.\n");
		return ERROR;
	}

	lsufs_op->query_op.idn = idn;

	return SUCCESS;
}

static int init_index(struct lsufs_operation *lsufs_op)
{
	int index, ret;

	ret = get_value_from_cli(&index);
	if (ret) {
		pr_err("Invalid Index.\n");
		return ERROR;
	}

	lsufs_op->query_op.index = index;

	return SUCCESS;
}

static int init_selector(struct lsufs_operation *lsufs_op)
{
	int selector, ret;

	ret = get_value_from_cli(&selector);
	if (ret) {
		pr_err("Invalid Selector.\n");
		return ERROR;
	}

	lsufs_op->query_op.selector = selector;

	return SUCCESS;
}

static int init_attr_value(struct lsufs_operation *lsufs_op)
{
	unsigned long long value;
	int ret;

	ret = get_ull_from_cli(&value);
	if (ret) {
		pr_err("Invalid Attribute Value.\n");
		return ERROR;
	}

	lsufs_op->query_op.attr_value = value;

	return SUCCESS;
}

static int setup_query_operation(int args, char *argv[], struct lsufs_operation *lsufs_op)
{
	int i, c = 0;
	int ret = ERROR;

	while (-1 != (c = getopt_long(args, argv, query_short_options, query_long_options, &i))) {
		switch (c) {
		case 'o':
			ret = init_opcode(lsufs_op);
			break;
		case 'i':
			ret = init_idn(lsufs_op);
			break;
		case 'I':
			ret = init_index(lsufs_op);
			break;
		case 's':
			ret = init_selector(lsufs_op);
			break;
		case 'v':
			ret = init_attr_value(lsufs_op);
			break;
		case 'd':
			ret = init_device_path(lsufs_op->device_path);
			break;
		default:
			pr_err("I cannot understand, please try 'query -h'.\n");
			ret = ERROR;
			break;
		}

		if (ret)
			break;
	}

	return ret;
}

static int query_op_sanity_check(struct lsufs_operation *lsufs_op)
{
	struct query_operation *qop = &lsufs_op->query_op;

	if (lsufs_op->device_path[0] == '\0') {
		pr_err("Path to bsg device not provided.\n");
		return ERROR;
	}

	if (qop->opcode == INIT) {
		pr_err("Query request opcode is not given.\n");
		return ERROR;
	} else if (qop->opcode == QUERY_REQ_OP_WRITE_ATTR && qop->attr_value == INIT) {
		pr_err("Query write attribute needs value input.\n");
		return ERROR;
	}

	if (qop->idn == INIT) {
		pr_err("Query request idn is not given.\n");
		return ERROR;
	}

	if (qop->index == INIT) {
		pr_err("Query request index is not given.\n");
		return ERROR;
	}

	if (qop->selector == INIT) {
		pr_err("Query request selector is not given.\n");
		return ERROR;
	}

	return SUCCESS;
}

int init_query_operation(int argc, char *argv[], void *op_data)
{
	struct lsufs_operation *lsufs_op = (struct lsufs_operation *)op_data;
	int ret;

	lsufs_op->query_op.opcode = INIT;
	lsufs_op->query_op.idn = INIT;
	lsufs_op->query_op.index = INIT;
	lsufs_op->query_op.selector = INIT;
	lsufs_op->query_op.attr_value = INIT;
	lsufs_op->device_path[0] = '\0';

	ret = setup_query_operation(argc, argv, lsufs_op);

	return ret ? ret : query_op_sanity_check(lsufs_op);
}

static void compose_ufs_bsg_req(struct lsufs_operation *lsufs_op, struct ufs_bsg_request *req,
				__u16 length)
{
	struct utp_upiu_header *hdr = &req->upiu_req.header;
	struct utp_upiu_query *qr = &req->upiu_req.qr;
	struct utp_upiu_query_attr *qr_attr;
	struct query_operation *qop = &lsufs_op->query_op;

        req->msgcode = UTP_UPIU_QUERY_REQ;
        hdr->dword_0 = DWORD(UTP_UPIU_QUERY_REQ, 0, 0, 0);
        hdr->dword_1 = DWORD(0, qop->func, 0, 0);
        hdr->dword_2 = DWORD(0, 0, length >> 8, (__u8)length);
        qr->opcode = qop->opcode;
        qr->idn = qop->idn;
        qr->index = qop->index;
        qr->selector = qop->selector;

	if (qop->opcode == QUERY_REQ_OP_READ_DESC) {
		qr->length = htobe16(length);
	} else if (qop->opcode == QUERY_REQ_OP_WRITE_ATTR) {
		qr_attr = (struct utp_upiu_query_attr *)&req->upiu_req.qr;
		qr_attr->value = htobe64(qop->attr_value);
	}
}

static int send_query_command(struct lsufs_operation *lsufs_op, __u8 *buf, __u16 *buf_len,
			      struct ufs_bsg_reply *bsg_reply)
{
	struct ufs_bsg_request bsg_request = {0};
	enum bsg_ioctl_dir dir;
	__u8 query_resp;
	int ret;

	switch (lsufs_op->query_op.opcode) {
	case QUERY_REQ_OP_READ_DESC:
	case QUERY_REQ_OP_READ_ATTR:
	case QUERY_REQ_OP_READ_FLAG:
		dir = BSG_IOCTL_DIR_FROM_DEV;
		lsufs_op->query_op.func = QUERY_REQ_FUNC_STD_READ;
		break;
	default:
		dir = BSG_IOCTL_DIR_TO_DEV;
		lsufs_op->query_op.func = QUERY_REQ_FUNC_STD_WRITE;
		break;
	}

	compose_ufs_bsg_req(lsufs_op, &bsg_request, *buf_len);

	ret = ufs_bsg_io(lsufs_op->fd, &bsg_request, bsg_reply, *buf_len, buf, dir);
	if (ret) {
		pr_err("ufs_bsg_io failed for query command (%d)\n", ret);
		return ret;
	}

	query_resp = be32toh(bsg_reply->upiu_rsp.header.dword_1) & MASK_RSP_UPIU_RESULT;
	query_resp = query_resp >> UPIU_RSP_CODE_OFFSET;
	if (query_resp) {
		pr_err("query request failed with response code %d\n", query_resp);
		ret = ERROR;
	} else {
		*buf_len = be32toh(bsg_reply->upiu_rsp.header.dword_2) & MASK_QUERY_DATA_SEG_LEN;
	}

	return ret;
}

static void dump_descriptor(__u8 *desc_buf, __u16 len)
{
	__u16 i;

	if (!len)
		return;

	for (i = 0; i < len; i ++, desc_buf ++)
		printf("Offset 0x%x : 0x%x\n", i, *desc_buf);
}

static int do_query_read_descriptor(struct lsufs_operation *lsufs_op)
{
	struct query_operation *qop = &lsufs_op->query_op;
	struct ufs_bsg_reply bsg_reply = {0};
	__u16 buf_len = DESCRIPTOR_BUFFER_SIZE;
	__u8 buf[DESCRIPTOR_BUFFER_SIZE] = {0};
	int id, ret;

	id = characteristics_look_up(ufs_descriptors, qop->idn);

	ret = send_query_command(lsufs_op, buf, &buf_len, &bsg_reply);
	if (ret) {
		pr_err("Failed to query read Descriptor IDN 0x%x.\n", qop->idn);
		return ret;
	}

	printf("Descriptor IDN 0x%x - %s :\n", qop->idn, id < 0 ? "???" : ufs_descriptors[id].name);
	dump_descriptor(buf, buf_len);

	return ret;
}

int query_read_descriptor(int fd, int idn, int index, int sel, __u8 *buf, __u16 buf_len)
{
	struct lsufs_operation lsufs_op;
	struct query_operation *qop = &lsufs_op.query_op;
	struct ufs_bsg_reply bsg_reply = {0};
	int ret;

	lsufs_op.fd = fd;
	qop->opcode = QUERY_REQ_OP_READ_DESC;
	qop->idn = idn;
	qop->index = index;
	qop->selector = sel;

	ret = send_query_command(&lsufs_op, buf, &buf_len, &bsg_reply);
	if (ret)
		pr_err("Failed to query read Descriptor IDN 0x%x.\n", qop->idn);

	return ret;
}

static int do_query_read_attribute(struct lsufs_operation *lsufs_op)
{
	struct query_operation *qop = &lsufs_op->query_op;
	struct ufs_bsg_reply bsg_reply = {0};
	struct utp_upiu_query_attr *qr_attr;
	__u16 buf_len = 0;
	int id, ret;

	id = characteristics_look_up(ufs_attributes, qop->idn);

	ret = send_query_command(lsufs_op, NULL, &buf_len, &bsg_reply);
	if (ret) {
		pr_err("Failed to query read Attribute IDN 0x%x, Index 0x%x.\n",
				qop->idn, qop->index);
		return ret;
	}

	qr_attr = (struct utp_upiu_query_attr *)&bsg_reply.upiu_rsp.qr;

	printf("Attribute IDN 0x%x - %s, Index 0x%x : 0x%lx\n", qop->idn,
			id < 0 ? "???" : ufs_attributes[id].name,
			qop->index,
			be64toh(qr_attr->value));

	return ret;
}

static int do_query_write_attribute(struct lsufs_operation *lsufs_op)
{
	struct query_operation *qop = &lsufs_op->query_op;
	struct ufs_bsg_reply bsg_reply = {0};
	__u16 buf_len = 0;
	int id, ret;

	id = characteristics_look_up(ufs_attributes, qop->idn);

	ret = send_query_command(lsufs_op, NULL, &buf_len, &bsg_reply);
	printf("%s write Attribute IDN 0x%x - %s, Index 0x%x\n",
			ret ? "Failed to" : "Successfully",
			qop->idn,
			id < 0 ? "???" : ufs_attributes[id].name,
			qop->index);

	return ret;
}

static int do_query_read_flag(struct lsufs_operation *lsufs_op)
{
	struct query_operation *qop = &lsufs_op->query_op;
	struct ufs_bsg_reply bsg_reply = {0};
	__u16 buf_len = 0;
	int id, ret;

	id = characteristics_look_up(ufs_flags, qop->idn);

	ret = send_query_command(lsufs_op, NULL, &buf_len, &bsg_reply);
	if (ret) {
		pr_err("Failed to query read Flag IDX 0x%x, Index 0x%x.\n", qop->idn, qop->index);
		return ret;
	}

	printf("Flag IDN 0x%x - %s, Index 0x%x : %d\n", qop->idn,
			id < 0 ? "???" : ufs_flags[id].name,
			qop->index,
			be32toh(bsg_reply.upiu_rsp.qr.value) & 0x1);

	return ret;
}

static int do_query_manipulate_flag(struct lsufs_operation *lsufs_op)
{
	struct query_operation *qop = &lsufs_op->query_op;
	struct ufs_bsg_reply bsg_reply = {0};
	char man_str[8] = {0};
	__u16 buf_len = 0;
	int id, ret;

	switch (qop->opcode) {
	case QUERY_REQ_OP_SET_FLAG:
		snprintf(man_str, 8, "%s", "set");
		break;
	case QUERY_REQ_OP_CLEAR_FLAG:
		snprintf(man_str, 8, "%s", "clear");
		break;
	case QUERY_REQ_OP_TOGGLE_FLAG:
		snprintf(man_str, 8, "%s", "toggle");
		break;
	}

	id = characteristics_look_up(ufs_flags, qop->idn);

	ret = send_query_command(lsufs_op, NULL, &buf_len, &bsg_reply);
	printf("%s %s Flag IDN 0x%x - %s, Index 0x%x\n",
			ret ? "Failed to" : "Successfully",
			man_str,
			qop->idn,
			id < 0 ? "???" : ufs_flags[id].name,
			qop->index);

	return ret;
}

int do_query_operation(void *op_data)
{
	struct lsufs_operation *lsufs_op = (struct lsufs_operation *)op_data;
	struct query_operation *qop = &lsufs_op->query_op;
	int fd, ret = 0, flag;

	switch (lsufs_op->query_op.opcode) {
	case QUERY_REQ_OP_READ_DESC:
	case QUERY_REQ_OP_READ_ATTR:
	case QUERY_REQ_OP_READ_FLAG:
		flag = O_RDONLY;
		break;
	default:
		flag = O_RDWR;
		break;
	}

	fd = open(lsufs_op->device_path, flag);
	if (fd < 0) {
		pr_err("Filed to open file (%d).\n", fd);
		return ERROR;
	}

	lsufs_op->fd = fd;

	switch (qop->opcode) {
	case QUERY_REQ_OP_READ_DESC:
		ret = do_query_read_descriptor(lsufs_op);
		break;
	case QUERY_REQ_OP_READ_ATTR:
		ret = do_query_read_attribute(lsufs_op);
		break;
	case QUERY_REQ_OP_WRITE_ATTR:
		ret = do_query_write_attribute(lsufs_op);
		break;
	case QUERY_REQ_OP_READ_FLAG:
		ret = do_query_read_flag(lsufs_op);
		break;
	case QUERY_REQ_OP_SET_FLAG:
	case QUERY_REQ_OP_CLEAR_FLAG:
	case QUERY_REQ_OP_TOGGLE_FLAG:
		ret = do_query_manipulate_flag(lsufs_op);
		break;
	default:
		pr_err("Unsupported query request type - %d\n", qop->opcode);
		ret = ERROR;
		break;
	}

	close(fd);

	return ret;
}
