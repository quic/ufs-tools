// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SHELL_MAX_CMDS 100
#define SHELL_MAX_CMD_LEN 256

#define SHELL_ASSERT(cond) \
	do { \
		if (!(cond)) { \
			printf("Assertion : [%s]\n", #cond); \
		} \
	} while (0)	                                  \

struct shell_cmd_type {
	const char* cmd;
	SHELL_CMD_HANDLER_TYPE cmd_handler;
	const char* help_str;
};

static const char *shell_cmd_prompt;
static char shell_cmd_buff[SHELL_MAX_CMD_LEN];
static const char *shell_cmd;

SHELL_CMD_HANDLER_TYPE	shell_last_cmd_handler;
static struct shell_cmd_args shell_args;

static struct shell_cmd_type shell_cmds[SHELL_MAX_CMDS];
static int shell_cmds_cnt;

static int shell_help(struct shell_cmd_args *args) {
	int i;
	(void) args;

	for (i = 0; i < shell_cmds_cnt; ++i) {
		printf ("%16s -> %s\n", shell_cmds[i].cmd, shell_cmds[i].help_str);
	}

	return 0;
}

int shell_exit(struct shell_cmd_args *args)
{
	(void) args;
	return -1;
}

int shell_test_args(struct shell_cmd_args *args)
{
	int i, val;

	printf("sizeof(struct shell_cmd_args) = [%zu]\n", sizeof(struct shell_cmd_args));
	printf("sizeof(enum variant_type) = [%zu]\n", sizeof(enum variant_type));

	printf ("args count : [%d]\n", args->args_cnt);
	for (i = 0; i < args->args_cnt; ++i) {
		switch (args->arg_val[i].vt_type) {
			case VT_INT:
				val = args->arg_val[i].val.int_val;
				printf ("int : [%d] [hex=%x]\n", val, val);
				break;

			case VT_CHAR:
				printf ("char : [%c]\n", args->arg_val[i].val.ch_val);
				break;

			case VT_STRING:
				printf ("str : [%s]\n", args->arg_val[i].val.str_val);
				break;

			default:
				printf ("invalid arg : [%d]\n", args->arg_val[i].vt_type);
				SHELL_ASSERT (0);
			}
	}
	return 0;
}

int shell_init(const char *prompt)
{
	shell_cmd_prompt = prompt;
	memset(&shell_cmds, 0, sizeof (shell_cmds));
	shell_cmds_cnt = 0;
	shell_last_cmd_handler = NULL;

	shell_add_cmd("help", shell_help, "print all available cmds");
	shell_add_cmd("exit", shell_exit, "exit shell");
	shell_add_cmd("quit", shell_exit, "quit shell");
	shell_add_cmd("targs", shell_test_args, "test args");

	return 0;
}

int shell_add_cmd(const char *cmd, SHELL_CMD_HANDLER_TYPE cmd_handler,
	const char *help_str)
{
	SHELL_ASSERT(shell_cmds_cnt < SHELL_MAX_CMDS);
	shell_cmds[shell_cmds_cnt].cmd = cmd;
	shell_cmds[shell_cmds_cnt].cmd_handler = cmd_handler;
	shell_cmds[shell_cmds_cnt].help_str = help_str;
	++shell_cmds_cnt;
	return 0;
}

static void shell_print_prompt(void)
{
	printf("\n%s>", shell_cmd_prompt);
}

static int is_alphabet(char ch)
{
	if ((ch >= 'A') && (ch <= 'Z'))
		return 1;
	if ((ch >= 'a') && (ch <= 'z'))
		return 1;
	return 0;
}

static int is_character(const char *str)
{
	if ((is_alphabet(str[0])) &&
			 ((str[1] == ' ') || (str[1] == '\0')))
		return 1;

	return 0;
}

int is_digit(char ch, int is_hex)
{
	if ((ch >= '0') && (ch <= '9'))
		return 1;

	if (is_hex) {
		ch = toupper(ch);
		if ((ch >= 'A') && (ch <= 'F'))
			return 1;
	}

	return 0;
}

static int is_number(const char *str)
{
	char ch;
	int is_hex = 0;
	int found_number = 0;

	if (str[0] == '0' && toupper(str[1])) {
		is_hex = 1;
		str += 2;
		found_number = 1;
	} else {
		found_number = is_digit(*str++, is_hex);
	}

	while (found_number && *str) {
		ch = *str++;
		if (ch == ' ')
			break;

		if (!is_digit(ch, is_hex))
			found_number = 0;
	}

	return found_number;
}

int shell_atoi(const char *str)
{
	int val = 0;

	if (is_number(str)) {
		val = atoi(str);
		if (val == 0)
			val = strtol(str, NULL, 16);
	}
	return val;
}

static int shell_read_cmd_line(void)
{
	char ch;
	int idx = 0, len;

	len = sizeof(shell_cmd_buff);
	memset(shell_cmd_buff, 0, len);

	while (idx < len) {
		ch = getchar();
		if (ch == '\r' || ch == '\n')
			break;
		shell_cmd_buff[idx++] = ch;
	}

        return idx;
}

static int shell_interpret_cmd_line(void)
{
	char *pstr;

	memset(&shell_args, 0, sizeof(shell_args));

	shell_cmd = strtok(shell_cmd_buff, " ");
	pstr = strtok (NULL, " ");
	while (pstr != NULL) {
		if (is_character(pstr)) {
			shell_args.arg_val[shell_args.args_cnt].vt_type = VT_CHAR;
			shell_args.arg_val[shell_args.args_cnt].val.ch_val = pstr[0];
			++shell_args.args_cnt;
		} else if (is_number(pstr)) {
			int val;
			val = atoi(pstr);
			if (val == 0)
				val = strtol(pstr, NULL, 16);
			shell_args.arg_val[shell_args.args_cnt].vt_type = VT_INT;
			shell_args.arg_val[shell_args.args_cnt].val.int_val = val;
			++shell_args.args_cnt;
		} else {
			shell_args.arg_val[shell_args.args_cnt].vt_type = VT_STRING;
			shell_args.arg_val[shell_args.args_cnt].val.str_val = pstr;
			++shell_args.args_cnt;
		}

		if (shell_args.args_cnt == SHELL_MAX_ARGS)
			return -1;

		pstr = strtok(NULL, " ");
	}

	return 0;
}

static int shell_read_cmd(void)
{
        int result;

        result = shell_read_cmd_line();
	if (result == 0)
		return 0;

        return shell_interpret_cmd_line();
}

static int shell_process_cmd(void)
{
	int i;
	int result = 0;

	if (shell_cmd_buff[0] == 0) {
		if (shell_last_cmd_handler) {
			return (shell_last_cmd_handler)(&shell_args);
		}
	}

	for (i = 0; i < shell_cmds_cnt; ++i) {
		if (strncmp(shell_cmd, shell_cmds[i].cmd, strlen(shell_cmd)) == 0) {
			result = shell_cmds[i].cmd_handler(&shell_args);
			shell_last_cmd_handler = shell_cmds[i].cmd_handler;
			break;
		}
	}

	return result;
}

int shell_process_cmd_line_args(int argc, char *argv[])
{
    int i=0, idx=0, len, result;

    if (argc <= 1)
		return -1;
	len = sizeof(shell_cmd_buff);
	memset(shell_cmd_buff, 0, len);

	for (i = 1; i < argc; ++i) {
		len = strlen(argv[i]);
		strncpy(&shell_cmd_buff[idx], argv[i], len);
		idx += len;
		if (i < (argc-1))
			shell_cmd_buff[idx++] = ' ';
	}

	result = shell_interpret_cmd_line();
	if (result != 0)
		return result;
	return shell_process_cmd();
}


int shell_run(void)
{
	int result = 0;

	SHELL_ASSERT(shell_cmd_prompt != NULL);

	while (1) {
		shell_print_prompt();
		result = shell_read_cmd();
		if (result)
			break;
		result = shell_process_cmd();
		if (result)
			break;
	}

	return result;
}
