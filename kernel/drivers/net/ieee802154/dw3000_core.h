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
#ifndef __DW3000_CORE_H
#define __DW3000_CORE_H

#include "dw3000.h"

/* Define DW3000 PDOA modes */
#define DW3000_PDOA_M0 0x0 /* PDOA mode is off */
#define DW3000_PDOA_M1 0x1 /* PDOA mode 1 */
#define DW3000_PDOA_M2 0x2 /* PDOA mode 2 (reserved or not supported) */
#define DW3000_PDOA_M3 0x3 /* PDOA mode 3 */
#define DW3000_PDOA_CONFIG_MASK 0x3

/* Define DW3000 STS modes */
#define DW3000_STS_MODE_OFF 0x0 /* STS is off */
#define DW3000_STS_MODE_1 0x1 /* STS mode 1 */
#define DW3000_STS_MODE_2 0x2 /* STS mode 2 */
#define DW3000_STS_MODE_ND 0x3 /* STS with no data */
#define DW3000_STS_MODE_SDC 0x8 /* Enable Super Deterministic Codes */
#define DW3000_STS_CONFIG_MASK 0xB

/* Constants for specifying TX Preamble length in symbols */
#define DW3000_PLEN_4096 0x03 /* Standard preamble length 4096 symbols */
#define DW3000_PLEN_2048 0x0A /* Non-standard preamble length 2048 symbols */
#define DW3000_PLEN_1536 0x06 /* Non-standard preamble length 1536 symbols */
#define DW3000_PLEN_1024 0x02 /* Standard preamble length 1024 symbols */
#define DW3000_PLEN_512 0x0d /* Non-standard preamble length 512 symbols */
#define DW3000_PLEN_256 0x09 /* Non-standard preamble length 256 symbols */
#define DW3000_PLEN_128 0x05 /* Non-standard preamble length 128 symbols */
#define DW3000_PLEN_72 0x07 /* Non-standard length 72 */
#define DW3000_PLEN_32 0x04 /* Non-standard length 32 */
#define DW3000_PLEN_64 0x01 /* Standard preamble length 64 symbols */

/* Constants for specifying the (Nominal) mean Pulse Repetition Frequency
   These are defined for direct write (with a shift if necessary) to CHAN_CTRL
   and TX_FCTRL regs */
#define DW3000_PRF_16M 1 /* UWB PRF 16 MHz */
#define DW3000_PRF_64M 2 /* UWB PRF 64 MHz */
#define DW3000_PRF_SCP 3 /* SCP UWB PRF ~100 MHz */

/* Constants for specifying Start of Frame Delimeter (SFD) type */
#define DW3000_SFD_TYPE_STD 0 /* Standard short IEEE802154 SFD (8 symbols) */
#define DW3000_SFD_TYPE_DW_8 1 /* Decawave-defined 8 symbols  SFD */
#define DW3000_SFD_TYPE_DW_16 2 /* Decawave-defined 16 symbols SFD */
#define DW3000_SFD_TYPE_4Z 3 /* IEEE802154z SFD (8 symbols) */

/* Constants for selecting the bit rate for data TX (and RX)
   These are defined for write (with just a shift) the TX_FCTRL register */
#define DW3000_BR_850K 0 /* UWB bit rate 850 kbits/s */
#define DW3000_BR_6M8 1 /* UWB bit rate 6.8 Mbits/s */

/* Constants for specifying Preamble Acquisition Chunk (PAC) Size in symbols */
#define DW3000_PAC8 0 /* recommended for RX of preamble length  128 and below */
#define DW3000_PAC16 1 /* recommended for RX of preamble length 256 */
#define DW3000_PAC32 2 /* recommended for RX of preamble length 512 */
#define DW3000_PAC4 3 /* recommended for RX of preamble length < 127 */

enum spi_modes {
	DW3000_SPI_RD_BIT = 0x0000U,
	DW3000_SPI_WR_BIT = 0x8000U,
	DW3000_SPI_AND_OR_8 = 0x8001U,
	DW3000_SPI_AND_OR_16 = 0x8002U,
	DW3000_SPI_AND_OR_32 = 0x8003U,
	DW3000_SPI_AND_OR_MSK = 0x0003U,
};

/* Defined constants for "mode" bitmask parameter passed into dw3000_starttx()
   function. */
#define DW3000_START_TX_IMMEDIATE 0x00 /* Send the frame immediately */
#define DW3000_START_TX_DELAYED \
	0x01 /* Send the frame at specified time (time
					must be less that half period away) */
#define DW3000_RESPONSE_EXPECTED \
	0x02 /* Will enable the receiver after TX has
					 completed */
#define DW3000_START_TX_DLY_REF \
	0x04 /* Send the frame at specified time (time
					in DREF_TIME register + any time in
					DX_TIME register) */
#define DW3000_START_TX_DLY_RS \
	0x08 /* Send the frame at specified time (time
				       in RX_TIME_0 register + any time in
				       DX_TIME register) */
#define DW3000_START_TX_DLY_TS \
	0x10 /* Send the frame at specified time (time
				       in TX_TIME_LO register + any time in
				       DX_TIME register) */
#define DW3000_START_TX_CCA \
	0x20 /* Send the frame if no preamble detected
				    within PTO time */

/* Frame filtering configuration options */
#define DW3000_FF_ENABLE_802_15_4 0x2 /* use 802.15.4 filtering rules */
#define DW3000_FF_DISABLE 0x0 /* disable FF */
#define DW3000_FF_BEACON_EN 0x001 /* beacon frames allowed */
#define DW3000_FF_DATA_EN 0x002 /* data frames allowed */
#define DW3000_FF_ACK_EN 0x004 /* ack frames allowed */
#define DW3000_FF_MAC_EN 0x008 /* mac control frames allowed */
#define DW3000_FF_RSVD_EN 0x010 /* reserved frame types allowed */
#define DW3000_FF_MULTI_EN 0x020 /* multipurpose frames allowed */
#define DW3000_FF_FRAG_EN 0x040 /* fragmented frame types allowed */
#define DW3000_FF_EXTEND_EN 0x080 /* extended frame types allowed */
#define DW3000_FF_COORD_EN \
	0x100 /* behave as coordinator (can receive frames
				    with no dest address (need PAN ID match)) */
#define DW3000_FF_IMPBRCAST_EN 0x200 /* allow MAC implicit broadcast */

void dw3000_init_config(struct dw3000 *dw);

int dw3000_init(struct dw3000 *dw);
void dw3000_remove(struct dw3000 *dw);

int dw3000_transfers_init(struct dw3000 *dw);
void dw3000_transfers_free(struct dw3000 *dw);

void dw3000_testmode(struct dw3000 *dw);
int dw3000_configure_chan(struct dw3000 *dw);

int dw3000_hardreset(struct dw3000 *dw);
int dw3000_softreset(struct dw3000 *dw);

int dw3000_setup_reset_gpio(struct dw3000 *dw);
int dw3000_setup_irq(struct dw3000 *dw);

int dw3000_enable(struct dw3000 *dw);
int dw3000_disable(struct dw3000 *dw);

int dw3000_seteui64(struct dw3000 *dw, __le64 val);
int dw3000_setpanid(struct dw3000 *dw, __le16 val);
int dw3000_setshortaddr(struct dw3000 *dw, __le16 val);
int dw3000_setpancoord(struct dw3000 *dw, bool active);
int dw3000_setpdoa(struct dw3000 *dw, u8 mode);
int dw3000_setsts(struct dw3000 *dw, u8 mode, enum dw3000_sts_lengths len);
int dw3000_setpromiscuous(struct dw3000 *dw, bool on);

int dw3000_clear_sys_status(struct dw3000 *dw, u32 clear_bits);
int dw3000_read_rx_timestamp(struct dw3000 *dw, u64 *rx_ts);
int dw3000_read_rdb_status(struct dw3000 *dw, u8 *status);
int dw3000_read_sys_status(struct dw3000 *dw, u32 *status);
int dw3000_read_sys_time(struct dw3000 *dw, u32 *sys_time);

int dw3000_poweron(struct dw3000 *dw);
int dw3000_poweroff(struct dw3000 *dw);

int dw3000_rx_enable(struct dw3000 *dw, int rx_delayed, u32 date_dtu,
		     u32 timeout_pac);
int dw3000_rx_disable(struct dw3000 *dw);

int dw3000_enable_autoack(struct dw3000 *dw, bool force);
int dw3000_disable_autoack(struct dw3000 *dw, bool force);

int dw3000_tx_frame(struct dw3000 *dw, struct sk_buff *skb, int tx_delayed,
		    u32 tx_date_dtu, int rx_delay_dly, u32 rx_timeout_pac);

s16 dw3000_readpdoa(struct dw3000 *dw);

void dw3000_isr(struct dw3000 *dw);

/* Preamble length related information. */
struct dw3000_plen_info {
	/* Preamble length in symbols. */
	int symb;
	/* PAC size in symbols. */
	int pac_symb;
	/* Register value for this preamble length. */
	uint8_t dw_reg;
	/* Register value for PAC size. */
	uint8_t dw_pac_reg;
};
extern const struct dw3000_plen_info _plen_info[];

/* Bitrate related information. */
struct dw3000_bitrate_info {
	/* Standard and Decawave non standard SFD length in symbols. */
	int sfd_symb[2];
	/* Chips per symbol for PHR. */
	int phr_chip_per_symb;
	/* Chips per symbol for data part. */
	int data_chip_per_symb;
};
extern const struct dw3000_bitrate_info _bitrate_info[];

/* PRF related information. */
struct dw3000_prf_info {
	/* Number of chips per symbol. */
	int chip_per_symb;
};
extern const struct dw3000_prf_info _prf_info[];

static inline int dw3000_compute_shr_dtu(struct dw3000 *dw)
{
	const struct dw3000_plen_info *plen_info =
		&_plen_info[dw->config.txPreambLength - 1];
	const int chip_per_symb =
		_prf_info[dw->config.txCode >= 9 ? DW3000_PRF_64M :
						   DW3000_PRF_16M]
			.chip_per_symb;
	const struct dw3000_bitrate_info *bitrate_info =
		&_bitrate_info[dw->config.dataRate];
	const int shr_symb =
		plen_info->symb +
		bitrate_info
			->sfd_symb[dw->config.sfdType ?
					   1 :
					   0]; /* TODO: support type 2 & 3 ? */
	return shr_symb * chip_per_symb / DW3000_CHIP_PER_DTU;
}

static inline int dw3000_compute_symbol_dtu(struct dw3000 *dw)
{
	const int chip_per_symb =
		_prf_info[dw->config.txCode >= 9 ? DW3000_PRF_64M :
						   DW3000_PRF_16M]
			.chip_per_symb;
	return chip_per_symb / DW3000_CHIP_PER_DTU;
}

static inline int dw3000_compute_chips_per_pac(struct dw3000 *dw)
{
	const int pac_symb = _plen_info[dw->config.txPreambLength - 1].pac_symb;
	const int chip_per_symb =
		_prf_info[dw->config.txCode >= 9 ? DW3000_PRF_64M :
						   DW3000_PRF_16M]
			.chip_per_symb;
	return chip_per_symb * pac_symb;
}

static inline int dw3000_compute_pre_timeout_pac(struct dw3000 *dw)
{
	/* Must be called AFTER dw->chips_per_pac initialisation */
	const int symb = _plen_info[dw->config.txPreambLength - 1].symb;
	const int pac_symb = _plen_info[dw->config.txPreambLength - 1].pac_symb;
	return (DW3000_RX_ENABLE_STARTUP_DLY * DW3000_CHIP_PER_DLY +
		dw->chips_per_pac - 1) /
		       dw->chips_per_pac +
	       symb / pac_symb + 2;
}

static inline void dw3000_update_timings(struct dw3000 *dw)
{
	struct mcps802154_llhw *llhw = dw->llhw;
	/* Update configuration dependant timings */
	llhw->shr_dtu = dw3000_compute_shr_dtu(dw);
	llhw->symbol_dtu = dw3000_compute_symbol_dtu(dw);
	/* The CCA detection time shall be equivalent to 40 data symbol periods,
	   Tdsym, for a nominal 850 kb/s, or equivalently, at least 8 (multiplexed)
	   preamble symbols should be captured in the CCA detection time. */
	llhw->cca_dtu = 8 * llhw->symbol_dtu;
	dw->chips_per_pac = dw3000_compute_chips_per_pac(dw);
	dw->pre_timeout_pac = dw3000_compute_pre_timeout_pac(dw);
}

#endif /* __DW3000_CORE_H */
