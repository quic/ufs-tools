// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <endian.h>
#include <linux/types.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#define SUCCESS	0
#define INIT	-1
#define ERROR	-2

#define DEVICE_PATH_NAME_SIZE_MAX	256

#define pr_err(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

#define DWORD(b3, b2, b1, b0) htobe32(((b3) << 24) | ((b2) << 16) |\
				      ((b1) << 8) | (b0))

#define MIN(a, b) ((a) < (b) ? (a) : (b))

struct ufs_characteristics {
	__u32 id;
	const char *name;
};

/**
 * struct ufs_desc_item - UFS descriptor field information
 * @offset: Byte offset of the field in the descriptor
 * @name: Field name (prefix determines size: b=1,w=2,d=4,q=8)
 * @interpreter: Optional interpretation function (NULL for simple fields)
 *               Parameters: pointer to raw value and its length
 */
struct ufs_desc_item {
	__u8 offset;
	const char *name;
	void (*interpreter)(__u8 *val, size_t len);
};

int get_ull_from_cli(unsigned long long *val);
int get_value_from_cli(int *val);
int init_device_path(char *path);
int characteristics_look_up(struct ufs_characteristics *c, __u32 id);
#endif /* __COMMON_H__ */
