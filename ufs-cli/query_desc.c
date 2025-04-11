// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <linux/types.h>
#include <stdio.h>
#include <endian.h>
#include <stddef.h>
#include "query_desc.h"

void interpret_extended_ufs_features(__u8 *val, size_t len)
{
	if (len < 4) {
		printf("  # [Incomplete data for extended features]");
		return;
	}

	__u32 features = be32toh(*(__be32*)val);

	printf("\n    # Extended UFS Features Support (JESD220D 4.1):\n");

	/* Byte 0 - Original features in bUFSFeaturesSupport */
	printf("    # %-40s: %s\n", "Field Firmware Update (FFU)",
		(features & (1 << 0)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Production State Awareness (PSA)",
		(features & (1 << 1)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Device Life Span",
		(features & (1 << 2)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Refresh Operation",
		(features & (1 << 3)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "High Temperature Operation",
		(features & (1 << 4)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Low Temperature Operation",
		(features & (1 << 5)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Extended Temperature",
		(features & (1 << 6)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "HPB Extension",
		(features & (1 << 7)) ? "Supported" : "Not supported");

	/* Byte 1 - Extended features */
	printf("    # %-40s: %s\n", "WriteBooster",
		(features & (1 << 8)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Performance Throttling",
		(features & (1 << 9)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Advanced RPMB",
		(features & (1 << 10)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Zoned UFS Extension",
		(features & (1 << 11)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Device Level Exception Warning",
		(features & (1 << 12)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Host Initiated Defrag (HID)",
		(features & (1 << 13)) ? "Supported" : "Not supported");

	/* Byte 2 - Extended features */
	printf("    # %-40s: %s\n", "Barrier Support",
		(features & (1 << 14)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Clear Error History",
		(features & (1 << 15)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "EXT_IID Support",
		(features & (1 << 16)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "File Based Optimization (FBO)",
		(features & (1 << 17)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "Fast Recovery Mode",
		(features & (1 << 18)) ? "Supported" : "Not supported");
	printf("    # %-40s: %s\n", "RPMB Authenticated Vendor CMD",
		(features & (1 << 19)) ? "Supported" : "Not supported");

	/* Note about reserved bits */
	if (features & 0xFFF00000)
		printf("    # NOTE: Reserved bits 20-31 are set (0x%08X)\n", features & 0xFFF00000);
}
