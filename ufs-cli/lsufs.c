// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "lsufs.h"
#include "shell.h"

#define LSUFS_VERSION  "1.0"

const char *lsufs_help =
	"\nlsufs cli :\n\n"
	"-h : help\n"
	"uic : do uic operation, try 'lsufs uic -h'\n"
	"query : do query operation, try 'lsufs query -h'\n";

const char *uic_operation_help =
	"\nuic operation cli : \n\n"
	"uic [-g | --get] [-s | --set <data>] [-i | --id <id>] [-p | --peer | -l | --local] [--TX | --tx | --Tx | --RX | --rx | --Rx] [-L | --lane <lane>] [-d | --device <device>]\n\n"
	"-h | --help : help\n"
	"-g | --get : get\n"
	"-s | --set : set, should be followed by data\n"
	"-p | --peer : peer\n"
	"-l | --local : local\n"
	"-i | --id : Unipro or M-PHY Attribute ID\n"
	"-t | --TX | --tx | --Tx : Select Tx\n"
	"-r | --RX | --rx | --Rx : Select RX\n"
	"-L | --lane : Select lane, should be followed by lane number, if lane number is not given, select lane 0 by default\n"
	"-d | --device : path to ufs-bsg device\n\n"
	"Example:\n"
	"  1. Get peer RX_EYEMON_Enable for lane1:\n"
	"  uic --get -i 0x00f6 --peer --RX --lane 1 -d /dev/ufs-bsg0\n"
	"  2. Set 0x44 to local RX_EYEMON_Enable for lane0:\n"
	"  uic --set 0x44 -i 0x00f6 --local --RX --lane 0 -d /dev/ufs-bsg0\n";

const char *query_operation_help =
	"\nquery operation cli : \n\n"
	"query [-o | --opcode <opcode>] [-v | --value <value>] [-i | --idn <IDN>] [-I | --index <index>] [-s | --select <selector>] [-d | --device <device>]\n\n"
	"-h | --help : help\n"
	"-o | --opcode :\n"
		"\t 1 - read descriptor\n"
		"\t 2 - write descriptor\n"
		"\t 3 - read attribute\n"
		"\t 4 - write attribute\n"
		"\t 5 - read flag\n"
		"\t 6 - set flag\n"
		"\t 7 - clear flag\n"
		"\t 8 - toggle flag\n"
	"-v | --value : Attribute value, only applicable for 'query write attribute', ignored in other cases\n"
	"-i | --idn : IDN of descriptors/attributes/flags\n"
	"-I | --index : Index of descriptors/attributes/flags\n"
	"-s | --select : Selector of descriptors/attributes/flags\n"
	"-d | --device : path to ufs-bsg device\n\n"
	"Example:\n"
	"  1. query read Device descriptor:\n"
	"  query -o 1 -i 0 -I 0 -s 0 -d /dev/ufs-bsg0\n\n"
	"  2. query write Device descriptor:\n"
	"  Not supported yet.\n\n"
	"  3. query read Attribute bCurrentPowerMode:\n"
	"  query -o 3 -i 0x2 -I 0 -s 0 -d /dev/ufs-bsg0\n\n"
	"  4. query write Attribute bRefreshFreq to value 02h:\n"
	"  query -o 4 -v 0x2 -i 0x2d -I 0 -s 0 -d /dev/ufs-bsg0\n\n"
	"  5. query read Flag fPowerOnWPEn:\n"
	"  query -o 5 -i 0x3 -I 0 -s 0 -d /dev/ufs-bsg0\n\n"
	"  6. query set Flag fRefreshEnable:\n"
	"  query -o 6 -i 0x7 -I 0 -s 0 -d /dev/ufs-bsg0\n\n"
	"  7. query clear Flag fRefreshEnable:\n"
	"  query -o 7 -i 0x7 -I 0 -s 0 -d /dev/ufs-bsg0\n\n"
	"  8. query toggle Flag fRefreshEnable:\n"
	"  query -o 8 -i 0x7 -I 0 -s 0 -d /dev/ufs-bsg0\n";

static struct lsufs_operation lsufs_op;

static struct lsufs_operation_nt lsufs_nts[] = {
	{"uic", OT_UIC},
	{"query", OT_QUERY},
	{0, 0},
};

static void print_help_info(enum lsufs_operation_type type)
{
	switch (type) {
	case OT_UIC:
		printf("%s\n", uic_operation_help);
		break;
	case OT_QUERY:
		printf("%s\n", query_operation_help);
		break;
	}
}

static int kshell_op_uic(struct shell_cmd_args *args)
{
	if (args->arg_val[0].vt_type == VT_STRING &&
	    (!strcmp(args->arg_val[0].val.str_val, "--help") ||
	     !strcmp(args->arg_val[0].val.str_val, "-h"))) {
		return ERROR;
	}

	return do_uic_operation(&lsufs_op);
}

static int kshell_op_query(struct shell_cmd_args *args)
{
	if (args->arg_val[0].vt_type == VT_STRING &&
	    (!strcmp(args->arg_val[0].val.str_val, "--help") ||
	     !strcmp(args->arg_val[0].val.str_val, "-h"))) {
		return ERROR;
	}

	return do_query_operation(&lsufs_op);
}

static int parse_args(int argc, char *argv[])
{
	struct lsufs_operation_nt *nt;
	bool is_operation_supported = false;
	int ret;

	if (argc < 2) {
		pr_err("Too less args, try 'lsufs -h'\n");
		return ERROR;
	}

	if (!strcmp(argv[1], "-v")) {
		printf("lsufs version %s.\n", LSUFS_VERSION);
		return SUCCESS;
	} else if (!strcmp(argv[1], "-h")) {
		printf("%s\n", lsufs_help);
		return SUCCESS;
	}

	for (nt = lsufs_nts; nt->name; nt++) {
		if (!strcmp(argv[1], nt->name)) {
			is_operation_supported = true;
			lsufs_op.type = nt->type;
			break;
		}
	}

	if (!is_operation_supported) {
		pr_err("Unsupported operation.\n");
		return ERROR;
	}

	if (argc == 3 && (!strcmp(argv[2], "-h") || !strcmp(argv[2], "--help"))) {
		print_help_info(lsufs_op.type);
		return SUCCESS;
	}

	switch (lsufs_op.type) {
	case OT_UIC:
		ret = init_uic_operation(argc, argv, &lsufs_op);
		if (ret) {
			pr_err("Please try 'uic -h'\n");
			return ret;
		}
		break;
	case OT_QUERY:
		ret = init_query_operation(argc, argv, &lsufs_op);
		if (ret) {
			pr_err("Please try 'query -h'\n");
			return ret;
		}
		break;
	}

	return SUCCESS;
}

int main(int argc, char *argv[])
{
	char **orig_argv = malloc(sizeof(char *) * argc);
	int ret;

	if (!orig_argv) {
		pr_err("Failed to allocate memory for lsufs command arguments\n");
		return ERROR;
	}

	shell_init("rshell");
	memcpy(orig_argv, argv, sizeof(char *) * argc);

	ret = parse_args(argc, argv);
	if (ret)
		return ret;

	shell_add_cmd("uic", kshell_op_uic, "Please try 'uic -h'\n");
	shell_add_cmd("query", kshell_op_query, "Please try 'query -h'\n");

	if (argc >= 1)
		return shell_process_cmd_line_args(argc, orig_argv);

	shell_run();

	return ret;
}
