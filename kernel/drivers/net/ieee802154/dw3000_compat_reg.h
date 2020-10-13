/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2020 Qorvo US, Inc.
 *
 * This software is provided under the GNU General Public License, version 2
 * (GPLv2), as well as under a Qorvo commercial license.
 *
 * You may choose to use this software under the terms of the GPLv2 License,
 * version 2 ("GPLv2"), as published by the Free Software Foundation.
 * You should have received a copy of the GPLv2 along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * This program is distributed under the GPLv2 in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GPLv2 for more
 * details.
 *
 * If you cannot meet the requirements of the GPLv2, you may not use this
 * software for any purpose without first obtaining a commercial license from
 * Qorvo.
 * Please contact Qorvo to inquire about licensing terms.
 */
#define MAX_CHIP_VERSIONS 2

int __dw3000_chip_version;

unsigned int __tx_fctrl_id[MAX_CHIP_VERSIONS] = { 0x24, 0x20 };
unsigned int __tx_fctrl_hi_id[MAX_CHIP_VERSIONS] = { 0x28, 0x24 };
unsigned int __dx_time_id[MAX_CHIP_VERSIONS] = { 0x2c, 0x28 };
unsigned int __rx_ttcko_lo_id[MAX_CHIP_VERSIONS] = { 0x5c, 0x58 };
unsigned int __rx_ttcko_hi_id[MAX_CHIP_VERSIONS] = { 0x60, 0x5c };
unsigned int __rx_time_0_id[MAX_CHIP_VERSIONS] = { 0x64, 0x60 };
unsigned int __rx_time_1_id[MAX_CHIP_VERSIONS] = { 0x68, 0x64 };
unsigned int __rx_time_2_id[MAX_CHIP_VERSIONS] = { 0x6c, 0x68 };
unsigned int __rx_time_3_id[MAX_CHIP_VERSIONS] = { 0x70, 0x6c };
unsigned int __tx_time_lo_id[MAX_CHIP_VERSIONS] = { 0x74, 0x70 };
unsigned int __tx_time_hi_id[MAX_CHIP_VERSIONS] = { 0x78, 0x74 };
unsigned int __tx_time_2_id[MAX_CHIP_VERSIONS] = { 0x10000, 0x78 };
unsigned int __tx_antd_id[MAX_CHIP_VERSIONS] = { 0x10004, 0x7c };
unsigned int __ack_resp_id[MAX_CHIP_VERSIONS] = { 0x10008, 0x10000 };
unsigned int __tx_power_id[MAX_CHIP_VERSIONS] = { 0x1000c, 0x10004 };
unsigned int __chan_ctrl_id[MAX_CHIP_VERSIONS] = { 0x10014, 0x10008 };
unsigned int __le_pend_01_id[MAX_CHIP_VERSIONS] = { 0x10018, 0x1000C };
unsigned int __le_pend_23_id[MAX_CHIP_VERSIONS] = { 0x1001c, 0x10010 };
unsigned int __spi_collision_status_id[MAX_CHIP_VERSIONS] = { 0x10020,
							      0x10014 };
unsigned int __rdb_status_id[MAX_CHIP_VERSIONS] = { 0x10024, 0x10018 };
unsigned int __rdb_diag_mode_id[MAX_CHIP_VERSIONS] = { 0x10028, 0x10020 };
unsigned int __regmap_ver_id[MAX_CHIP_VERSIONS] = { 0x1002c, 0x10024 };
unsigned int __sar_ctrl_sar_force_sel_bit_len[MAX_CHIP_VERSIONS] = { 3U, 4U };
unsigned int __sar_ctrl_sar_force_sel_bit_mask[MAX_CHIP_VERSIONS] = { 0x7000U,
								      0xf000U };
unsigned int __nvm_cfg_gear_id_bit_offset[MAX_CHIP_VERSIONS] = { 11U, 12U };
unsigned int __nvm_cfg_gear_id_bit_mask[MAX_CHIP_VERSIONS] = { 0x1800U,
							       0x3000U };
unsigned int __nvm_cfg_gear_kick_bit_offset[MAX_CHIP_VERSIONS] = { 10U, 0x11U };
unsigned int __nvm_cfg_gear_kick_bit_mask[MAX_CHIP_VERSIONS] = { 0x400U,
								 0x800U };
unsigned int __nvm_cfg_bias_kick_bit_offset[MAX_CHIP_VERSIONS] = { 8U, 10U };
unsigned int __nvm_cfg_bias_kick_bit_mask[MAX_CHIP_VERSIONS] = { 0x100U,
								 0x400U };
unsigned int __nvm_cfg_ldo_kick_bit_offset[MAX_CHIP_VERSIONS] = { 7U, 9U };
unsigned int __nvm_cfg_ldo_kick_bit_mask[MAX_CHIP_VERSIONS] = { 0x80U, 0x200U };
unsigned int __nvm_cfg_dgc_kick_bit_offset[MAX_CHIP_VERSIONS] = { 6U, 8U };
unsigned int __nvm_cfg_dgc_kick_bit_mask[MAX_CHIP_VERSIONS] = { 0x40U, 0x100U };
unsigned int __nvm_cfg_dgc_sel_bit_offset[MAX_CHIP_VERSIONS] = { 13U, 7U };
unsigned int __nvm_cfg_dgc_sel_bit_mask[MAX_CHIP_VERSIONS] = { 0x2000U, 0x80U };
unsigned int __nvm_cfg_nvm_pd_bit_offset[MAX_CHIP_VERSIONS] = { 9U, 6U };
unsigned int __nvm_cfg_nvm_pd_bit_mask[MAX_CHIP_VERSIONS] = { 0x200U, 0x40U };
unsigned int __ip_diag_1_ipchannelarea_bit_len[MAX_CHIP_VERSIONS] = { 17U,
								      19U };
unsigned int __ip_diag_1_ipchannelarea_bit_mask[MAX_CHIP_VERSIONS] = {
	0x1ffffUL, 0x7ffffUL
};
unsigned int __cy0_diag_1_cy0channelarea_bit_len[MAX_CHIP_VERSIONS] = { 16U,
									19U };
unsigned int __cy0_diag_1_cy0channelarea_bit_mask[MAX_CHIP_VERSIONS] = {
	0xffffU, 0x7ffffU
};
unsigned int __cy1_diag_1_cy1channelarea_bit_len[MAX_CHIP_VERSIONS] = { 16U,
									19U };
unsigned int __cy1_diag_1_cy1channelarea_bit_mask[MAX_CHIP_VERSIONS] = {
	0xffffU, 0x7ffffU
};
unsigned int __ip_config_hi_id[MAX_CHIP_VERSIONS] = { 0xe000e, 0xe0010 };
unsigned int __cy_config_lo_id[MAX_CHIP_VERSIONS] = { 0xe0012, 0xe0014 };
unsigned int __cy_config_hi_id[MAX_CHIP_VERSIONS] = { 0xe0016, 0xe0018 };
unsigned int __cia_coefficient_adjust_id[MAX_CHIP_VERSIONS] = { 0xe001a,
								0xe001c };
unsigned int __pgf_delay_comp_lo_id[MAX_CHIP_VERSIONS] = { 0xe001e, 0xe0020 };
unsigned int __pgf_delay_comp_hi_id[MAX_CHIP_VERSIONS] = { 0xe0022, 0xe0024 };
unsigned int __adc_mem_ptr_id[MAX_CHIP_VERSIONS] = { 0xf0020, 0xf0024 };
unsigned int __test_ctrl0_id[MAX_CHIP_VERSIONS] = { 0xf0024, 0xf0028 };
unsigned int __fcmd_status_id[MAX_CHIP_VERSIONS] = { 0xf003c, 0xf0040 };
unsigned int __test_logging_id[MAX_CHIP_VERSIONS] = { 0xf0040, 0xf0044 };
unsigned int __status_logging_id[MAX_CHIP_VERSIONS] = { 0xf0044, 0xf0048 };
unsigned int __ctr_dbg_id[MAX_CHIP_VERSIONS] = { 0xf0048, 0xf004C };
unsigned int __led_ctrl_id[MAX_CHIP_VERSIONS] = { 0x110016, 0x110018 };
unsigned int __rx_ppm_id[MAX_CHIP_VERSIONS] = { 0x11001a, 0x11001c };
unsigned int __fosc_ctrl_id[MAX_CHIP_VERSIONS] = { 0x11001e, 0x110020 };
unsigned int __bias_ctrl_id[MAX_CHIP_VERSIONS] = { 0x11001f, 0x110030 };
unsigned int __bias_ctrl_dig_bias_ctrl_tc_r3_ulv_bit_offset[MAX_CHIP_VERSIONS] = {
	12U, 9U
};
unsigned int __bias_ctrl_dig_bias_ctrl_tc_r3_ulv_bit_mask[MAX_CHIP_VERSIONS] = {
	0x3000U, 0x600U
};
/* LDO and BIAS tune kick */
/* C0 : Writing to bit 7 and 8 */
/* D0 NOTE: only kick LDO bit 9 (Writing to bit 10 for BIAS and 9 for LDO) */
unsigned int __ldo_bias_kick[MAX_CHIP_VERSIONS] = { 0x180, 0x200 };

#define DW3000_TX_FCTRL_ID __tx_fctrl_id[__dw3000_chip_version]
#define DW3000_TX_FCTRL_HI_ID __tx_fctrl_hi_id[__dw3000_chip_version]
#define DW3000_DX_TIME_ID __dx_time_id[__dw3000_chip_version]
#define DW3000_RX_TTCKO_LO_ID __rx_ttcko_lo_id[__dw3000_chip_version]
#define DW3000_RX_TTCKO_HI_ID __rx_ttcko_hi_id[__dw3000_chip_version]
#define DW3000_RX_TIME_0_ID __rx_time_0_id[__dw3000_chip_version]
#define DW3000_RX_TIME_1_ID __rx_time_1_id[__dw3000_chip_version]
#define DW3000_RX_TIME_2_ID __rx_time_2_id[__dw3000_chip_version]
#define DW3000_RX_TIME_3_ID __rx_time_3_id[__dw3000_chip_version]
#define DW3000_TX_TIME_LO_ID __tx_time_lo_id[__dw3000_chip_version]
#define DW3000_TX_TIME_HI_ID __tx_time_hi_id[__dw3000_chip_version]
#define DW3000_TX_TIME_2_ID __tx_time_2_id[__dw3000_chip_version]
#define DW3000_TX_ANTD_ID __tx_antd_id[__dw3000_chip_version]
#define DW3000_ACK_RESP_ID __ack_resp_id[__dw3000_chip_version]
#define DW3000_TX_POWER_ID __tx_power_id[__dw3000_chip_version]
#define DW3000_CHAN_CTRL_ID __chan_ctrl_id[__dw3000_chip_version]
#define DW3000_LE_PEND_01_ID __le_pend_01_id[__dw3000_chip_version]
#define DW3000_LE_PEND_23_ID __le_pend_23_id[__dw3000_chip_version]
#define DW3000_SPI_COLLISION_STATUS_ID \
	__spi_collision_status_id[__dw3000_chip_version]
#define DW3000_RDB_STATUS_ID __rdb_status_id[__dw3000_chip_version]
#define DW3000_RDB_DIAG_MODE_ID __rdb_diag_mode_id[__dw3000_chip_version]
#define DW3000_REGMAP_VER_ID __regmap_ver_id[__dw3000_chip_version]
#define DW3000_SAR_CTRL_SAR_FORCE_SEL_BIT_LEN \
	__sar_ctrl_sar_force_sel_bit_len[__dw3000_chip_version]
#define DW3000_SAR_CTRL_SAR_FORCE_SEL_BIT_MASK \
	__sar_ctrl_sar_force_sel_bit_mask[__dw3000_chip_version]
#define DW3000_NVM_CFG_GEAR_ID_BIT_OFFSET \
	__nvm_cfg_gear_id_bit_offset[__dw3000_chip_version]
#define DW3000_NVM_CFG_GEAR_ID_BIT_MASK \
	__nvm_cfg_gear_id_bit_mask[__dw3000_chip_version]
#define DW3000_NVM_CFG_GEAR_KICK_BIT_OFFSET \
	__nvm_cfg_gear_kick_bit_offset[__dw3000_chip_version]
#define DW3000_NVM_CFG_GEAR_KICK_BIT_MASK \
	__nvm_cfg_gear_kick_bit_mask[__dw3000_chip_version]
#define DW3000_NVM_CFG_BIAS_KICK_BIT_OFFSET \
	__nvm_cfg_bias_kick_bit_offset[__dw3000_chip_version]
#define DW3000_NVM_CFG_BIAS_KICK_BIT_MASK \
	__nvm_cfg_bias_kick_bit_mask[__dw3000_chip_version]
#define DW3000_NVM_CFG_LDO_KICK_BIT_OFFSET \
	__nvm_cfg_ldo_kick_bit_offset[__dw3000_chip_version]
#define DW3000_NVM_CFG_LDO_KICK_BIT_MASK \
	__nvm_cfg_ldo_kick_bit_mask[__dw3000_chip_version]
#define DW3000_NVM_CFG_DGC_KICK_BIT_OFFSET \
	__nvm_cfg_dgc_kick_bit_offset[__dw3000_chip_version]
#define DW3000_NVM_CFG_DGC_KICK_BIT_MASK \
	__nvm_cfg_dgc_kick_bit_mask[__dw3000_chip_version]
#define DW3000_NVM_CFG_DGC_SEL_BIT_OFFSET \
	__nvm_cfg_dgc_sel_bit_offset[__dw3000_chip_version]
#define DW3000_NVM_CFG_DGC_SEL_BIT_MASK \
	__nvm_cfg_dgc_sel_bit_mask[__dw3000_chip_version]
#define DW3000_NVM_CFG_NVM_PD_BIT_OFFSET \
	__nvm_cfg_nvm_pd_bit_offset[__dw3000_chip_version]
#define DW3000_NVM_CFG_NVM_PD_BIT_MASK \
	__nvm_cfg_nvm_pd_bit_mask[__dw3000_chip_version]
#define DW3000_IP_DIAG_1_IPCHANNELAREA_BIT_LEN \
	__ip_diag_1_ipchannelarea_bit_len[__dw3000_chip_version]
#define DW3000_IP_DIAG_1_IPCHANNELAREA_BIT_MASK \
	__ip_diag_1_ipchannelarea_bit_len[__dw3000_chip_version]
#define DW3000_CY0_DIAG_1_CY0CHANNELAREA_BIT_LEN \
	__cy0_diag_1_cy0channelarea_bit_len[__dw3000_chip_version]
#define DW3000_CY0_DIAG_1_CY0CHANNELAREA_BIT_MASK \
	__cy0_diag_1_cy0channelarea_bit_mask[__dw3000_chip_version]
#define DW3000_CY1_DIAG_1_CY1CHANNELAREA_BIT_LEN \
	__cy1_diag_1_cy1channelarea_bit_len[__dw3000_chip_version]
#define DW3000_CY1_DIAG_1_CY1CHANNELAREA_BIT_MASK \
	__cy1_diag_1_cy1channelarea_bit_mask[__dw3000_chip_version]
#define DW3000_IP_CONFIG_HI_ID __ip_config_hi_id[__dw3000_chip_version]
#define DW3000_CY_CONFIG_LO_ID __cy_config_lo_id[__dw3000_chip_version]
#define DW3000_CY_CONFIG_HI_ID __cy_config_hi_id[__dw3000_chip_version]
#define DW3000_CIA_COEFFICIENT_ADJUST_ID \
	__cia_coefficient_adjust_id[__dw3000_chip_version]
#define DW3000_PGF_DELAY_COMP_LO_ID \
	__pgf_delay_comp_lo_id[__dw3000_chip_version]
#define DW3000_PGF_DELAY_COMP_HI_ID \
	__pgf_delay_comp_hi_id[__dw3000_chip_version]
#define DW3000_ADC_MEM_PTR_ID __adc_mem_ptr_id[__dw3000_chip_version]
#define DW3000_TEST_CTRL0_ID __test_ctrl0_id[__dw3000_chip_version]
#define DW3000_FCMD_STATUS_ID __fcmd_status_id[__dw3000_chip_version]
#define DW3000_TEST_LOGGING_ID __test_logging_id[__dw3000_chip_version]
#define DW3000_STATUS_LOGGING_ID __status_logging_id[__dw3000_chip_version]
#define DW3000_CTR_DBG_ID __ctr_dbg_id[__dw3000_chip_version]
#define DW3000_LED_CTRL_ID __led_ctrl_id[__dw3000_chip_version]
#define DW3000_RX_PPM_ID __rx_ppm_id[__dw3000_chip_version]
#define DW3000_FOSC_CTRL_ID __fosc_ctrl_id[__dw3000_chip_version]
#define DW3000_BIAS_CTRL_ID __bias_ctrl_id[__dw3000_chip_version]
#define DW3000_BIAS_CTRL_DIG_BIAS_CTRL_TC_R3_ULV_BIT_OFFSET \
	__bias_ctrl_dig_bias_ctrl_tc_r3_ulv_bit_offset[__dw3000_chip_version]
#define DW3000_BIAS_CTRL_DIG_BIAS_CTRL_TC_R3_ULV_BIT_MASK \
	__bias_ctrl_dig_bias_ctrl_tc_r3_ulv_bit_mask[__dw3000_chip_version]
#define DW3000_LDO_BIAS_KICK __ldo_bias_kick[__dw3000_chip_version]
