// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

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
#include <malloc.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include "common.h"
#include "query.h"
#include "uic.h"

#define EOM_VERSION  "1.0"

#define EOM_TARGET_TEST_COUNT_DEFAULT	0x5D
#define EOM_TARGET_TEST_COUNT_MAX	0x7F
#define EOM_PHY_ERROR_COUNT_THRESHOLD	0x3C
#define EOM_DIRECTION_SHIFT		0x6
#define EOM_STEP_MASK			0x3F
#define EOM_TEMP_DATA_SIZE		4 * 1024 * 1024	//4MB file
#define EOM_TEMP_DATA_MEM_ALIGN_SIZE	4096

#define STRING_BUFFER_SIZE		0x24

struct eom_result {
	int lane;
	int timing;
	int volt;
	int error_cnt;
};

struct EOMData {
	int timing_max_steps;
	int timing_max_offset;
	int voltage_max_steps;
	int voltage_max_offset;
	int data_cnt;
	int num_lanes;
	int local_peer;

	struct eom_result *er;
} eom_data;

static char output_path[DEVICE_PATH_NAME_SIZE_MAX];
static char device_path[DEVICE_PATH_NAME_SIZE_MAX];
const static char *ufseom_tmp_file = "ufseom_tmp_data";
static char *tmp_buf;

/* Global EOM control parameters */
static int lane;
static int voltage;
static int target_test_count;
static int eom_result_count;
static int tmp_fd, bsg_fd;
static bool single_voltage;
static bool do_io;
static bool verbose;

const char *ufseom_help =
	"\nufseom cli :\n\n"
	"ufseom [-p | --peer | -l | --local] [-D | --data] [-L | --lane <lane no.>] [-v | --voltage <voltage>] [-t | --target <target test count>] [-o | --output <output>] [-d | --device <device>]\n\n"
	"-h : help\n"
	"--version : UFS EOM version\n"
	"-p | --peer : peer\n"
	"-l | --local : local\n"
	"-D | --data : explicitly do I/O transfer with random data patterns to stress the link while EOM is running\n"
	"-L | --lane : lane no. 0 or 1, collect EOM data for all connected lanes if not given\n"
	"-v | --voltage : collect EOM data for this voltage only\n"
	"-t | --target : target test count\n"
	"-o | --output : path to the folder where the EOM report is saved\n"
	"-V | --verbose : enable detailed EOM information and logs\n"
	"-d | --device : path to ufs-bsg device\n\n"
	"Example:\n"
	"  1. Collect EOM data for local Rx:\n"
	"  ufseom -l -D -o /data/ -d /dev/ufs-bsg0\n"
	"  2. Collect EOM data for peer Rx:\n"
	"  ufseom -p -D -o /data/ -d /dev/ufs-bsg0\n"
	"  3. Collect EOM data for local Rx with voltage 0 only:\n"
	"  ufseom -l -D -v 0 -o /data/ -d /dev/ufs-bsg0\n\n"
	"Note that to get accurate EOM data, user should disable UFS driver low power mode features,\n"
	"such as Clock Scaling, Clock Gating, Suspend/Resume and Auto Hibernate. For example:\n"
	"$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkscale_enable\n"
	"$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkgate_enable\n"
	"$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/auto_hibern8\n\n"
	"In addition, ufseom changes UFS Host and/or UFS device UIC layer execution environments,\n"
	"although UFS EOM is not supposed to disturb normal I/O traffics, it is recommanded to\n"
	"reboot the system after use ufseom.\n";

static char *ufseom_short_options = "plDL:v:t:o:d:V";

static struct option ufseom_long_options[] = {
	{"peer", no_argument, NULL, 'p'}, /* UFS device */
	{"local", no_argument, NULL, 'l'}, /* UFS host*/
	{"data", no_argument, NULL, 'D'}, /* Do I/Os */
	{"lane", required_argument, NULL, 'L'}, /* Lane */
	{"voltage", required_argument, NULL, 'v'}, /* Voltage */
	{"target", required_argument, NULL, 't'}, /* Target test count */
	{"output", required_argument, NULL, 'o'}, /* EOM result output path */
	{"device", required_argument, NULL, 'd'}, /* UFS BSG device path. For example: /dev/ufs-bsg0 */
	{"verbose", no_argument, NULL, 'V'}, /* Enable detailed EOM information and logs */
	{NULL, 0, NULL, 0}
};

static uint64_t fast_rand64(uint64_t *seed)
{
	uint64_t val = *seed;

	val = (3935559000370003845LL * val + 3037000493LL);
	*seed = val;

	return val & 0x7FFFFFFFFFFFFFFFLL;
}

static void populate_data_pattern(char *buffer)
{
	unsigned int i, val;
	unsigned int size = EOM_TEMP_DATA_SIZE / sizeof(int);
	uint64_t seed = 99098;
	int *buf = (int *)buffer;

	for (i = 0; i < size; i++) {
		val = (fast_rand64(&seed) & 0xFFFFFFFF);
		*buf = val;
		buf++;
	}
}

static int parse_string_desc(__u8 *buf, char *string)
{
	int len, i, j;

	if (buf == NULL || string == NULL)
		return ERROR;

	/* bLength */
	len = buf[0];

	for (i = 2, j = 0; i < len; i++) {
		if (buf[i])
			string[j++] = (char)buf[i];
	}

	strcat(string, "\0");

	return SUCCESS;
}

static int get_voltage_value_from_cli(int *val)
{
	char *end;

	if (strstr(optarg, "0x") || strstr(optarg, "0X")) {
		*val = (int)strtol(optarg, &end, 0);
		if (*end != '\0')
			return ERROR;
	} else {
		*val = atoi(optarg);
	}

	if (*val == 0 && strncmp(optarg, "0", 1))
		return ERROR;

	return SUCCESS;
}

int get_device_info(char *mname, char *pname, char *pversion)
{
	int mname_idx, pname_idx, pver_idx, ret;
	__u8 desc_buf[DESCRIPTOR_BUFFER_SIZE] = {0};
	char string_buf[STRING_BUFFER_SIZE];

	ret = query_read_descriptor(bsg_fd, DEVICE_DESCRIPTOR_IDN, 0, 0, desc_buf, DESCRIPTOR_BUFFER_SIZE);
	if (ret) {
		pr_err("Failed to read Device Descriptor\n");
		return ret;
	}

	mname_idx = desc_buf[MANUFACTURER_NAME_OFFSET];
	pname_idx = desc_buf[PRODUCT_NAME_OFFSET];
	pver_idx = desc_buf[PRODUCT_REVISION_LEVEL_OFFSET];

	ret = query_read_descriptor(bsg_fd, STRING_DESCRIPTOR_IDN, mname_idx, 0, desc_buf, DESCRIPTOR_BUFFER_SIZE);
	if (ret) {
		pr_err("Failed to read Manufacturer Name String Descriptor\n");
		return ret;
	}
	memset(string_buf, 0, STRING_BUFFER_SIZE);
	parse_string_desc(desc_buf, string_buf);
	strcpy(mname, string_buf);

	ret = query_read_descriptor(bsg_fd, STRING_DESCRIPTOR_IDN, pname_idx, 0, desc_buf, DESCRIPTOR_BUFFER_SIZE);
	if (ret) {
		pr_err("Failed to read Product Name String Descriptor\n");
		return ret;
	}
	memset(string_buf, 0, STRING_BUFFER_SIZE);
	parse_string_desc(desc_buf, string_buf);
	strcpy(pname, string_buf);

	ret = query_read_descriptor(bsg_fd, STRING_DESCRIPTOR_IDN, pver_idx, 0, desc_buf, DESCRIPTOR_BUFFER_SIZE);
	if (ret) {
		pr_err("Failed to read Product Revision Level String Descriptor\n");
		return ret;
	}
	memset(string_buf, 0, STRING_BUFFER_SIZE);
	parse_string_desc(desc_buf, string_buf);
	strcpy(pversion, string_buf);

	return SUCCESS;
}

static int config_eom(int peer, int lane, int timing, int volt, int target_count)
{
	int ret;

	/* Enable Eye Monitor */
	ret = uic_set(bsg_fd, UIC_ARG_MIB_SEL(RX_EYEMON_ENABLE, SELECT_RX(lane)), ATTR_SET_NOR, 1, peer);
	if (ret) {
		pr_err("Failed to set RX_EYEMON_Enable\n");
		return ret;
	}

	/* Config Eye Monitor timing steps */
	ret = uic_set(bsg_fd, UIC_ARG_MIB_SEL(RX_EYEMON_TIMING_STEPS, SELECT_RX(lane)), ATTR_SET_NOR, timing, peer);
	if (ret) {
		pr_err("Failed to set RX_EYEMON_Timing_Steps\n");
		return ret;
	}

	/* Config Eye Monitor voltage steps */
	ret = uic_set(bsg_fd, UIC_ARG_MIB_SEL(RX_EYEMON_VOLTAGE_STEPS, SELECT_RX(lane)), ATTR_SET_NOR, volt, peer);
	if (ret) {
		pr_err("Failed to set RX_EYEMON_Voltage_Steps\n");
		return ret;
	}

	/* Config Eye Monitor target test count */
	ret = uic_set(bsg_fd, UIC_ARG_MIB_SEL(RX_EYEMON_TARGET_TEST_COUNT, SELECT_RX(lane)),
				ATTR_SET_NOR, target_count, peer);
	if (ret) {
		pr_err("Failed to set RX_EYEMON_Target_Test_Count\n");
		return ret;
	}

	/* Select NO_ADAPT */
	ret = uic_set(bsg_fd, UIC_ARG_MIB_SEL(PA_TXHSADAPTTYPE, SELECT_TX(0)), ATTR_SET_NOR, PA_NO_ADAPT, 0);
	if (ret) {
		pr_err("Failed to set NO_ADAPT\n");
		return ret;
	}

	/* Do a Power Mode Change to Fast Mode to apply NO_ADAPT and also trigger a RCT to kick start EOM */
	ret = uic_set(bsg_fd, UIC_ARG_MIB_SEL(PA_PWRMODE, SELECT_TX(0)), ATTR_SET_NOR, 0x11, 0);
	if (ret) {
		pr_err("Failed to trigger RCT\n");
		return ret;
	}

	/* Poll UniPro State to confirm PMC is done. */
	while (1) {
		ret = uic_get(bsg_fd, UIC_ARG_MIB_SEL(QCOM_DME_VS_UNIPRO_STATE, SELECT_TX(0)), 0);
		if (ret < 0) {
			/* Failed to get QCOM_DME_VS_UNIPRO_STATE, maybe not supported? */
			break;
		} else if ((ret & QCOM_DME_VS_UNIPRO_STATE_MASK) == QCOM_DME_VS_UNIPRO_STATE_LINK_UP) {
			break;
		}
	}

	/* QCOM_DME_VS_UNIPRO_STATE not supported? Delay a bit to make sure PMC is completed */
	if (ret < 0)
		usleep(200000);

	return SUCCESS;
}

static int eom_scan(int peer, int lane, int timing, int volt, int target_count)
{
	struct EOMData *data = &eom_data;
	int voltage_direction, timing_direction;
	int voltage_steps, timing_steps;
	int eom_start, eom_tested_count, eom_error_count;
	int ret;

	if (volt < 0) {
		voltage_direction = 1;
		voltage_steps = (voltage_direction << EOM_DIRECTION_SHIFT) | (-volt & EOM_STEP_MASK);
	} else {
		voltage_direction = 0;
		voltage_steps = (voltage_direction << EOM_DIRECTION_SHIFT) | (volt & EOM_STEP_MASK);
	}

	if (timing < 0) {
		timing_direction = 1;
		timing_steps = (timing_direction << EOM_DIRECTION_SHIFT) | (-timing & EOM_STEP_MASK);
	} else {
		timing_direction = 0;
		timing_steps = (timing_direction << EOM_DIRECTION_SHIFT) | (timing & EOM_STEP_MASK);
	}

	ret = config_eom(peer, lane, timing_steps, voltage_steps, target_count);
	if (ret) {
		pr_err("Failed to configure EOM.\n");
		return ret;
	}

repeat_eom_scan:
	if (!do_io)
		goto skip_io;

	if (peer == PEER) {
		/* Write to excercise peer device's Rx */
		ret = pwrite(tmp_fd, tmp_buf, EOM_TEMP_DATA_SIZE, 0);
		if (ret < 0) {
			pr_err("Failed to write tmp file\n");
			return ERROR;
		}
	} else {
		/* Read to excercise local device's Rx */
		ret = pread(tmp_fd, tmp_buf, EOM_TEMP_DATA_SIZE, 0);
		if (ret < 0){
			pr_err("Failed to read tmp file\n");
			return ERROR;
		}
	}

skip_io:
	/* Get RX_EYEMON_Start */
	eom_start = uic_get(bsg_fd, UIC_ARG_MIB_SEL(RX_EYEMON_START, SELECT_RX(lane)), peer);
	if (eom_start < 0) {
		pr_err("Failed to get RX_EYEMON_Start, eom_start = %d\n", eom_start);
		return ERROR;
	}

	/* EOM has not yet stopped */
	if (eom_start & RX_EYEMON_START_MASK)
		goto repeat_eom_scan;

	/* Get RX_EYEMON_Tested_Count */
	eom_tested_count = uic_get(bsg_fd, UIC_ARG_MIB_SEL(RX_EYEMON_TESTED_COUNT, SELECT_RX(lane)), peer);
	if (eom_tested_count < 0) {
		pr_err("Failed to get RX_EYEMON_Tested_Count\n");
		return ERROR;
	}

	/* Get RX_EYEMON_Error_Count */
	eom_error_count = uic_get(bsg_fd, UIC_ARG_MIB_SEL(RX_EYEMON_ERROR_COUNT, SELECT_RX(lane)), peer);
	if (eom_error_count < 0) {
		pr_err("Failed to get RX_EYEMON_Error_Count\n");
		return ERROR;
	}

	/* EOM has stopped, good to log results */
	if (eom_tested_count >= target_count || eom_error_count >= EOM_PHY_ERROR_COUNT_THRESHOLD) {
		if (verbose)
			printf("lane: %d timing: %d voltage: %d error count: %d [tested_count: %d]\n", lane, timing, volt,
												       eom_error_count,
												       eom_tested_count);

		data->er[data->data_cnt].lane = lane;
		data->er[data->data_cnt].timing = timing;
		data->er[data->data_cnt].volt = volt;
		data->er[data->data_cnt].error_cnt = eom_error_count;
		data->data_cnt ++;
		if (data->data_cnt > eom_result_count) {
			pr_err("The count of data exceeds the maximum %d of the device\n", eom_result_count);
			return ERROR;
		}

		return SUCCESS;
	}

	/* EOM is running or has not yet started */
	goto repeat_eom_scan;
}

static int generate_eom_report(char *eom_file, struct EOMData *data)
{
	char mname[MANUFACTURER_NAME_STRING_DESC_SIZE];
	char pname[PRODUCT_NAME_STRING_DESC_SIZE];
	char pver[PRODUCT_REVISION_LEVEL_STRING_DESC_SIZE];
	int i, ret;
	FILE *file;

	ret = get_device_info(mname, pname, pver);
	if (ret)
		return ret;

	file = fopen(eom_file, "w");
	if (!file) {
		pr_err("Failed to create EOM result file %s\n", eom_file);
		return ERROR;
	}

	fprintf(file, "UFS %s Side Eye Monitor Start\n", data->local_peer ? "Device" : "Host");
	fprintf(file, "- - - - UFS INQUIRY ID: %s %s %s\n", mname, pname, pver);
	fprintf(file, "EOM Capabilities:\n");
	fprintf(file, "TimingMaxSteps %d TimingMaxOffset %d\n", data->timing_max_steps, data->timing_max_offset);
	fprintf(file, "VoltageMaxSteps %d VoltageMaxOffset %d\n\n", data->voltage_max_steps, data->voltage_max_offset);

	for (i = 0; i < data->data_cnt; i++)
		fprintf(file, "lane: %d timing: %d voltage: %d error count: %d\n", data->er[i].lane, data->er[i].timing,
										   data->er[i].volt, data->er[i].error_cnt);

	fclose(file);
	printf("EOM results saved to %s\n", eom_file);

	return SUCCESS;
}

static int check_output_path(const char *path)
{
	int i = 0, length = strlen(path);

	for (; i < length; i++)
		if (path[i] == ' ')
			return ERROR;

	if (path[length - 1] != '/')
		return ERROR;

	return SUCCESS;
}

static int init_lane(void)
{
	int l, ret;

	ret = get_value_from_cli(&l);
	if (ret) {
		pr_err("Invalid input for Lane number\n");
		return ERROR;
	}

	if (l < 0 || l > 1) {
		pr_err("Invalid Lane number\n");
		return ERROR;
	}

	lane = l;
	eom_data.num_lanes = 1;

	return SUCCESS;
}

static int init_voltage_limit(void)
{
	int v, ret;

	ret = get_voltage_value_from_cli(&v);
	if (ret) {
		pr_err("Invalid input for voltage\n");
		return ERROR;
	}

	single_voltage = true;
	voltage = v;

	return SUCCESS;
}

static int init_target_test_count(void)
{
	int t, ret;

	ret = get_value_from_cli(&t);
	if (ret) {
		pr_err("Invalid input for target test count\n");
		return ERROR;
	}

	if (t <= 0 || t > EOM_TARGET_TEST_COUNT_MAX) {
		pr_err("Invalid target test count\n");
		return ERROR;
	}

	target_test_count = t;

	return SUCCESS;
}

static int parse_args(int argc, char *argv[])
{
	int i, c = 0, ret = ERROR;

	if (argc < 2) {
		pr_err("Too less args, try 'ufseom -h'\n");
		return ret;
	}

	if (!strcmp(argv[1], "--version")) {
		printf("ufseom version %s.\n", EOM_VERSION);
		return ret;
	} else if (!strcmp(argv[1], "-h")) {
		printf("%s\n", ufseom_help);
		return ret;
	}

	while (-1 != (c = getopt_long(argc, argv, ufseom_short_options, ufseom_long_options, &i))) {
		switch (c) {
		case 'p':
			eom_data.local_peer = PEER;
			ret = SUCCESS;
			break;
		case 'l':
			eom_data.local_peer = LOCAL;
			ret = SUCCESS;
			break;
		case 'D':
			do_io = true;
			ret = SUCCESS;
			break;
		case 'V':
			verbose = true;
			ret = SUCCESS;
			break;
		case 'L':
			ret = init_lane();
			break;
		case 'v':
			ret = init_voltage_limit();
			break;
		case 'o':
			ret = init_device_path(output_path);
			break;
		case 'd':
			ret = init_device_path(device_path);
			break;
		case 't':
			ret = init_target_test_count();
			break;
		default:
			pr_err("I cannot understand, please try 'ufseom -h'.\n");
			ret = ERROR;
			break;
		}

		if (ret)
			break;
	}

	if (ret)
		return ret;

	if (eom_data.local_peer == INIT) {
		pr_err("Local or peer is not given\n");
		return ERROR;
	}

	if (lane == INIT) {
		lane = 0;
		eom_data.num_lanes = 2;
		pr_err("Lane no. is not given, collect EOM data for all connected lanes\n");
	}

	if (target_test_count == INIT) {
		target_test_count = EOM_TARGET_TEST_COUNT_DEFAULT;
		pr_err("Target test count is not given, use default %d\n", target_test_count);
	}

	if (device_path[0] == '\0') {
		pr_err("Path to bsg device not provided.\n");
		return ERROR;
	}

	if (output_path[0] == '\0') {
		pr_err("Path to output folder not provided.\n");
		return ERROR;
	}

	/* Check space and '/' in the given output path */
	if (check_output_path(output_path)) {
		pr_err("Invalid output path\n");
		return ERROR;
	}

	return SUCCESS;
}

static void init_eom_operation(void)
{
	lane = INIT;
	voltage = INIT;
	target_test_count = INIT;
	tmp_fd = INIT;

	single_voltage = false;

	output_path[0] = '\0';
	device_path[0] = '\0';
}

int main(int argc, char *argv[])
{
	struct EOMData *data = &eom_data;
	struct timespec ts_start, ts_end;
	char tmp_file[1024], output_file[1024], eom_file_name[256], lane_str[8];
	size_t eom_result_size;
	int t, v, l, n, eom_cap, ret;

	init_eom_operation();

	ret = parse_args(argc, argv);
	if (ret)
		return ret;

	bsg_fd = open(device_path, O_RDWR);
	if (bsg_fd < 0) {
		pr_err("Filed to open file %s (%d).\n", device_path, bsg_fd);
		goto free_eom_result;
	}

	/* Get RX_EYEMON_Capability */
	eom_cap = uic_get(bsg_fd, UIC_ARG_MIB_SEL(RX_EYEMON_CAPABILITY, SELECT_RX(lane)), data->local_peer);
	if (eom_cap < 0) {
		pr_err("Failed to read RX_EYEMON_Capability\n");
		ret = ERROR;
		goto close_bsg;
	} else if (!(eom_cap & 0x1)) {
		pr_err("EOM is not supported\n");
		ret = ERROR;
		goto close_bsg;
	}

	if (!do_io)
		goto skip_io_prepare;

	strcpy(tmp_file, output_path);
	strcat(tmp_file, ufseom_tmp_file);
	tmp_fd = open(tmp_file, O_RDWR | O_DIRECT | O_CREAT, S_IWUSR | S_IRUSR);
	if (tmp_fd < 0) {
		pr_err("Failed to open file %s (%d)\n", tmp_file, tmp_fd);
		ret = ERROR;
		goto close_bsg;
	}

	/* Allocate buffer for I/O */
	tmp_buf = memalign(EOM_TEMP_DATA_MEM_ALIGN_SIZE, EOM_TEMP_DATA_SIZE);
	if (!tmp_buf) {
		pr_err("Failed to allocate memory for I/O\n");
		ret = ERROR;
		goto close_tmp;
	}

	/* Prepare temp file for I/O */
	populate_data_pattern(tmp_buf);
	ret = pwrite(tmp_fd, tmp_buf, EOM_TEMP_DATA_SIZE, 0);
	if (ret < 0) {
		pr_err("Failed to create tmp file for I/O\n");
		goto out;
	}

skip_io_prepare:
	/* EOM result file naming rule: local/peer_lane_0/_1_targetestcount.eom */
	snprintf(lane_str, sizeof(lane_str), "%d", lane);
	snprintf(eom_file_name, sizeof(eom_file_name), "%s_lane_%s_ttc_%d.eom",
						      data->local_peer ? "peer" : "local",
						      (data->num_lanes == 2) ? "0_1" : lane_str,
						      target_test_count);
	strcpy(output_file, output_path);
	strcat(output_file, eom_file_name);

	/* Get RX_EYEMON_Timing_MAX_Steps_Capability */
	data->timing_max_steps = uic_get(bsg_fd,
					 UIC_ARG_MIB_SEL(RX_EYEMON_TIMING_MAX_STEPS_CAPABILITY, SELECT_RX(lane)),
					 data->local_peer);
	if (data->timing_max_steps < 0) {
		pr_err("Failed to get RX_EYEMON_Timing_MAX_Steps_Capability\n");
		ret = ERROR;
		goto out;
	}

	/* Get RX_EYEMON_Timing_MAX_Offset_Capability */
	data->timing_max_offset = uic_get(bsg_fd,
					  UIC_ARG_MIB_SEL(RX_EYEMON_TIMING_MAX_OFFSET_CAPABILITY, SELECT_RX(lane)),
					  data->local_peer);
	if (data->timing_max_offset < 0) {
		pr_err("Failed to get RX_EYEMON_Timing_MAX_Offset_Capability\n");
		ret = ERROR;
		goto out;
	}

	/* Get RX_EYEMON_Voltage_MAX_Steps_Capability */
	data->voltage_max_steps = uic_get(bsg_fd,
					  UIC_ARG_MIB_SEL(RX_EYEMON_VOLTAGE_MAX_STEPS_CAPABILITY, SELECT_RX(lane)),
					  data->local_peer);
	if (data->voltage_max_steps < 0) {
		pr_err("Failed to get RX_EYEMON_Voltage_MAX_Steps_Capability\n");
		ret = ERROR;
		goto out;
	}

	/* Get RX_EYEMON_Voltage_MAX_Offset_Capability */
	data->voltage_max_offset = uic_get(bsg_fd,
					   UIC_ARG_MIB_SEL(RX_EYEMON_VOLTAGE_MAX_OFFSET_CAPABILITY, SELECT_RX(lane)),
					   data->local_peer);
	if (data->voltage_max_offset < 0) {
		pr_err("Failed to get RX_EYEMON_Voltage_MAX_Offset_Capability\n");
		ret = ERROR;
		goto out;
	}

	eom_result_count = (data->timing_max_steps * 2 + 1) * (data->voltage_max_steps * 2 + 1) * data->num_lanes;
	eom_result_size = eom_result_count * sizeof(struct eom_result);
	data->er = malloc(eom_result_size);
	if (!data->er) {
		pr_err("Failed to allocate memory for eom_result\n");
		return ERROR;
	}
	memset(data->er, 0, eom_result_size);

	if (verbose) {
		printf("EOM Capabilities:\n");
		printf("TimingMaxSteps %d TimingMaxOffset %d\n", data->timing_max_steps, data->timing_max_offset);
		printf("VoltageMaxSteps %d VoltageMaxOffset %d\n", data->voltage_max_steps, data->voltage_max_offset);
	}

	/* Sanity check for single voltage */
	if (single_voltage && (voltage > data->voltage_max_steps || voltage < -data->voltage_max_steps)) {
		pr_err("Invalid voltage\n");
		goto out;
	}

	printf("Start EOM Scan...\n");
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	/* Main loop starts here */
	for (l = lane, n = data->num_lanes; n > 0; n--, l++) {
		/* Sweep all timings */
		for (t = -data->timing_max_steps; t <= data->timing_max_steps; t++) {
			/* Measure only the given voltage */
			if (single_voltage) {
				ret = eom_scan(data->local_peer, l, t, voltage, target_test_count);
				if (ret) {
					pr_err("Fail to run EOM scan\n");
					goto out;
				}
				continue;
			}

			/* Sweep all voltages */
			for (v = -data->voltage_max_steps; v <= data->voltage_max_steps; v++) {
				ret = eom_scan(data->local_peer, l, t , v, target_test_count);
				if (ret) {
					pr_err("Fail to run EOM scan\n");
					goto out;
				}
			}
		}

		/* Disable Eye Monitor */
		ret = uic_set(bsg_fd, UIC_ARG_MIB_SEL(RX_EYEMON_ENABLE, SELECT_RX(l)), ATTR_SET_NOR, 0, data->local_peer);
		if (ret) {
			pr_err("Filed to disable EOM for lane %d\n", l);
			goto out;
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &ts_end);
	printf("EOM Scan Finished!\n Time elapsed: %ld seconds\n", ts_end.tv_sec - ts_start.tv_sec);

	ret = generate_eom_report(output_file, data);
	if (ret)
		pr_err("Filed to generate EOM report\n");

out:
	if (!tmp_buf)
		free(tmp_buf);
close_tmp:
	if (tmp_fd >= 0)
		close(tmp_fd);
close_bsg:
	close(bsg_fd);
free_eom_result:
	free(data->er);

	return ret;
}
