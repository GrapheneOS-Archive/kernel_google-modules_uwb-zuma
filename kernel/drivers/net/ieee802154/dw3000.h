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
#ifndef __DW3000_H
#define __DW3000_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/skbuff.h>
#include <net/mcps802154.h>
#include "dw3000_stm.h"
#undef BIT_MASK

#ifndef DEBUG
#define DEBUG 0
#endif

#define LOG(format, arg...)                                             \
	do {                                                            \
		pr_info("dw3000: %s(): " format "\n", __func__, ##arg); \
	} while (0)

/* Defined constants when SPI CRC mode is used */
enum dw3000_spi_crc_mode {
	DW3000_SPI_CRC_MODE_NO = 0, /* No CRC */
	DW3000_SPI_CRC_MODE_WR, /* This is used to enable SPI CRC check (the SPI
				   CRC check will be enabled on DW3000 and CRC-8
				   added for SPI write transactions) */
	DW3000_SPI_CRC_MODE_WRRD /* This is used to optionally enable additional
				    CRC check on the SPI read operations, while
				    the CRC check on the SPI write operations is
				    also enabled */
};

/* RX ISR data */
struct dw3000_isr_data {
	u32 status; /* initial value of register as ISR is entered */
	u16 datalength; /* length of frame */
	u8 rx_flags; /* RX frame flags, see above */
};

/* Time units and conversion factor */
#define DW3000_CHIP_FREQ 499200000
#define DW3000_CHIP_PER_DTU 2
#define DW3000_CHIP_PER_DLY 512
#define DW3000_DTU_FREQ (DW3000_CHIP_FREQ / DW3000_CHIP_PER_DTU)
#define DW3000_RCTU_PER_CHIP 128
#define DW3000_RCTU_PER_DTU (DW3000_RCTU_PER_CHIP * DW3000_CHIP_PER_DTU)
/* 6.9.1.5 in 4z, for HRP UWB PHY:
   416 chips = 416 / (499.2 * 10^6) ~= 833.33 ns */
#define DW3000_DTU_PER_RSTU (416 / DW3000_CHIP_PER_DTU)

#define DW3000_RX_ENABLE_STARTUP_DLY 16
#define DW3000_RX_ENABLE_STARTUP_RCTU                         \
	(DW3000_RX_ENABLE_STARTUP_DLY * DW3000_CHIP_PER_DLY * \
	 DW3000_RCTU_PER_CHIP)
#define DW3000_RX_ENABLE_STARTUP_DTU                          \
	(DW3000_RX_ENABLE_STARTUP_DLY * DW3000_CHIP_PER_DLY / \
	 DW3000_CHIP_PER_DTU)

/* Enum used for selecting location to load DGC data from. */
enum d3000_dgc_load_location {
	DW3000_DGC_LOAD_FROM_SW = 0,
	DW3000_DGC_LOAD_FROM_OTP
};

/* DW3000 data */
struct dw3000_local_data {
	u32 partID;
	u32 lotID;
	enum dw3000_spi_crc_mode spicrc;
	/* Flag to check if DC values are programmed in OTP.
	   See d3000_dgc_load_location. */
	u8 dgc_otp_set;
	u8 vBatP;
	u8 tempP;
	u8 otprev;
	u8 dblbuffon;
	u16 max_frames_len;
	u16 sleep_mode;
	s16 ststhreshold;
	u8 stsconfig;
	u8 cia_diagnostic;
	/* Transmit frame control */
	u32 tx_fctrl;
	/* Preamble detection timeout period in units of PAC size symbols */
	u16 rx_timeout_pac;
	/* Wait-for-response time (RX after TX delay) */
	u32 w4r_time;
	/* Auto ack turnaroud time */
	u8 ack_time;
};

/* Maximum skb length
 *
 * Maximum supported frame size minus the checksum.
 */
#define DW3000_MAX_SKB_LEN (IEEE802154_MAX_SIFS_FRAME_SIZE - IEEE802154_FCS_LEN)

/* Additional informations on rx. */
enum dw3000_rx_flags {
	/* Set if an automatix ack is send. */
	DW3000_RX_FLAG_AACK = BIT(0),
};

/* Receive descriptor */
struct dw3000_rx {
	/* Receive lock */
	spinlock_t lock;
	/* Socket buffer */
	struct sk_buff *skb;
	/* Additional information on rx. See dw3000_rx_flags. */
	u8 flags;
};

/* DW3000 STS length field of the CP_CFG register (unit of 8 symbols bloc) */
enum dw3000_sts_lengths {
	DW3000_STS_LEN_8 = 0,
	DW3000_STS_LEN_16 = 1,
	DW3000_STS_LEN_32 = 2,
	DW3000_STS_LEN_64 = 3,
	DW3000_STS_LEN_128 = 4,
	DW3000_STS_LEN_256 = 5,
	DW3000_STS_LEN_512 = 6,
	DW3000_STS_LEN_1024 = 7,
	DW3000_STS_LEN_2048 = 8
};

/* Structure for setting device configuration via dw3000_configure() function */
struct dw3000_config {
	/* Channel number (5 or 9) */
	u8 chan;
	/* DW3000_PLEN_64..DW3000_PLEN_4096 */
	u8 txPreambLength;
	/* TX preamble code (the code configures the PRF, e.g. 9 -> PRF of 64 MHz) */
	u8 txCode;
	/* RX preamble code (the code configures the PRF, e.g. 9 -> PRF of 64 MHz) */
	u8 rxCode;
	/* SFD type (0 for short IEEE 8b standard, 1 for DW 8b, 2 for DW 16b, 2 for 4z BPRF) */
	u8 sfdType;
	/* Data rate {DW3000_BR_850K or DW3000_BR_6M8} */
	u8 dataRate;
	/* PHR mode {0x0 - standard DW3000_PHRMODE_STD, 0x3 - extended frames DW3000_PHRMODE_EXT} */
	u8 phrMode;
	/* PHR rate {0x0 - standard DW3000_PHRRATE_STD, 0x1 - at datarate DW3000_PHRRATE_DTA} */
	u8 phrRate;
	/* SFD timeout value (in symbols) */
	u16 sfdTO;
	/* STS mode (no STS, STS before PHR or STS after data) */
	u8 stsMode;
	/* STS length (see enum) */
	enum dw3000_sts_lengths stsLength;
	/* PDOA mode */
	u8 pdoaMode;
};

/* TX configuration,  power & PG delay */
struct dw3000_txconfig {
	u8 PGdly;
	/*TX POWER
	31:24     TX_CP_PWR
	23:16     TX_SHR_PWR
	15:8      TX_PHR_PWR
	7:0       TX_DATA_PWR */
	u32 power;
};

/* DW3000 device */
struct dw3000 {
	/* SPI device */
	struct spi_device *spi;
	/* Generic device */
	struct device *dev;
	/* MCPS 802.15.4 device */
	struct mcps802154_llhw *llhw;
	/* Configuration */
	struct dw3000_config config;
	struct dw3000_txconfig txconfig;
	/* Data */
	struct dw3000_local_data data;
	/* SPI controller power-management */
	int has_lock_pm;
	/* Control GPIOs */
	int reset_gpio;
	/* State machine */
	struct dw3000_state stm;
	/* Receive descriptor */
	struct dw3000_rx rx;
	/* Shared message protected by a mutex */
	struct mutex msg_mutex;
	struct spi_message *msg_readwrite_fdx;
	/* Precomputed spi_messages */
	struct spi_message *msg_fast_command;
	struct spi_message *msg_read_rdb_status;
	struct spi_message *msg_read_rx_timestamp;
	struct spi_message *msg_read_rx_timestamp_a;
	struct spi_message *msg_read_rx_timestamp_b;
	struct spi_message *msg_read_sys_status;
	struct spi_message *msg_read_sys_time;
	struct spi_message *msg_write_sys_status;
	/* Chips per PAC unit. */
	int chips_per_pac;
	/* Preamble timeout in PAC unit. */
	int pre_timeout_pac;
	/* Is auto-ack activated? */
	bool autoack;
};

#endif /* __DW3000_H */
