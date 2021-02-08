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

#ifndef DW3000_TESTMODE_NL_H
#define DW3000_TESTMODE_NL_H

#include <linux/types.h>

/**
 * struct dw3000_rssi - Required data for RSSI calculation in userland.
 * @cir_pwr: value of the channel impulse response power (CIR power)
 * @pacc_cnt: value of the the preamble accumulation count (PACC count)
 * @prf_64mhz: value of preamble repetition frequency (0 = 16MHz, 1 = 64MHz)
 */
struct dw3000_rssi {
	uint32_t cir_pwr : 17;
	uint16_t pacc_cnt : 11;
	uint8_t prf_64mhz : 1;
} __attribute__((__packed__));

/**
 * Since both DW3720 & DW3120 user manuals specify only 11-bits at most for
 * diagnostic counters, we do the same for RSSI report number.
 */
#define DW3000_RSSI_REPORTS_MAX (1 << 11)
#define DW3000_TM_RSSI_DATA_MAX_LEN \
	(DW3000_RSSI_REPORTS_MAX * sizeof(struct dw3000_rssi))

/* OTP address limit */
#define DW3000_OTP_ADDRESS_LIMIT 0x7f

/* All dw3000 testmode interface attributes */
enum dw3000_tm_attr {
	__DW3000_TM_ATTR_INVALID = 0,
	DW3000_TM_ATTR_CMD = 1,
	DW3000_TM_ATTR_RX_GOOD_CNT = 2,
	DW3000_TM_ATTR_RX_BAD_CNT = 3,
	DW3000_TM_ATTR_RSSI_DATA = 4,
	DW3000_TM_ATTR_OTP_ADDR = 5,
	DW3000_TM_ATTR_OTP_VAL = 6,
	DW3000_TM_ATTR_OTP_DONE = 7,
	DW3000_TM_ATTR_CCC_TIME0 = 8,
	DW3000_TM_ATTR_CCC_CHANNEL = 9,
	DW3000_TM_ATTR_CCC_TSTART = 10,
	DW3000_TM_ATTR_CCC_TEND = 11,
	DW3000_TM_ATTR_CCC_CMD_RC = 12,

	/* keep last */
	__DW3000_TM_ATTR_AFTER_LAST,
	DW3000_TM_ATTR_MAX = __DW3000_TM_ATTR_AFTER_LAST - 1,
};

/* All dw3000 testmode interface commands specified in DW3000_TM_ATTR_CMD */
enum dw3000_tm_cmd {
	__DW3000_TM_CMD_INVALID = 0,
	DW3000_TM_CMD_START_RX_DIAG = 1,
	DW3000_TM_CMD_STOP_RX_DIAG = 2,
	DW3000_TM_CMD_GET_RX_DIAG = 3,
	DW3000_TM_CMD_CLEAR_RX_DIAG = 4,
	DW3000_TM_CMD_START_TX_CWTONE = 5,
	DW3000_TM_CMD_STOP_TX_CWTONE = 6,
	DW3000_TM_CMD_OTP_READ = 7,
	DW3000_TM_CMD_OTP_WRITE = 8,

	/* CCC NFCC/AP coexistence test commands */
	DW3000_TM_CMD_CCC_START = 9,
	DW3000_TM_CMD_CCC_TEST_SCRATCH = 10,
	DW3000_TM_CMD_CCC_TEST_SPI1 = 11,
	DW3000_TM_CMD_CCC_TEST_SPI2 = 12,
	DW3000_TM_CMD_CCC_READ_TLVS = 13,
	DW3000_TM_CMD_CCC_WRITE_TLVS = 14,

	/* keep last */
	__DW3000_TM_CMD_AFTER_LAST,
	DW3000_TM_CMD_MAX = __DW3000_TM_CMD_AFTER_LAST - 1,
};

#endif /* DW3000_TESTMODE_NL_H */
