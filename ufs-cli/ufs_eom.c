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
#define EOM_SUPPORTED_MIN_GEAR		4
#define EOM_TIMING_VOLTAGE_INIT		0xFF

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
	int gear;
	int rate;

	struct eom_result *er;
} eom_data;

enum eom_output_target {
	EOM_OUTPUT_TARGET_UNSET = 0,
	EOM_OUTPUT_TARGET_DIR,
	EOM_OUTPUT_TARGET_FILE,
	EOM_OUTPUT_TARGET_STDOUT,
};

enum eom_output_format {
	EOM_OUTPUT_FORMAT_TEXT = 0,
	EOM_OUTPUT_FORMAT_JSON,
};

static char output_path[DEVICE_PATH_NAME_SIZE_MAX];
static char device_path[DEVICE_PATH_NAME_SIZE_MAX];
const static char *ufseom_tmp_file = "ufseom_tmp_data";
static char *tmp_buf;

/* Global EOM control parameters */
static int lane;
static int voltage_low;
static int voltage_high;
static int timing_left;
static int timing_right;
static int target_test_count;
static int eom_result_count;
static int tmp_fd, bsg_fd;
static bool do_io;
static bool verbose;
static int output_target;
static int output_format;

static FILE *runtime_log_stream(void)
{
	if (output_target == EOM_OUTPUT_TARGET_STDOUT && output_format == EOM_OUTPUT_FORMAT_JSON)
		return stderr;

	return stdout;
}

const char *ufseom_help =
	"\nufseom cli :\n\n"
	"ufseom [-p | --peer | -l | --local] [-D | --data] [-L | --lane <lane no.>] [--voltage-low <low voltage value>] [--voltage-high <high voltage value>] [--timing-left <left timing value>] [--timing-right <right timing value>] [-T | --target <target test count>] [-f | --format <text|json>] [-o | --output <output>] [-d | --device <device>]\n\n"
	"-h : help\n"
	"--version : UFS EOM version\n"
	"-p | --peer : peer\n"
	"-l | --local : local\n"
	"-D | --data : explicitly do I/O transfer with random data patterns to stress the link while EOM is running\n"
	"-L | --lane : lane no. 0 or 1, collect EOM data for all connected lanes if not given\n"
	"--voltage-low : collect EOM data from low voltage to high voltage, if it is not given, it defaults to -voltage_max_steps\n"
	"--voltage-high : collect EOM data from low voltage to high voltage, if it is not given, it defaults to voltage_max_steps\n"
	"--timing-left : collect EOM data from left timing to right timing, if it is not given, it defaults to -timing_max_steps\n"
	"--timing-right : collect EOM data from left timing to right timing, if it is not given, it defaults to timing_max_steps\n"
	"-t | --target : target test count\n"
	"-f | --format : output format. supported values: text, json\n"
	"-o | --output : report output destination. '-' for stdout, file path, or folder path ending with '/'\n"
	"-V | --verbose : enable detailed EOM information and logs\n"
	"-d | --device : path to ufs-bsg device\n\n"
	"Example:\n"
	"  1. Collect EOM data for local Rx:\n"
	"  ufseom -l -D -o /data/ -d /dev/ufs-bsg0\n"
	"  2. Collect EOM data for peer Rx:\n"
	"  ufseom -p -D -o /data/ -d /dev/ufs-bsg0\n"
	"  3. Collect EOM data for local Rx with voltage 0 only:\n"
	"  ufseom -l -D --voltage-low 0 --voltage-high 0 -o /data/ -d /dev/ufs-bsg0\n"
	"  4. Collect EOM data for local Rx from voltage 0 to 8:\n"
	"  ufseom -l -D --voltage-low 0 --voltage-high 8 -o /data/ -d /dev/ufs-bsg0\n"
	"  5. Collect EOM data for local Rx for voltage from 0 to 8 and timing from -1 to 1:\n"
	"  ufseom -l -D --voltage-low 0 --voltage-high 8 --timing-left -1 --timing-right 1 -o /data/ -d /dev/ufs-bsg0\n"
	"  6. Print JSON report to stdout:\n"
	"  ufseom -l -f json -o - -d /dev/ufs-bsg0\n\n"
	"Note that to get accurate EOM data, user should disable UFS driver low power mode features,\n"
	"such as Clock Scaling, Clock Gating, Suspend/Resume and Auto Hibernate. For example:\n"
	"$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkscale_enable\n"
	"$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkgate_enable\n"
	"$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/auto_hibern8\n\n"
	"In addition, ufseom changes UFS Host and/or UFS device UIC layer execution environments,\n"
	"although UFS EOM is not supposed to disturb normal I/O traffics, it is recommanded to\n"
	"reboot the system after use ufseom.\n";

static char *ufseom_short_options = "plDL:t:f:o:d:V";

static struct option ufseom_long_options[] = {
	{"peer", no_argument, NULL, 'p'}, /* UFS device */
	{"local", no_argument, NULL, 'l'}, /* UFS host*/
	{"data", no_argument, NULL, 'D'}, /* Do I/Os */
	{"lane", required_argument, NULL, 'L'}, /* Lane */
	{"target", required_argument, NULL, 't'}, /* Target test count */
	{"format", required_argument, NULL, 'f'}, /* Output format */
	{"output", required_argument, NULL, 'o'}, /* EOM result output path */
	{"device", required_argument, NULL, 'd'}, /* UFS BSG device path. For example: /dev/ufs-bsg0 */
	{"verbose", no_argument, NULL, 'V'}, /* Enable detailed EOM information and logs */
	{"voltage-low", required_argument, NULL, 1}, /* Voltage low */
	{"voltage-high", required_argument, NULL, 2}, /* Voltage high */
	{"timing-left", required_argument, NULL, 3}, /* Timing left*/
	{"timing-right", required_argument, NULL, 4}, /* Timing right */
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
	uint64_t seed = rand();
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

static int get_voltage_timing_value_from_cli(int *val)
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

	/* Write to excercise peer device's Rx */
	populate_data_pattern(tmp_buf);
	ret = pwrite(tmp_fd, tmp_buf, EOM_TEMP_DATA_SIZE, 0);
	if (ret < 0) {
		pr_err("Failed to write tmp file\n");
		return ERROR;
	}

	if (peer == LOCAL) {
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
				fprintf(runtime_log_stream(),
				       "lane: %d timing: %d voltage: %d error count: %d [tested_count: %d]\n", lane, timing, volt,
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

static void json_write_string(FILE *file, const char *str)
{
	const unsigned char *p = (const unsigned char *)str;

	fputc('"', file);
	for (; *p; p++) {
		switch (*p) {
		case '\"':
			fputs("\\\"", file);
			break;
		case '\\':
			fputs("\\\\", file);
			break;
		case '\b':
			fputs("\\b", file);
			break;
		case '\f':
			fputs("\\f", file);
			break;
		case '\n':
			fputs("\\n", file);
			break;
		case '\r':
			fputs("\\r", file);
			break;
		case '\t':
			fputs("\\t", file);
			break;
		default:
			if (*p < 0x20)
				fprintf(file, "\\u%04x", *p);
			else
				fputc(*p, file);
			break;
		}
	}
	fputc('"', file);
}

static int generate_eom_report_text(FILE *file, struct EOMData *data, const char *mname, const char *pname, const char *pver)
{
	int i;

	fprintf(file, "UFS %s Side Eye Monitor Start\n", data->local_peer ? "Device" : "Host");
	fprintf(file, "- - - - UFS INQUIRY ID: %s %s %s\n", mname, pname, pver);
	fprintf(file, "- - - - UFS Gear Speed: HS-G%d Rate-%c\n", data->gear, data->rate == PA_HS_MODE_A ? 'A' : 'B');
	fprintf(file, "EOM Capabilities:\n");
	fprintf(file, "TimingMaxSteps %d TimingMaxOffset %d\n", data->timing_max_steps, data->timing_max_offset);
	fprintf(file, "VoltageMaxSteps %d VoltageMaxOffset %d\n\n", data->voltage_max_steps, data->voltage_max_offset);

	for (i = 0; i < data->data_cnt; i++)
		fprintf(file, "lane: %d timing: %d voltage: %d error count: %d\n", data->er[i].lane, data->er[i].timing,
											   data->er[i].volt, data->er[i].error_cnt);

	return SUCCESS;
}

static int generate_eom_report_json(FILE *file, struct EOMData *data, const char *mname, const char *pname, const char *pver)
{
	char note[MANUFACTURER_NAME_STRING_DESC_SIZE + PRODUCT_NAME_STRING_DESC_SIZE +
		  PRODUCT_REVISION_LEVEL_STRING_DESC_SIZE + 4];
	double time_scale, voltage_scale;
	int l, n, i;

	if (snprintf(note, sizeof(note), "%s %s %s", mname, pname, pver) >= sizeof(note)) {
		pr_err("Device note is too long\n");
		return ERROR;
	}

	time_scale = (data->timing_max_offset * 0.01) / data->timing_max_steps;
	voltage_scale = (data->voltage_max_offset * 10.0) / data->voltage_max_steps;
	voltage_scale = ((int)(voltage_scale * 100)) / 100.0;

	fprintf(file, "{\n");
	fprintf(file, "  \"version\": \"1.0.0\",\n");
	fprintf(file, "  \"results\": [\n");
	fprintf(file, "    {\n");
	fprintf(file, "      \"interface\": \"%s\",\n", data->local_peer ? "Device UFS" : "Host UFS");
	fprintf(file, "      \"instance\": 0,\n");
	fprintf(file, "      \"time_scale\": %.5f,\n", time_scale);
	fprintf(file, "      \"time_units\": \"UI\",\n");
	fprintf(file, "      \"voltage_scale\": %.2f,\n", voltage_scale);
	fprintf(file, "      \"voltage_units\": \"mV\",\n");
	fprintf(file, "      \"note\": ");
	json_write_string(file, note);
	fprintf(file, ",\n");
	fprintf(file, "      \"lanes\": [\n");

	for (l = lane, n = data->num_lanes; n > 0; n--, l++) {
		fprintf(file, "        {\n");
		fprintf(file, "          \"lane_number\": %d,\n", l);
		fprintf(file, "          \"note\": \"\",\n");
		fprintf(file, "          \"eye\": [\n");

		for (i = 0; i < data->data_cnt; i++) {
			if (data->er[i].lane != l)
				continue;

			fprintf(file, "            [%d, %d, %d]", data->er[i].timing, data->er[i].volt, data->er[i].error_cnt);

			if (i < data->data_cnt - 1) {
				int j;
				bool has_more_for_lane = false;

				for (j = i + 1; j < data->data_cnt; j++) {
					if (data->er[j].lane == l) {
						has_more_for_lane = true;
						break;
					}
				}

				if (has_more_for_lane)
					fprintf(file, ",");
			}
			fprintf(file, "\n");
		}

		fprintf(file, "          ]\n");
		fprintf(file, "        }");
		if (n > 1)
			fprintf(file, ",");
		fprintf(file, "\n");
	}

	fprintf(file, "      ]\n");
	fprintf(file, "    }\n");
	fprintf(file, "  ]\n");
	fprintf(file, "}\n");

	return SUCCESS;
}

static int generate_eom_report(char *eom_file, struct EOMData *data)
{
	char mname[MANUFACTURER_NAME_STRING_DESC_SIZE];
	char pname[PRODUCT_NAME_STRING_DESC_SIZE];
	char pver[PRODUCT_REVISION_LEVEL_STRING_DESC_SIZE];
	int ret;
	FILE *file;
	bool is_stdout = output_target == EOM_OUTPUT_TARGET_STDOUT;

	ret = get_device_info(mname, pname, pver);
	if (ret)
		return ret;

	if (is_stdout) {
		file = stdout;
	} else {
		file = fopen(eom_file, "w");
		if (!file) {
			pr_err("Failed to create EOM result file %s\n", eom_file);
			return ERROR;
		}
	}

	if (output_format == EOM_OUTPUT_FORMAT_JSON)
		ret = generate_eom_report_json(file, data, mname, pname, pver);
	else
		ret = generate_eom_report_text(file, data, mname, pname, pver);

	if (!is_stdout)
		fclose(file);

	if (!ret && !is_stdout)
		printf("EOM results saved to %s\n", eom_file);

	return ret;
}

static int init_output_target(void)
{
	size_t length;

	if (optarg[0] == '\0') {
		pr_err("Output path cannot be empty\n");
		return ERROR;
	}

	if (!strcmp(optarg, "-")) {
		output_target = EOM_OUTPUT_TARGET_STDOUT;
		output_path[0] = '\0';
		return SUCCESS;
	}

	if (strlen(optarg) >= DEVICE_PATH_NAME_SIZE_MAX) {
		pr_err("Output path is too long\n");
		return ERROR;
	}

	strcpy(output_path, optarg);
	length = strlen(output_path);
	if (output_path[length - 1] == '/')
		output_target = EOM_OUTPUT_TARGET_DIR;
	else
		output_target = EOM_OUTPUT_TARGET_FILE;

	return SUCCESS;
}

static int init_output_format(void)
{
	if (!strcmp(optarg, "text")) {
		output_format = EOM_OUTPUT_FORMAT_TEXT;
		return SUCCESS;
	}

	if (!strcmp(optarg, "json")) {
		output_format = EOM_OUTPUT_FORMAT_JSON;
		return SUCCESS;
	}

	pr_err("Invalid format '%s'. Supported values: text, json\n", optarg);

	return ERROR;
}

static int get_tmp_file_path(char *tmp_file, size_t size)
{
	const char *tmp_dir = "/tmp/";

	if (output_target == EOM_OUTPUT_TARGET_DIR)
		tmp_dir = output_path;

	if (snprintf(tmp_file, size, "%s%s", tmp_dir, ufseom_tmp_file) >= size) {
		pr_err("Temp file path is too long\n");
		return ERROR;
	}

	return SUCCESS;
}

static int get_output_file_path(char *output_file, size_t size, const char *eom_file_name)
{
	if (output_target == EOM_OUTPUT_TARGET_STDOUT) {
		output_file[0] = '\0';
		return SUCCESS;
	}

	if (output_target == EOM_OUTPUT_TARGET_FILE) {
		if (snprintf(output_file, size, "%s", output_path) >= size) {
			pr_err("Output file path is too long\n");
			return ERROR;
		}

		return SUCCESS;
	}

	if (snprintf(output_file, size, "%s%s", output_path, eom_file_name) >= size) {
		pr_err("Output file path is too long\n");
		return ERROR;
	}

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
			case 'f':
				ret = init_output_format();
				break;
			case 'o':
				ret = init_output_target();
				break;
			case 'd':
				ret = init_device_path(device_path);
			break;
		case 't':
			ret = init_target_test_count();
			break;
		case 1:
			ret = get_voltage_timing_value_from_cli(&voltage_low);
			break;
		case 2:
			ret = get_voltage_timing_value_from_cli(&voltage_high);
			break;
		case 3:
			ret = get_voltage_timing_value_from_cli(&timing_left);
			break;
		case 4:
			ret = get_voltage_timing_value_from_cli(&timing_right);
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

	if (output_target == EOM_OUTPUT_TARGET_UNSET) {
		pr_err("Output destination not provided.\n");
		return ERROR;
	}

	return SUCCESS;
}

static int timing_voltage_sanity_check(struct EOMData *data)
{
	int ret = ERROR;

	if (voltage_low == EOM_TIMING_VOLTAGE_INIT)
		voltage_low = -data->voltage_max_steps;

	if (voltage_high == EOM_TIMING_VOLTAGE_INIT)
		voltage_high = data->voltage_max_steps;

	if (timing_left == EOM_TIMING_VOLTAGE_INIT)
		timing_left = -data->timing_max_steps;

	if (timing_right == EOM_TIMING_VOLTAGE_INIT)
		timing_right = data->timing_max_steps;

	/* Sanity check for voltage range*/
	if (voltage_low > data->voltage_max_steps || voltage_low < -data->voltage_max_steps ||
				voltage_high > data->voltage_max_steps || voltage_high < -data->voltage_max_steps) {
		pr_err("Invalid voltage range: hardware limits: [-%d, %d]\n", data->voltage_max_steps, data->voltage_max_steps);
		ret = ERROR;
		goto out;
	}

	/* Sanity check for timing range*/
	if (timing_left > data->timing_max_steps || timing_left < -data->timing_max_steps ||
				timing_right > data->timing_max_steps || timing_right < -data->timing_max_steps) {
		pr_err("Invalid timing range: hardware limits: [-%d, %d]\n", data->timing_max_steps, data->timing_max_steps);
		ret = ERROR;
		goto out;
	}

	if (voltage_low > voltage_high) {
		pr_err("Voltage high is less than voltage low\n");
		ret = ERROR;
		goto out;
	}

	if (timing_left > timing_right) {
		pr_err("Timing right is less than timing left\n");
		ret = ERROR;
		goto out;
	}

	ret = SUCCESS;
out:
	return ret;
}

static void init_eom_operation(void)
{
	lane = INIT;
	target_test_count = INIT;
	tmp_fd = INIT;
	voltage_low = EOM_TIMING_VOLTAGE_INIT;
	voltage_high = EOM_TIMING_VOLTAGE_INIT;
	timing_left = EOM_TIMING_VOLTAGE_INIT;
	timing_right = EOM_TIMING_VOLTAGE_INIT;

	output_path[0] = '\0';
	device_path[0] = '\0';
	output_target = EOM_OUTPUT_TARGET_UNSET;
	output_format = EOM_OUTPUT_FORMAT_TEXT;
}

int main(int argc, char *argv[])
{
	struct EOMData *data = &eom_data;
	struct timespec ts_start, ts_end;
	char tmp_file[1024], output_file[1024], eom_file_name[256], lane_str[8];
	const char *report_ext;
	size_t eom_result_size;
	int t, v, l, n, eom_cap, cur_gear, cur_rate, ret;

	init_eom_operation();

	ret = parse_args(argc, argv);
	if (ret)
		return ret;

	bsg_fd = open(device_path, O_RDWR);
	if (bsg_fd < 0) {
		pr_err("Filed to open file %s (%d).\n", device_path, bsg_fd);
		return ERROR;
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

	/* Get PA_RxGear */
	cur_gear = uic_get(bsg_fd, UIC_ARG_MIB_SEL(PA_RXGEAR, SELECT_RX(0)), 0);
	if (cur_gear < 0) {
		pr_err("Failed to get current gear from PA_RXGEAR\n");
		ret = ERROR;
		goto close_bsg;
	} else if (verbose) {
		fprintf(runtime_log_stream(), "PA_RxGear: %d\n", cur_gear);
	}

	if (cur_gear < EOM_SUPPORTED_MIN_GEAR) {
		pr_err("EOM is not supported at current gear %d\n", cur_gear);
		ret = ERROR;
		goto close_bsg;
	}

	data->gear = cur_gear;

	/* Get RX_HSRATE_Series */
	cur_rate = uic_get(bsg_fd, UIC_ARG_MIB_SEL(RX_HSRATE_SERIES, SELECT_RX(0)), 0);
	if (cur_rate < 0) {
		pr_err("Failed to get current rate from RX_HSRATE_Series\n");
		ret = ERROR;
		goto close_bsg;
	} else if (verbose) {
		fprintf(runtime_log_stream(), "RX_HSRATE_Series: %d\n", cur_rate);
	}

	data->rate = cur_rate;

	if (!do_io)
		goto skip_io_prepare;

	ret = get_tmp_file_path(tmp_file, sizeof(tmp_file));
	if (ret) {
		ret = ERROR;
		goto close_bsg;
	}

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

skip_io_prepare:
	/* EOM result file naming rule: local/peer_lane_0/_1_targetestcount.eom */
	report_ext = output_format == EOM_OUTPUT_FORMAT_JSON ? "json" : "eom";
	snprintf(lane_str, sizeof(lane_str), "%d", lane);
	snprintf(eom_file_name, sizeof(eom_file_name), "%s_lane_%s_gear_%d_ttc_%d.%s",
							      data->local_peer ? "peer" : "local",
							      (data->num_lanes == 2) ? "0_1" : lane_str,
							      cur_gear, target_test_count,
							      report_ext);

	ret = get_output_file_path(output_file, sizeof(output_file), eom_file_name);
	if (ret) {
		ret = ERROR;
		goto out;
	}

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

	if (verbose) {
		fprintf(runtime_log_stream(), "EOM Capabilities:\n");
		fprintf(runtime_log_stream(), "TimingMaxSteps %d TimingMaxOffset %d\n",
			data->timing_max_steps, data->timing_max_offset);
		fprintf(runtime_log_stream(), "VoltageMaxSteps %d VoltageMaxOffset %d\n",
			data->voltage_max_steps, data->voltage_max_offset);
	}

	/* Sanity check for voltage and timing range*/
	ret = timing_voltage_sanity_check(data);
	if (ret) {
		pr_err("Timing or voltage is invalid\n");
		ret = ERROR;
		goto out;
	}

	if (verbose)
		fprintf(runtime_log_stream(), "timing_left:%d, timing_right:%d, voltage_low:%d, voltage_high:%d\n",
						timing_left, timing_right, voltage_low, voltage_high);

	eom_result_count = (timing_right - timing_left + 1) * (voltage_high - voltage_low + 1) * data->num_lanes;
	eom_result_size = eom_result_count * sizeof(struct eom_result);
	data->er = malloc(eom_result_size);
	if (!data->er) {
		pr_err("Failed to allocate memory for eom_result\n");
		ret = ERROR;
		goto out;
	}
	memset(data->er, 0, eom_result_size);

	/* Set seed for a new sequence of pseudo-random integers */
	srand((unsigned)clock());

	fprintf(runtime_log_stream(), "Start EOM Scan...\n");
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	/* Main loop starts here */
	for (l = lane, n = data->num_lanes; n > 0; n--, l++) {
		for (t = timing_left; t <= timing_right; t++) {
			for (v = voltage_low; v <= voltage_high; v++) {
				ret = eom_scan(data->local_peer, l, t, v, target_test_count);
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
	fprintf(runtime_log_stream(), "EOM Scan Finished!\n Time elapsed: %ld seconds\n", ts_end.tv_sec - ts_start.tv_sec);

	ret = generate_eom_report(output_file, data);
	if (ret)
		pr_err("Filed to generate EOM report\n");

out:
	free(data->er);
	free(tmp_buf);
close_tmp:
	close(tmp_fd);
close_bsg:
	close(bsg_fd);

	return ret;
}
