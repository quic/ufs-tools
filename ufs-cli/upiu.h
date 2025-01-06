// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __UPIU_H__
#define __UPIU_H__

#include <linux/types.h>
#include <sys/types.h>
#include <stddef.h>

/* UPIU Transaction Codes */
enum {
	UTP_UPIU_NOP_OUT	= 0x00,
	UTP_UPIU_COMMAND	= 0x01,
	UTP_UPIU_DATA_OUT	= 0x02,
	UTP_UPIU_TASK_REQ	= 0x04,
	UTP_UPIU_QUERY_REQ	= 0x16,
};
#endif /* __UPIU_H__ */
