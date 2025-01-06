// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SHELL_H__
#define __SHELL_H__

#define SHELL_MAX_ARGS 15

enum variant_type {
	VT_INT,
	VT_CHAR,
	VT_STRING
};

struct shell_variant_type {
	enum variant_type vt_type;

	union {
		int int_val;
		char ch_val;
		const char *str_val;
	}val;
};

struct shell_cmd_args {
	int args_cnt;
	struct shell_variant_type arg_val[SHELL_MAX_ARGS];
};

typedef int (*SHELL_CMD_HANDLER_TYPE)(struct shell_cmd_args *shell_arg);

int shell_init(const char *prompt);
int shell_add_cmd(const char *cmd, SHELL_CMD_HANDLER_TYPE cmd_handler, const char *help_str);
int shell_run(void);
int shell_atoi(const char *str);
int shell_process_cmd_line_args(int argc, char *argv[]);
#endif /* __SHELL_H__ */
