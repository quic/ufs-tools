// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __UIC_H__
#define __UIC_H__

#include <linux/types.h>
#include <sys/types.h>
#include <stddef.h>
#include "common.h"

#define UIC_ARG_MIB_SEL(attr, sel)	((((attr) & 0xFFFF) << 16) | ((sel) & 0xFFFF))
#define UIC_ARG_MIB(attr)		UIC_ARG_MIB_SEL(attr, 0)

#define UIC_ARG_ATTR_TYPE(t)	(((t) & 0xFF) << 16)
#define UIC_GET_ATTR_ID(v)	(((v) >> 16) & 0xFFFF)

#define SELECT_RX(l)	((l) + 4)
#define SELECT_TX(l)	(l)

#define ATTR_SET_NOR	0
#define ATTR_SET_ST	1

#define MASK_UIC_CONFIG_RESULT_CODE	0xFF

#define UIC_CMD_SIZE (sizeof(__u32) * 4)

#define RX_EYEMON_CAPABILITY			0x00F1
#define RX_EYEMON_TIMING_MAX_STEPS_CAPABILITY	0x00F2
#define RX_EYEMON_TIMING_MAX_OFFSET_CAPABILITY	0x00F3
#define RX_EYEMON_VOLTAGE_MAX_STEPS_CAPABILITY	0x00F4
#define RX_EYEMON_VOLTAGE_MAX_OFFSET_CAPABILITY 0x00F5
#define RX_EYEMON_ENABLE			0x00F6
#define RX_EYEMON_TIMING_STEPS			0x00F7
#define RX_EYEMON_VOLTAGE_STEPS			0x00F8
#define RX_EYEMON_TARGET_TEST_COUNT		0x00F9
#define RX_EYEMON_TESTED_COUNT			0x00FA
#define RX_EYEMON_ERROR_COUNT			0x00FB
#define RX_EYEMON_START				0x00FC

#define PA_PWRMODE				0x1571
#define PA_TXHSADAPTTYPE			0x15D4
#define PA_RXGEAR				0x1583
#define RX_HSRATE_SERIES			0xA2

#define RX_EYEMON_START_MASK			0x1

#define QCOM_DME_VS_UNIPRO_STATE		0xD000

#define QCOM_DME_VS_UNIPRO_STATE_MASK		0x7
#define QCOM_DME_VS_UNIPRO_STATE_LINK_UP	0x2

/* Adpat type for PA_TXHSADAPTTYPE attribute */
#define PA_REFRESH_ADAPT       0x00
#define PA_INITIAL_ADAPT       0x01
#define PA_NO_ADAPT            0x03

#define PA_HS_MODE_A		1
#define PA_HS_MODE_B		2

enum uic_operation_mode {
	GET,
	SET,
};

enum uic_operation_target {
	LOCAL,
	PEER,
};

enum uic_operation_dir {
	TX,
	RX,
};

struct uic_operation {
	int mode; /* get/set */
	int dir; /* Tx/Rx */
	int local_peer;
	int lane;
	int attr_id;
	__u32 data;
};

struct uic_command {
	__u32 command;
	__u32 argument1;
	__u32 argument2;
	__u32 argument3;
};

enum uic_cmds {
	UIC_CMD_DME_GET = 0x01,
	UIC_CMD_DME_SET = 0x02,
	UIC_CMD_DME_PEER_GET = 0x03,
	UIC_CMD_DME_PEER_SET = 0x04,
};

static struct ufs_characteristics unipro_mphy_attrs[] = {
	{0x0001, "TX_HSMODE_Capability"},
	{0x0002, "TX_HSGEAR_Capability"},
	{0x0003, "TX_PWMG0_Capability"},
	{0x0004, "TX_PWMGEAR_Capability"},
	{0x0005, "TX_Amplitude_Capability"},
	{0x0006, "TX_ExternalSYNC_Capability"},
	{0x0007, "TX_HS_Unterminated_LINE_Drive_Capability"},
	{0x0008, "TX_LS_Terminated_LINE_Drive_Capability"},
	{0x0009, "TX_Min_SLEEP_NoConfig_Time_Capability"},
	{0x000A, "TX_Min_STALL_NoConfig_Time_Capability"},
	{0x000B, "TX_Min_SAVE_Config_Time_Capability"},
	{0x000C, "TX_REF_CLOCK_SHARED_Capability"},
	{0x000D, "TX_PHY_MajorMinor_Release_Capability"},
	{0x000E, "TX_PHY_Editorial_Release_Capability"},
	{0x000F, "TX_Hibern8Time_Capability"},
	{0x0010, "TX_Advanced_Granularity_Capability"},
	{0x0011, "TX_Advanced_Hibern8Time_Capability"},
	{0x0012, "TX_HS_Equalizer_Setting_Capability"},
	{0x0021, "TX_MODE"},
	{0x0022, "TX_HSRATE_Series"},
	{0x0023, "TX_HSGEAR"},
	{0x0024, "TX_PWMGEAR"},
	{0x0025, "TX_Amplitude"},
	{0x0026, "TX_HS_SlewRate"},
	{0x0027, "TX_SYNC_Source"},
	{0x0028, "TX_HS_SYNC_LENGTH"},
	{0x0029, "TX_HS_PREPARE_LENGTH"},
	{0x002A, "TX_LS_PREPARE_LENGTH"},
	{0x002B, "TX_HIBERN8_Control"},
	{0x002C, "TX_LCC_Enable"},
	{0x002D, "TX_PWM_BURST_Closure_Extension"},
	{0x002E, "TX_BYPASS_8B10B_Enable"},
	{0x002F, "TX_DRIVER_POLARITY"},
	{0x0030, "TX_HS_Unterminated_LINE_Drive_Enable"},
	{0x0031, "TX_LS_Terminated_LINE_Drive_Enable"},
	{0x0032, "TX_LCC_Sequencer"},
	{0x0033, "TX_Min_ActivateTime"},
	{0x0034, "TX_PWM_G6_G7_SYNC_LENGTH"},
	{0x0035, "TX_Advanced_Granularity_Step"},
	{0x0036, "TX_Advanced_Granularity"},
	{0x0037, "TX_HS_Equalizer_Setting"},
	{0x0038, "TX_Min_SLEEP_NoConfig_Time"},
	{0x0039, "TX_Min_STALL_NoConfig_Time"},
	{0x003A, "TX_HS_ADAPT_LENGTH"},
	{0x0041, "TX_FSM_State"},
	{0x0061, "MC_Output_Amplitude"},
	{0x0062, "MC_HS_Unterminated_Enable"},
	{0x0063, "MC_LS_Terminated_Enable"},
	{0x0064, "MC_HS_Unterminated_LINE_Drive_Enable"},
	{0x0065, "MC_LS_Terminated_LINE_Drive_Enable"},

	{0x0081, "RX_HSMODE_Capability"},
	{0x0082, "RX_HSGEAR_Capability"},
	{0x0083, "RX_PWMG0_Capability"},
	{0x0084, "RX_PWMGEAR_Capability"},
	{0x0085, "RX_HS_Unterminated_Capability"},
	{0x0086, "RX_LS_Terminated_Capability"},
	{0x0087, "RX_Min_SLEEP_NoConfig_Time_Capability"},
	{0x0088, "RX_Min_STALL_NoConfig_Time_Capability"},
	{0x0089, "RX_Min_SAVE_Config_Time_Capability"},
	{0x008A, "RX_REF_CLOCK_SHARED_Capability"},
	{0x008B, "RX_HS_G1_SYNC_LENGTH_Capability"},
	{0x008C, "RX_HS_G1_PREPARE_LENGTH_Capability"},
	{0x008D, "RX_LS_PREPARE_LENGTH_Capability"},
	{0x008E, "RX_PWM_Burst_Closure_Length_Capability"},
	{0x008F, "RX_Min_ActivateTime_Capability"},
	{0x0090, "RX_PHY_MajorMinor_Release_Capability"},
	{0x0091, "RX_PHY_Editorial_Release_Capability"},
	{0x0092, "RX_Hibern8Time_Capability"},
	{0x0093, "RX_PWM_G6_G7_SYNC_LENGTH_Capability"},
	{0x0094, "RX_HS_G2_SYNC_LENGTH_Capability"},
	{0x0095, "RX_HS_G3_SYNC_LENGTH_Capability"},
	{0x0096, "RX_HS_G2_PREPARE_LENGTH_Capability"},
	{0x0097, "RX_HS_G3_PREPARE_LENGTH_Capability"},
	{0x0098, "RX_Advanced_Granularity_Capability"},
	{0x0099, "RX_Advanced_Hibern8Time_Capability"},
	{0x009A, "RX_Advanced_Min_ActivateTime_Capability"},
	{0x009B, "RX_HS_G4_SYNC_LENGTH_Capability"},
	{0x009C, "RX_HS_G4_PREPARE_LENGTH_Capability"},
	{0x009D, "RX_HS_Equalizer_Setting_Capability"},
	{0x009E, "RX_HS_ADAPT_REFRESH_Capability"},
	{0x009F, "RX_HS_ADAPT_INITIAL_Capability"},
	{0x00A1, "RX_MODE"},
	{0x00A2, "RX_HSRATE_Series"},
	{0x00A3, "RX_HSGEAR"},
	{0x00A4, "RX_PWMGEAR"},
	{0x00A5, "RX_LS_Terminated_Enable"},
	{0x00A6, "RX_HS_Unterminated_Enable"},
	{0x00A7, "RX_Enter_HIBERN8"},
	{0x00A8, "RX_BYPASS_8B10B_Enable"},
	{0x00A9, "RX_Termination_Force_Enable"},
	{0x00AA, "RX_ADAPT_Control"},
	{0x00AB, "RX_RECEIVER_POLARITY"},
	{0x00AC, "RX_HS_ADAPT_LENGTH"},
	{0x00C1, "RX_FSM_State"},

	{0x00D1, "OMC_TYPE_Capability"},
	{0x00D2, "MC_HSMODE_Capability"},
	{0x00D3, "MC_HSGEAR_Capability"},
	{0x00D4, "MC_HS_START_TIME_Var_Capability"},
	{0x00D5, "MC_HS_START_TIME_Range_Capability"},
	{0x00D6, "MC_RX_SA_Capability"},
	{0x00D7, "MC_HS_LA_Capability"},
	{0x00D8, "MC_HS_LS_PREPARE_LENGTH"},
	{0x00D9, "MC_PWMG0_Capability"},
	{0x00DA, "MC_PWMGEAR_Capability"},
	{0x00DB, "MC_LS_Terminated_Capability"},
	{0x00DC, "MC_HS_Unterminated_Capability"},
	{0x00DD, "MC_LS_Terminated_LINE_Drive_Capability"},
	{0x00DE, "MC_HS_Unterminated_LINE_Drive_Capabilit"},
	{0x00DF, "MC_MFG_ID_Part1"},
	{0x00E0, "MC_MFG_ID_Part2"},
	{0x00E1, "MC_PHY_MajorMinor_Release_Capability"},
	{0x00E2, "MC_PHY_Editorial_Release_Capability"},
	{0x00E3, "MC_Vendor_Info_Part1"},
	{0x00E4, "MC_Vendor_Info_Part2"},
	{0x00E5, "MC_Vendor_Info_Part3"},
	{0x00E6, "MC_Vendor_Info_Part4"},

	{0x00F1, "RX_EYEMON_Capability"},
	{0x00F2, "RX_EYEMON_Timing_MAX_Steps_Capability"},
	{0x00F3, "RX_EYEMON_Timing_MAX_Offset_Capability"},
	{0x00F4, "RX_EYEMON_Voltage_MAX_Steps_Capability"},
	{0x00F5, "RX_EYEMON_Voltage_MAX_Offset_Capability"},
	{0x00F6, "RX_EYEMON_Enable"},
	{0x00F7, "RX_EYEMON_Timing_Steps"},
	{0x00F8, "RX_EYEMON_Voltage_Steps"},
	{0x00F9, "RX_EYEMON_Target_Test_Count"},
	{0x00FA, "RX_EYEMON_Tested_Count"},
	{0x00FB, "RX_EYEMON_Error_Count"},
	{0x00FC, "RX_EYEMON_Start"},

	{0x1560, "PA_ActiveTxDataLanes"},
	{0x1564, "PA_TxTrailingClocks"},
	{0x1580, "PA_ActiveRxDataLanes"},
	{0x1500, "PA_PHY_Type"},
	{0x1520, "PA_AvailTxDataLanes"},
	{0x1540, "PA_AvailRxDataLanes"},
	{0x1543, "PA_MinRxTrailingClocks"},
	{0x1567, "PA_TxPWRStatus"},
	{0x1582, "PA_RxPWRStatus"},
	{0x15A0, "PA_RemoteVerInfo"},
	{0x1552, "PA_TxHsG1SyncLength"},
	{0x1553, "PA_TxHsG1PrepareLength"},
	{0x1554, "PA_TxHsG2SyncLength"},
	{0x1555, "PA_TxHsG2PrepareLength"},
	{0x1556, "PA_TxHsG3SyncLength"},
	{0x1557, "PA_TxHsG3PrepareLength"},
	{0x155A, "PA_TxMk2Extension"},
	{0x155B, "PA_PeerScrambling"},
	{0x155C, "PA_TxSkip"},
	{0x155D, "PA_TxSkipPeriod"},
	{0x155E, "PA_Local_TX_LCC_Enable"},
	{0x155F, "PA_Peer_TX_LCC_Enable"},
	{0x1561, "PA_ConnectedTxDataLanes"},
	{0x1568, "PA_TxGear"},
	{0x1569, "PA_TxTermination"},
	{0x156A, "PA_HSSeries"},
	{0x1571, "PA_PWRMode"},
	{0x1581, "PA_ConnectedRxDataLanes"},
	{0x1583, "PA_RxGear"},
	{0x1584, "PA_RxTermination"},
	{0x1585, "PA_Scrambling"},
	{0x1586, "PA_MaxRxPWMGear"},
	{0x1587, "PA_MaxRxHSGear"},
	{0x1590, "PA_PACPReqTimeout"},
	{0x1591, "PA_PACPReqEoBTimeout"},
	{0x15A1, "PA_LogicalLaneMap"},
	{0x15A2, "PA_SleepNoConfigTime"},
	{0x15A3, "PA_StallNoConfigTime"},
	{0x15A4, "PA_SaveConfigTime"},
	{0x15A5, "PA_RxHSUnterminationCapability"},
	{0x15A6, "PA_RxLSTerminationCapability"},
	{0x15A7, "PA_Hibern8Time"},
	{0x15A8, "PA_TActivate"},
	{0x15A9, "PA_LocalVerInfo"},
	{0x15AA, "PA_Granularity"},
	{0x15AB, "PA_MK2ExtensionGuardBand"},
	{0x15B0, "PA_PWRModeUserData"},
	{0x15C0, "PA_PACPFrameCount"},
	{0x15C1, "PA_PACPErrorCount"},
	{0x15C2, "PA_PHYTestControl"},
	{0x15D0, "PA_TxHsG4SyncLength"},
	{0x15D1, "PA_TxHsG4PrepareLength"},
	{0x15D2, "PA_PeerRxHsAdaptRefresh"},
	{0x15D3, "PA_PeerRxHsAdaptInitial"},
	{0x15D4, "PA_TxHsAdaptType"},
	{0x15D5, "PA_AdaptAfterLRSTInPA_INIT"},

	{0x5100, "DME_TX_DATA_OFL"},
	{0x5101, "DME_TX_NAC_RECEIVED"},
	{0x5102, "DME_TX_QoS_COUNT"},
	{0x5103, "DME_TX_DL_LM_ERROR"},
	{0x5110, "DME_RX_DATA_OFL"},
	{0x5111, "DME_RX_CRC_ERROR"},
	{0x5112, "DME_RX_QoS_COUNT"},
	{0x5113, "DME_RX_DL_LM_ERROR"},
	{0x5120, "DME_TXRX_DATA_OFL"},
	{0x5121, "DME_TXRX_PA_INIT_REQUEST"},
	{0x5122, "DME_TXRX_QoS_COUNT"},
	{0x5123, "DME_TXRX_DL_LM_ERROR"},
	{0x5130, "DME_QoS_ENABLE"},
	{0x5131, "DME_QoS_STATUS"},
	{INIT, NULL},
};

int init_uic_operation(int argc, char *argv[], void *op_data);
int do_uic_operation(void *op_data);
int uic_get(int fd, __u32 attr_sel, int peer);
int uic_set(int fd, __u32 attr_sel, __u8 attr_set, __u32 mib_val, int peer);
#endif /* __UIC_H__ */
