// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QUERY_H__
#define __QUERY_H__

#include <linux/types.h>
#include <sys/types.h>
#include <stddef.h>
#include "common.h"

#define DESCRIPTOR_BUFFER_SIZE	256 /* enough for recent years */

#define MANUFACTURER_NAME_STRING_DESC_SIZE	18
#define PRODUCT_NAME_STRING_DESC_SIZE		34
#define PRODUCT_REVISION_LEVEL_STRING_DESC_SIZE	10

#define DEVICE_DESCRIPTOR_IDN		0x0
#define STRING_DESCRIPTOR_IDN		0x5

#define MANUFACTURER_NAME_OFFSET		0x14
#define PRODUCT_NAME_OFFSET			0x15
#define PRODUCT_REVISION_LEVEL_OFFSET		0x2A

enum {
	QUERY_REQ_FUNC_STD_READ		= 0x01,
	QUERY_REQ_FUNC_STD_WRITE	= 0x81,
};

enum query_req_opcode {
	QUERY_REQ_OP_READ_DESC		= 0x1,
	QUERY_REQ_OP_WRITE_DESC		= 0x2,
	QUERY_REQ_OP_READ_ATTR		= 0x3,
	QUERY_REQ_OP_WRITE_ATTR		= 0x4,
	QUERY_REQ_OP_READ_FLAG		= 0x5,
	QUERY_REQ_OP_SET_FLAG		= 0x6,
	QUERY_REQ_OP_CLEAR_FLAG		= 0x7,
	QUERY_REQ_OP_TOGGLE_FLAG	= 0x8,
	QUERY_REQ_OP_MAX,
};

struct query_operation {
	int func;
	int opcode;
	int idn;
	int index;
	int selector;
	__u64 attr_value;
};

static struct ufs_characteristics ufs_descriptors[] = {
	{0x0, "Device Descriptor"},
	{0x1, "Configuration Descriptor"},
	{0x2, "Unit Descriptor"},
	{0x4, "Interconnect Descriptor"},
	{0x5, "String Descriptor"},
	{0x7, "Geometry Descriptor"},
	{0x8, "Power Parameters Descriptor"},
	{0x9, "Device Health Descriptor"},
	{INIT, NULL},
};

static struct ufs_characteristics ufs_attributes[] = {
	{0x00, "bBootLunEn"},
	{0x02, "bCurrentPowerMode"},
	{0x03, "bActiveICCLevel"},
	{0x04, "bOutOfOrderDataEn"},
	{0x05, "bBackgroundOpStatus"},
	{0x06, "bPurgeStatus"},
	{0x07, "bMaxDataInSize"},
	{0x08, "bMaxDataOutSize"},
	{0x09, "dDynCapNeeded"},
	{0x0a, "bRefClkFreq"},
	{0x0b, "bConfigDescrLock"},
	{0x0c, "bMaxNumOfRTT"},
	{0x0d, "wExceptionEventControl"},
	{0x0e, "wExceptionEventStatus"},
	{0x0f, "dSecondsPassed"},
	{0x10, "wContextConf"},
	{0x11, "Obsolete"},
	{0x14, "bDeviceFFUStatus"},
	{0x15, "bPSAState"},
	{0x16, "dPSADataSize"},
	{0x17, "bRefClkGatingWaitTime"},
	{0x18, "bDeviceCaseRoughTemperaure"},
	{0x19, "bDeviceTooHighTempBoundary"},
	{0x1a, "bDeviceTooLowTempBoundary"},
	{0x1b, "bThrottlingStatus"},
	{0x1c, "bWriteBoosterBufferFlushStatus"},
	{0x1d, "bAvailableWriteBoosterBufferSize"},
	{0x1e, "bWriteBoosterBufferLifeTimeEst"},
	{0x1f, "dCurrentWriteBoosterBufferSize"},
	{0x2a, "bEXTIIDEn"},
	{0x2b, "wHostHintCacheSize"},
	{0x2c, "bRefreshStatus"},
	{0x2d, "bRefreshFreq"},
	{0x2e, "bRefreshUnit"},
	{0x2f, "bRefreshMethod"},
	{0x30, "qTimestamp"},
	{0x34, "qDeviceLevelExceptionID"},
	{0x35, "bDefragOperation"},
	{0x36, "dHIDAvaliableSize"},
	{0x37, "dHIDSize"},
	{0x38, "bHIDProgressRatio"},
	{0x39, "bHIDState"},
	{0x3c, "bWriteBoosterBufferResizeHint"},
	{0x3d, "bWriteBoosterBufferResizeEn"},
	{0x3e, "bWriteBoosterBufferResizeStatus"},
	{0x3f, "bWriteBoosterBufferPartialFlushMode"},
	{0x40, "dMaxFIFOSizeForWriteBoosterPartialFlushMode"},
	{0x41, "dCurrentFIFOSizeForWriteBoosterPartialFlushMode"},
	{0x42, "dPinnedWriteBoosterBufferCurrentAllocUnits"},
	{0x43, "bPinnedWriteBoosterBufferAvailablePercentage"},
	{0x44, "dPinnedWriteBoosterCummulativeWrittenSize"},
	{0x45, "dPinnedWriteBoosterBufferNumAllocUnits"},
	{0x46, "dNonPinnedWriteBoosterBufferMinNumAllocUnits"},
	{INIT, NULL},
};

static struct ufs_characteristics ufs_flags[] = {
	{0x01, "fDeviceInit"},
	{0x02, "fPermanentWPEn"},
	{0x03, "fPowerOnWPEn"},
	{0x04, "fBackgroundOpsEn"},
	{0x05, "fDeviceLifeSpanModeEn"},
	{0x06, "fPurgeEnable"},
	{0x07, "fRefreshEnable"},
	{0x08, "fPhyResourceRemoval"},
	{0x09, "fBusyRTC"},
	{0x0b, "fPermanentlyDisableFwUpdate"},
	{0x0e, "fWriteBoosterEn"},
	{0x0f, "fWriteBoosterBufferFlushEn"},
	{0x10, "fWriteBoosterBufferFlushDuringHibernate"},
	{0x11, "fHPBReset"},
	{0x12, "fHPBEnable"},
	{0x13, "fUnpinEn"},
	{INIT, NULL},
};

int init_query_operation(int argc, char *argv[], void *op_data);
int query_read_descriptor(int fd, int idn, int index, int sel, __u8 *buf, __u16 buf_len);
int do_query_operation(void *op_data);
#endif /* __QUERY_H__ */
