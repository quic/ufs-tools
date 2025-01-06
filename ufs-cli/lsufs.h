// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __LSUFS_H__
#define __LSUFS_H__

#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include "common.h"
#include "query.h"
#include "uic.h"

struct lsufs_operation_nt {
	char *name;
	int type;
};

enum lsufs_operation_type {
	OT_UIC,
	OT_QUERY,
};

struct lsufs_operation {
	enum lsufs_operation_type type;
	int fd;
	char device_path[DEVICE_PATH_NAME_SIZE_MAX];

	union {
		struct uic_operation uic_op;
		struct query_operation query_op;
	};
};
#endif /* __LSUFS_H__ */
