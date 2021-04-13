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
#include <linux/regulator/consumer.h>
#include "dw3000_chip.h"
#include "dw3000_stm.h"
#include "dw3000_calib.h"
#include "dw3000_testmode_nl.h"

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

/* ISR data */
struct dw3000_isr_data {
	u32 status; /* initial value of register as ISR is entered */
	u16 status_hi; /* initial value of register as ISR is entered, 2 hi bytes */
	u16 datalength; /* length of frame */
	u64 ts_rctu; /* frame timestamp in RCTU unit */
	u8 dss_stat; /* value of the dual-SPI semaphore events */
	u8 rx_flags; /* RX frame flags, see dw3000_rx_flags */
};

/* Time units and conversion factor */
#define DW3000_CHIP_FREQ 499200000
#define DW3000_CHIP_PER_DTU 2
#define DW3000_CHIP_PER_DLY 512
#define DW3000_DTU_FREQ (DW3000_CHIP_FREQ / DW3000_CHIP_PER_DTU)
#define DW3000_RCTU_PER_CHIP 128
#define DW3000_RCTU_PER_DTU (DW3000_RCTU_PER_CHIP * DW3000_CHIP_PER_DTU)
#define DW3000_RCTU_PER_DLY (DW3000_CHIP_PER_DLY / DW3000_RCTU_PER_CHIP)
#define DW3000_NSEC_PER_DTU (1000000000 / DW3000_DTU_FREQ)
/* 6.9.1.5 in 4z, for HRP UWB PHY:
   416 chips = 416 / (499.2 * 10^6) ~= 833.33 ns */
#define DW3000_DTU_PER_RSTU (416 / DW3000_CHIP_PER_DTU)
#define DW3000_DTU_PER_DLY (DW3000_CHIP_PER_DLY / DW3000_CHIP_PER_DTU)

#define DW3000_RX_ENABLE_STARTUP_DLY 16
#define DW3000_RX_ENABLE_STARTUP_DTU                          \
	(DW3000_RX_ENABLE_STARTUP_DLY * DW3000_CHIP_PER_DLY / \
	 DW3000_CHIP_PER_DTU)

/* Enum used for selecting location to load DGC data from. */
enum d3000_dgc_load_location {
	DW3000_DGC_LOAD_FROM_SW = 0,
	DW3000_DGC_LOAD_FROM_OTP
};

/* DW3000 OTP */
struct dw3000_otp_data {
	u32 partID;
	u32 lotID;
	u32 ldo_tune_lo;
	u32 ldo_tune_hi;
	u32 bias_tune;
	u32 dgc_addr;
	u8 xtal_trim;
	u8 vBatP;
	u8 tempP;
	u8 rev;
};

/**
 * enum dw3000_ciagdiag_reg_select - CIA diagnostic register selector config.
 * According to DW3000's configuration, we must read some values
 * (e.g: channel impulse response power, preamble accumulation count)
 * in different registers in the CIA interface:
 * 0: without STS
 * 1: with STS
 * 2: PDOA mode 3
 */
enum dw3000_ciagdiag_reg_select {
	DW3000_CIA_DIAG_REG_SELECT_WITHOUT_STS = 0,
	DW3000_CIA_DIAG_REG_SELECT_WITH_STS = 1,
	DW3000_CIA_DIAG_REG_SELECT_WITH_PDAO_M3 = 3,
};

/* DW3000 data */
/* TODO: Delete */
struct dw3000_local_data {
	enum dw3000_spi_crc_mode spicrc;
	/* Flag to check if DC values are programmed in OTP.
	   See d3000_dgc_load_location. */
	u8 dgc_otp_set;
	u8 otprev;
	u8 dblbuffon;
	u16 max_frames_len;
	u16 sleep_mode;
	s16 ststhreshold;
	u8 stsconfig;
	/* CIA diagnostic on/off */
	bool ciadiag_enabled;
	/* CIA diagnostic double buffering option */
	u8 ciadiag_opt;
	/* CIA diagnostic register selector according to DW3000's config */
	enum dw3000_ciagdiag_reg_select ciadiag_reg_select;
	/* Transmit frame control */
	u32 tx_fctrl;
	/* Preamble detection timeout period in units of PAC size symbols */
	u16 rx_timeout_pac;
	/* Wait-for-response time (RX after TX delay) */
	u32 w4r_time;
	/* Auto ack turnaroud time */
	u8 ack_time;
};

/* Statistics items */
enum dw3000_stats_items {
	DW3000_STATS_RX_GOOD,
	DW3000_STATS_RX_TO,
	DW3000_STATS_RX_ERROR,
	__DW3000_STATS_COUNT
};

/* DW3000 statistics */
struct dw3000_stats {
	/* Total stats */
	u16 count[__DW3000_STATS_COUNT];
	/* Required data array for calculation of the RSSI average */
	struct dw3000_rssi rssi[DW3000_RSSI_REPORTS_MAX];
	/* Stats on/off */
	bool enabled;
};

/* Maximum skb length
 *
 * Maximum supported frame size minus the checksum.
 */
#define DW3000_MAX_SKB_LEN (IEEE802154_MAX_SIFS_FRAME_SIZE - IEEE802154_FCS_LEN)

/* Additional information on rx. */
enum dw3000_rx_flags {
	/* Set if an automatic ack is send. */
	DW3000_RX_FLAG_AACK = BIT(0),
	/* Set if no data. */
	DW3000_RX_FLAG_ND = BIT(1),
	/* Set if timestamp known. */
	DW3000_RX_FLAG_TS = BIT(2),
	/* Ranging bit */
	DW3000_RX_FLAG_RNG = BIT(3),
	/* CIA done */
	DW3000_RX_FLAG_CIA = BIT(4),
	/* CIA error */
	DW3000_RX_FLAG_CER = BIT(5),
	/* STS error */
	DW3000_RX_FLAG_CPER = BIT(6)
};

/* Receive descriptor */
struct dw3000_rx {
	/* Receive lock */
	spinlock_t lock;
	/* Socket buffer */
	struct sk_buff *skb;
	/* Frame timestamp */
	u64 ts_rctu;
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

/**
 * dw3000_config - Structure holding current device configuration
 */
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
	/* PDOA mode */
	u8 pdoaMode;
	/* Antenna currently connected to RF1 & RF2 ports respectively. */
	s8 ant[2];
	/* Calibrated PDOA offset */
	s16 pdoaOffset;
	/* Calibrated rmarker offset */
	u32 rmarkerOffset;
	/* STS length (see enum) */
	enum dw3000_sts_lengths stsLength;
};

/* TX configuration,  power & PG delay */
struct dw3000_txconfig {
	u8 PGdly;
	u8 PGcount;
	/*TX POWER
	31:24     TX_CP_PWR
	23:16     TX_SHR_PWR
	15:8      TX_PHR_PWR
	7:0       TX_DATA_PWR */
	u32 power;
	/* Normal or test mode */
	bool testmode_enabled;
};

struct sysfs_power_stats {
	u64 dur;
	u64 count;
};

enum power_state {
	DW3000_PWR_OFF = 0,
	DW3000_PWR_RUN,
	DW3000_PWR_IDLE,
	DW3000_PWR_RX,
	DW3000_PWR_TX,
	DW3000_PWR_MAX,
};

enum operational_state {
	DW3000_OP_STATE_OFF = 0,
	DW3000_OP_STATE_WAKE_UP,
	DW3000_OP_STATE_INIT_RC,
	DW3000_OP_STATE_SLEEP,
	DW3000_OP_STATE_DEEP_SLEEP,
	DW3000_OP_STATE_IDLE_RC,
	DW3000_OP_STATE_IDLE_PLL,
	DW3000_OP_STATE_TX_WAIT,
	DW3000_OP_STATE_TX,
	DW3000_OP_STATE_RX_WAIT,
	DW3000_OP_STATE_RX,
	DW3000_OP_STATE_MAX,
};

/**
 * struct dw3000_power - DW3000 device power related data
 * @stats: calculated stats
 * @start_time: timestamp of current state start
 * @cur_state: current state
 * @tx_adjust: TX time adjustment based on frame length
 * @rx_start: RX start date in DTU for RX time adjustment
 */
struct dw3000_power {
	struct sysfs_power_stats stats[DW3000_PWR_MAX];
	u64 start_time;
	int cur_state;
	int tx_adjust;
	u32 rx_start;
};

/**
 * struct dw3000 - main DW3000 device structure
 * @spi: pointer to corresponding spi device
 * @dev: pointer to generic device holding sysfs attributes
 * @sysfs_power_dir: kobject holding sysfs power directory
 * @chip_ops: version specific chip operations
 * @llhw: pointer to associated struct mcps802154_llhw
 * @config: current running chip configuration
 * @txconfig: current running TX configuration
 * @data: local data and register cache
 * @otp_data: OTP data cache
 * @calib_data: calibration data
 * @stats: statistics
 * @power: power related statistics and states
 * @chip_dev_id: identified chip device ID
 * @has_lock_pm: power management locked status
 * @reset_gpio: GPIO to use for hard reset
 * @regulator: Power supply
 * @chips_per_pac: chips per PAC unit
 * @pre_timeout_pac: preamble timeout in PAC unit
 * @coex_delay_us: WiFi coexistence GPIO delay in us
 * @coex_gpio: WiFi coexistence GPIO, >= 0 if activated
 * @lna_pa_mode: LNA/PA configuration to use
 * @autoack: auto-ack status, true if activated
 * @nfcc_mode: NFCC mode enabled, true if activated
 * @pgf_cal_running: true if pgf calibration is running
 * @stm: High-priority thread state machine
 * @rx: received skbuff and associated spinlock
 * @msg_mutex: mutex protecting @msg_readwrite_fdx
 * @msg_readwrite_fdx: pre-computed generic register read/write SPI message
 * @msg_fast_command: pre-computed fast command SPI message
 * @msg_read_cir_pwr: pre-computed SPI message
 * @msg_read_pacc_cnt: pre-computed SPI message
 * @msg_read_rdb_status: pre-computed SPI message
 * @msg_read_rx_timestamp: pre-computed SPI message
 * @msg_read_rx_timestamp_a: pre-computed SPI message
 * @msg_read_rx_timestamp_b: pre-computed SPI message
 * @msg_read_sys_status: pre-computed SPI message
 * @msg_read_sys_status_hi: pre-computed SPI message
 * @msg_read_sys_time: pre-computed SPI message
 * @msg_write_sys_status: pre-computed SPI message
 * @msg_read_dss_status: pre-computed SPI message
 * @msg_write_dss_status: pre-computed SPI message
 * @msg_write_spi_collision_status: pre-computed SPI message
 * @current_operational_state: internal operational state of the chip
 */
struct dw3000 {
	/* SPI device */
	struct spi_device *spi;
	/* Generic device */
	struct device *dev;
	/* Kernel object holding sysfs power sub-directory */
	struct kobject sysfs_power_dir;
	/* Chip version specific operations */
	const struct dw3000_chip_ops *chip_ops;
	/* MCPS 802.15.4 device */
	struct mcps802154_llhw *llhw;
	/* Configuration */
	struct dw3000_config config;
	struct dw3000_txconfig txconfig;
	/* Data */
	struct dw3000_local_data data;
	struct dw3000_otp_data otp_data;
	struct dw3000_calibration_data calib_data;
	/* Statistics */
	struct dw3000_stats stats;
	struct dw3000_power power;
	/* Detected chip device ID */
	u32 chip_dev_id;
	/* SPI controller power-management */
	int has_lock_pm;
	/* Control GPIOs */
	int reset_gpio;
	/* Power supply */
	struct regulator *regulator;
	/* Chips per PAC unit. */
	int chips_per_pac;
	/* Preamble timeout in PAC unit. */
	int pre_timeout_pac;
	/* WiFi coexistence GPIO and delay */
	unsigned coex_delay_us;
	s8 coex_gpio;
	/* LNA/PA mode */
	s8 lna_pa_mode;
	/* Is auto-ack activated? */
	bool autoack;
	/* Is NFCC mode enabled */
	bool nfcc_mode;
	/* pgf calibration running */
	bool pgf_cal_running;
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
	struct spi_message *msg_read_sys_status_hi;
	struct spi_message *msg_read_sys_time;
	struct spi_message *msg_write_sys_status;
	struct spi_message *msg_read_dss_status;
	struct spi_message *msg_write_dss_status;
	struct spi_message *msg_write_spi_collision_status;
	/* Internal operational state of the chip */
	enum operational_state current_operational_state;
};

#endif /* __DW3000_H */
