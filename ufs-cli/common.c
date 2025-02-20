// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "common.h"

int get_value_from_cli(int *val)
{
	char *end;

	if (strstr(optarg, "0x") || strstr(optarg, "0X")) {
		*val = (int)strtol(optarg, &end, 0);
		if (*end != '\0')
			return ERROR;
	} else {
		*val = atoi(optarg);
	}

	if ((*val == 0 && strncmp(optarg, "0", 1)) || *val < 0)
		return ERROR;

	return SUCCESS;
}

int get_ull_from_cli(unsigned long long *val)
{
	char *end;

	if (strstr(optarg, "0x") || strstr(optarg, "0X")) {
		*val = strtoull(optarg, &end, 0);
		if (*end != '\0')
			return ERROR;
	} else {
		*val = (unsigned long long)atoll(optarg);
	}

	if ((*val == 0 && strncmp(optarg, "0", 1)) || *val < 0)
		return ERROR;

	return SUCCESS;
}

int init_device_path(char *path)
{
        if (optarg[0] == 0) {
                pr_err("Need the path to bsg device.\n");
                return ERROR;
        }

        if (strlen(optarg) >= DEVICE_PATH_NAME_SIZE_MAX) {
                pr_err("bsg device path is too long.\n");
                return ERROR;
        }

        strcpy(path, optarg);

        return SUCCESS;
}

int characteristics_look_up(struct ufs_characteristics *c, __u32 id)
{
	int i;
	bool found = false;

	for (i = 0; c->id != INIT; i ++, c ++) {
		if (c->id == id) {
			found = true;
			break;
		}
	}

	if (!found)
		return INIT;

	return i;
}
