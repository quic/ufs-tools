// // SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "query.h"

#define QUERY_TRANSLATE_VERSION 	"JESD220D 4.1"

/**
 * field_name_to_size - Determine the size of a field based on its name prefix
 * @name: Field name string
 *
 * Determines the field size using the following naming convention, this matches the UFS
 * specification's field naming scheme:
 *   - 'w' prefix → 2 bytes (word)
 *   - 'd' prefix → 4 bytes (dword)
 *   - 'q' prefix → 8 bytes (qword)
 *   - 'b'/'i' prefix  → 1 byte (byte)
 *
 * Returns: Number of bytes for the field (1, 2, 4, or 8)
 */
static size_t field_name_to_size(const char *name)
{
	if (!name || !name[0])
		return 1; // Default to 1 byte

	switch (name[0]) {
		case 'w':
			return 2; // Word (2 bytes)
		case 'd':
			return 4; // Dword (4 bytes)
		case 'q':
			return 8; // Qword (8 bytes)
		case 'b':
		case 'i':
		default:
			return 1; // Byte (1 byte)
	}
}

/**
 * ufs_desc_translate - Generic UFS descriptor translate function
 * @desc_buf: Descriptor buffer
 * @len: Length of valid descriptor data
 * @desc_fields: Array of descriptor field definitions
 * @desc_name: Human-readable descriptor name for output
 *
 * This generic function can interpret any UFS descriptor type when provided
 * with the appropriate field definition array.
 */
void ufs_desc_translate(__u8 *desc_buf, __u16 len, const struct ufs_desc_item *desc_fields, const char *desc_name)
{
	const struct ufs_desc_item *item;
	size_t size;
	int i;

	if (!desc_buf || len < 1 || !desc_fields) {
		printf("Invalid descriptor parameters\n");
		return;
	}

	printf("UFS %s Descriptor:\n", desc_name);

	for (i = 0; desc_fields[i].name != NULL; i++) {
		item = &desc_fields[i];

		if (strncmp(item->name, "Reserved", 8) == 0)
			continue;

		size = field_name_to_size(item->name);

		/* Check boundry - stop at either buffer end or descriptor end */
		if (item->offset + size > MIN(len, desc_buf[0]))
			break;

		printf("0x%02X: %-35s = ", item->offset, item->name);

		switch(size) {
			case 1:
				printf("0x%02X", desc_buf[item->offset]);
				break;
			case 2:
				printf("0x%04X", be16toh(*(__be16*)(desc_buf + item->offset)));
				break;
			case 4:
				printf("0x%08X", be32toh(*(__be32*)(desc_buf + item->offset)));
				break;
			case 8:
				printf("0x%016lX", be64toh(*(__be64*)(desc_buf + item->offset)));
				break;
			default:
				printf("[Invalid Item]");
				break;
		}

		printf("\n");

		if (item->translate)
			item->translate(desc_buf + item->offset, size);
	}
}

void translate_extended_ufs_features(__u8 *val, size_t len)
{
	__u32 features = be32toh(*(__be32*)val);

	if (len < 4) {
		printf("  # [Incomplete data for extended features]");
		return;
	}

	printf("    # Extended UFS Features Support (%s):\n", QUERY_TRANSLATE_VERSION);

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
		printf("    # NOTE: Unparsed bit[31:20] contains meaningful data (0x%08X)\n",
				features);
}
