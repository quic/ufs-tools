// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "lsufs.h"
#include "ufs_bsg.h"
#include "uic.h"

static char *uic_short_options = "pli:trL:gs:d:";

static struct option uic_long_options[] = {
	{"peer", no_argument, NULL, 'p'}, /* UFS device */
	{"local", no_argument, NULL, 'l'}, /* UFS host*/
	{"id", required_argument, NULL, 'i'}, /* UniPro/M-PHY Attribute ID */
	{"TX", no_argument, NULL, 't'}, /* TX */
	{"RX", no_argument, NULL, 'r'}, /* RX */
	{"tx", no_argument, NULL, 't'}, /* TX */
	{"rx", no_argument, NULL, 'r'}, /* RX */
	{"Tx", no_argument, NULL, 't'}, /* TX */
	{"Rx", no_argument, NULL, 'r'}, /* RX */
	{"lane", required_argument, NULL, 'L'}, /* Lane */
	{"get", no_argument, NULL, 'g'}, /* dme_get */
	{"set", required_argument, NULL, 's'}, /* dme_set */
	{"GET", no_argument, NULL, 'g'}, /* dme_get */
	{"SET", required_argument, NULL, 's'}, /* dme_set */
	{"device", required_argument, NULL, 'd'}, /* UFS BSG device path. For example: /dev/ufs-bsg0 */
	{NULL, 0, NULL, 0}
};

static int init_id(struct lsufs_operation *lsufs_op)
{
	int id, ret;

	ret = get_value_from_cli(&id);
	if (ret) {
		pr_err("Invalid Attribute id.\n");
		return ERROR;
	}

	lsufs_op->uic_op.attr_id = id;

	return SUCCESS;
}

static int init_set_op(struct lsufs_operation *lsufs_op)
{
	int data, ret;

	ret = get_value_from_cli(&data);
	if (ret) {
		pr_err("UIC set needs data input.\n");
		return ERROR;
	}

	lsufs_op->uic_op.data = data;
	lsufs_op->uic_op.mode = SET;

	return SUCCESS;
}

static int init_lane(struct lsufs_operation *lsufs_op)
{
	int lane, ret;

	ret = get_value_from_cli(&lane);
	if (ret) {
		pr_err("Invalid Lane number\n");
		return ERROR;
	}

	lsufs_op->uic_op.lane = lane;

	return SUCCESS;
}

static int setup_uic_operation(int args, char *argv[], struct lsufs_operation *lsufs_op)
{
	int i, c = 0, ret = ERROR;

	while (-1 != (c = getopt_long(args, argv, uic_short_options, uic_long_options, &i))) {
		switch (c) {
		case 'g':
			lsufs_op->uic_op.mode = GET;
			ret = SUCCESS;
			break;
		case 's':
			ret = init_set_op(lsufs_op);
			break;
		case 'i':
			ret = init_id(lsufs_op);
			break;
		case 'p':
			lsufs_op->uic_op.local_peer = PEER;
			ret = SUCCESS;
			break;
		case 'l':
			lsufs_op->uic_op.local_peer = LOCAL;
			ret = SUCCESS;
			break;
		case 't':
			lsufs_op->uic_op.dir = TX;
			ret = SUCCESS;
			break;
		case 'r':
			lsufs_op->uic_op.dir = RX;
			ret = SUCCESS;
			break;
		case 'L':
			ret = init_lane(lsufs_op);
			break;
		case 'd':
			ret = init_device_path(lsufs_op->device_path);
			break;
		default:
			pr_err("I cannot understand, please try 'uic -h'.\n");
			ret = ERROR;
			break;
		}

		if (ret)
			break;
	}

	return ret;
}

static int uic_op_sanity_check(struct lsufs_operation *lsufs_op)
{
	struct uic_operation *uop = &lsufs_op->uic_op;

	if (lsufs_op->device_path[0] == '\0') {
		pr_err("Path to bsg device not provided.\n");
		return ERROR;
	}

	if (uop->attr_id == INIT) {
		pr_err("Attribute ID is not given.\n");
		return ERROR;
	}

	if (uop->mode == INIT) {
		pr_err("get/set is not given.\n");
		return ERROR;
	}

	if (uop->mode == SET && uop->data == INIT) {
		pr_err("UIC set needs data input.\n");
		return ERROR;
	}

	if (uop->local_peer == INIT) {
		uop->local_peer = LOCAL;
		printf("local/peer is not given, assume local.\n");
	}

	if (uop->dir == INIT) {
		uop->dir = TX;
		printf("Tx/Rx is not given, assume Tx.\n");
	}

	if (uop->lane == INIT) {
		uop->lane = 0;
		printf("Lane N is not given, assume lane 0.\n");
	}

	return SUCCESS;
}

int init_uic_operation(int argc, char *argv[], void *op_data)
{
	struct lsufs_operation *lsufs_op = (struct lsufs_operation *)op_data;
	int ret;

	lsufs_op->uic_op.mode = INIT;
	lsufs_op->uic_op.dir = INIT;
	lsufs_op->uic_op.local_peer = INIT;
	lsufs_op->uic_op.lane = INIT;
	lsufs_op->uic_op.attr_id = INIT;
	lsufs_op->uic_op.data = INIT;
	lsufs_op->device_path[0] = '\0';

	ret = setup_uic_operation(argc, argv, lsufs_op);

	return ret ? ret : uic_op_sanity_check(lsufs_op);
}

static int send_uic_command(int fd, struct ufs_bsg_request *bsg_request, enum bsg_ioctl_dir dir)
{
	struct ufs_bsg_reply bsg_reply = {0};
	struct uic_command uc = {0};
	__u8 result = 0;
	int ret;

	ret = ufs_bsg_io(fd, bsg_request, &bsg_reply, 0, NULL, dir);
	if (ret)
		return ERROR;

	memcpy(&uc, &bsg_reply.upiu_rsp.uc, UIC_CMD_SIZE);
	result = uc.argument2 & MASK_UIC_CONFIG_RESULT_CODE;
	if (result) {
		ret = ERROR;
		pr_err("UIC command failed with config result code: %u.\n", result);
	} else {
		ret = uc.argument3;
	}

	return ret;
}

static int __uic_get(int fd, __u32 attr_sel, int peer)
{
	struct ufs_bsg_request bsg_request = {0};
	struct uic_command *uic_cmd = (struct uic_command *)&bsg_request.upiu_req.uc;

	uic_cmd->command = peer ? UIC_CMD_DME_PEER_GET : UIC_CMD_DME_GET;
	uic_cmd->argument1 = attr_sel;
	bsg_request.msgcode = UPIU_TRANSACTION_UIC_CMD;

	return send_uic_command(fd, &bsg_request, BSG_IOCTL_DIR_FROM_DEV);
}

int uic_get(int fd, __u32 attr_sel, int peer)
{
	return __uic_get(fd, attr_sel, peer);
}

static int do_uic_get(struct lsufs_operation *lsufs_op)
{
	struct uic_operation *uop = &lsufs_op->uic_op;
	int id, ret;

	id = characteristics_look_up(unipro_mphy_attrs, uop->attr_id);

	ret = __uic_get(lsufs_op->fd,
			UIC_ARG_MIB_SEL(uop->attr_id,
					uop->dir == TX ? SELECT_TX(uop->lane) :
							 SELECT_RX(uop->lane)),
			uop->local_peer);

	if (ret == ERROR) {
		pr_err("Filed to get %s Attribute ID 0x%x - %s\n", uop->local_peer ? "peer" : "local",
				uop->attr_id, id < 0 ? "???" : unipro_mphy_attrs[id].name);
		return ret;
	}

	printf("%s Attribute ID 0x%x - %s = 0x%x\n", uop->local_peer ? "peer" : "local",
			uop->attr_id, id < 0 ? "???" : unipro_mphy_attrs[id].name, ret);

	return SUCCESS;
}

static int __uic_set(int fd, __u32 attr_sel, __u8 attr_set, __u32 mib_val, int peer)
{
	struct ufs_bsg_request bsg_request = {0};
	struct uic_command *uic_cmd = (struct uic_command *)&bsg_request.upiu_req.uc;

	uic_cmd->command = peer ? UIC_CMD_DME_PEER_SET : UIC_CMD_DME_SET;
	uic_cmd->argument1 = attr_sel;
	uic_cmd->argument2 = UIC_ARG_ATTR_TYPE(attr_set);
	uic_cmd->argument3 = mib_val;
	bsg_request.msgcode = UPIU_TRANSACTION_UIC_CMD;

	return send_uic_command(fd, &bsg_request, BSG_IOCTL_DIR_FROM_DEV);
}

int uic_set(int fd, __u32 attr_sel, __u8 attr_set, __u32 mib_val, int peer)
{
	int ret;

	ret = __uic_set(fd, attr_sel, attr_set, mib_val, peer);

	return ret == ERROR ? ret : 0;
}

static int do_uic_set(struct lsufs_operation *lsufs_op)
{
	struct uic_operation *uop = &lsufs_op->uic_op;
	int id, ret;

	id = characteristics_look_up(unipro_mphy_attrs, uop->attr_id);

	ret = __uic_set(lsufs_op->fd,
			UIC_ARG_MIB_SEL(uop->attr_id,
					uop->dir == TX ? SELECT_TX(uop->lane) :
							 SELECT_RX(uop->lane)),
			ATTR_SET_NOR,
			uop->data,
			uop->local_peer);

	printf("%s set %s Attribute ID 0x%x - %s\n", ret == ERROR ? "Failed to" : "Successfully",
			uop->local_peer == PEER ? "peer" : "local", uop->attr_id,
			id < 0 ? "???" : unipro_mphy_attrs[id].name);

	return ret == ERROR ? ret : 0;
}

int do_uic_operation(void *op_data)
{
	struct lsufs_operation *lsufs_op = (struct lsufs_operation *)op_data;
	int fd, ret = 0, flag = O_RDWR;

	if (lsufs_op->uic_op.mode == GET)
		flag = O_RDONLY;

	fd = open(lsufs_op->device_path, flag);
	if (fd < 0) {
		pr_err("Filed to open file (%d).\n", fd);
		return ERROR;
	}

	lsufs_op->fd = fd;

	switch (lsufs_op->uic_op.mode) {
	case GET:
		ret = do_uic_get(lsufs_op);
		break;
	case SET:
		ret = do_uic_set(lsufs_op);
		break;
	default:
		pr_err("Why are we here?!\n");
		ret = ERROR;
		break;
	}

	close(fd);

	return ret;
}
