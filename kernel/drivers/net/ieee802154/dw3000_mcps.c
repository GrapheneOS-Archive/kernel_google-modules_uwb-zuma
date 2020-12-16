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
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/pm_runtime.h>
#include <net/mcps802154.h>

#include "dw3000.h"
#include "dw3000_core.h"
#include "dw3000_mcps.h"
#include "dw3000_testmode.h"
#include "dw3000_trc.h"

static inline u64 timestamp_dtu_to_rctu(struct mcps802154_llhw *llhw,
					u32 timestamp_dtu);

static inline u32 timestamp_rctu_to_dtu(struct mcps802154_llhw *llhw,
					u64 timestamp_rctu);

static inline int dtu_to_pac(struct mcps802154_llhw *llhw, int timeout_dtu)
{
	struct dw3000 *dw = llhw->priv;

	return (timeout_dtu * DW3000_CHIP_PER_DTU + dw->chips_per_pac - 1) /
	       dw->chips_per_pac;
}

static inline int dtu_to_dly(struct mcps802154_llhw *llhw, int dtu)
{
	return (dtu * DW3000_CHIP_PER_DTU / DW3000_CHIP_PER_DLY);
}

static inline int rctu_to_dly(struct mcps802154_llhw *llhw, int rctu)
{
	return (rctu / DW3000_RCTU_PER_CHIP / DW3000_CHIP_PER_DLY);
}

static int do_start(struct dw3000 *dw, void *in, void *out)
{
#if (KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE)
	struct spi_controller *ctlr = dw->spi->controller;
#else
	struct spi_master *ctlr = dw->spi->master;
#endif
	/* Lock power management of SPI controller */
	int ret = pm_runtime_get_sync(ctlr->dev.parent);

	if (ret < 0) {
		pm_runtime_put_noidle(ctlr->dev.parent);
		dev_err(&ctlr->dev, "Failed to power device: %d\n", ret);
	}
	dw->has_lock_pm = !ret;
	/* Enable the device */
	return dw3000_enable(dw);
}

static int start(struct mcps802154_llhw *llhw)
{
	struct dw3000 *dw = llhw->priv;
	struct dw3000_stm_command cmd = { do_start, NULL, NULL };
	int ret;

	trace_dw3000_mcps_start(dw);
	ret = dw3000_enqueue_generic(dw, &cmd);
	trace_dw3000_return_int(dw, ret);
	return ret;
}

static int do_stop(struct dw3000 *dw, void *in, void *out)
{
	/* Disable the device */
	dw3000_disable(dw);
	/* Unlock power management of SPI controller */
	if (dw->has_lock_pm) {
#if (KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE)
		struct spi_controller *ctlr = dw->spi->controller;
#else
		struct spi_master *ctlr = dw->spi->master;
#endif
		pm_runtime_put(ctlr->dev.parent);
		dw->has_lock_pm = false;
	}
	return 0;
}

static void stop(struct mcps802154_llhw *llhw)
{
	struct dw3000 *dw = llhw->priv;
	struct dw3000_stm_command cmd = { do_stop, NULL, NULL };

	trace_dw3000_mcps_stop(dw);
	dw3000_enqueue_generic(dw, &cmd);
	trace_dw3000_return_void(dw);
}

struct do_tx_frame_params {
	struct sk_buff *skb;
	const struct mcps802154_tx_frame_info *info;
};

static int do_tx_frame(struct dw3000 *dw, void *in, void *out)
{
	struct do_tx_frame_params *params = (struct do_tx_frame_params *)in;
	const struct mcps802154_tx_frame_info *info = params->info;
	struct mcps802154_llhw *llhw = dw->llhw;
	u32 tx_date_dtu = 0;
	int rx_delay_dly = -1;
	u32 rx_timeout_pac = 0;
	int tx_delayed = 1;
	int rc;

	/* Enable STS */
	if (info->flags & MCPS802154_TX_FRAME_ENABLE_STS) {
		rc = dw3000_setsts(dw, DW3000_STS_MODE_1, DW3000_STS_LEN_256);
		if (unlikely(rc))
			return rc;
		rc = dw3000_setpdoa(dw, DW3000_PDOA_M3);
	} else {
		rc = dw3000_setsts(dw, DW3000_STS_MODE_OFF, DW3000_STS_LEN_256);
		if (unlikely(rc))
			return rc;
		rc = dw3000_setpdoa(dw, DW3000_PDOA_M0);
	}
	if (unlikely(rc))
		return rc;

	/* Calculate the transfer date.*/
	if (info->flags & MCPS802154_TX_FRAME_TIMESTAMP_DTU) {
		tx_date_dtu = info->timestamp_dtu + llhw->shr_dtu;
	} else if (info->flags & MCPS802154_TX_FRAME_TIMESTAMP_RCTU) {
		tx_date_dtu = timestamp_rctu_to_dtu(llhw, info->timestamp_rctu);
	} else {
		/* Send immediately. */
		tx_delayed = 0;
	}

	if (info->rx_enable_after_tx_dtu > 0) {
		/* Disable auto-ack if it was previously enabled. */
		dw3000_disable_autoack(dw, false);
		/* Calculate the after tx rx delay. */
		rx_delay_dly = dtu_to_dly(llhw, info->rx_enable_after_tx_dtu) -
			       DW3000_RX_ENABLE_STARTUP_DLY;
		rx_delay_dly = rx_delay_dly >= 0 ? rx_delay_dly : 0;
		/* Calculate the after tx rx timeout. */
		if (info->rx_enable_after_tx_timeout_dtu == 0) {
			rx_timeout_pac = dw->pre_timeout_pac;
		} else if (info->rx_enable_after_tx_timeout_dtu != -1) {
			rx_timeout_pac =
				dw->pre_timeout_pac +
				dtu_to_pac(
					llhw,
					info->rx_enable_after_tx_timeout_dtu);
		} else {
			/* No timeout. */
		}
	}
	return dw3000_tx_frame(dw, params->skb, tx_delayed, tx_date_dtu,
			       rx_delay_dly, rx_timeout_pac);
}

static int tx_frame(struct mcps802154_llhw *llhw, struct sk_buff *skb,
		    const struct mcps802154_tx_frame_info *info)
{
	struct dw3000 *dw = llhw->priv;
	struct do_tx_frame_params params = { skb, info };
	struct dw3000_stm_command cmd = { do_tx_frame, &params, NULL };
	int ret;

	trace_dw3000_mcps_tx_frame(dw, skb->len);
	ret = dw3000_enqueue_generic(dw, &cmd);
	trace_dw3000_return_int(dw, ret);
	return ret;
}

static int do_rx_enable(struct dw3000 *dw, void *in, void *out)
{
	struct mcps802154_rx_info *info = (struct mcps802154_rx_info *)(in);
	struct mcps802154_llhw *llhw = dw->llhw;
	u32 date_dtu = 0;
	u32 timeout_pac = 0;
	int rx_delayed = 1;
	int rc;

	/* Enable STS */
	if (info->flags & MCPS802154_RX_INFO_ENABLE_STS) {
		rc = dw3000_setsts(dw, DW3000_STS_MODE_1, DW3000_STS_LEN_256);
		if (unlikely(rc))
			return rc;
		rc = dw3000_setpdoa(dw, DW3000_PDOA_M3);
	} else {
		rc = dw3000_setsts(dw, DW3000_STS_MODE_OFF, DW3000_STS_LEN_256);
		if (unlikely(rc))
			return rc;
		rc = dw3000_setpdoa(dw, DW3000_PDOA_M0);
	}
	if (unlikely(rc))
		return rc;

	/* Calculate the transfer date. */
	if (info->flags & MCPS802154_RX_INFO_TIMESTAMP_DTU) {
		date_dtu = info->timestamp_dtu - DW3000_RX_ENABLE_STARTUP_DTU;
	} else if (info->flags & MCPS802154_RX_INFO_TIMESTAMP_RCTU) {
		date_dtu = timestamp_rctu_to_dtu(llhw, info->timestamp_rctu) -
			   llhw->shr_dtu - DW3000_RX_ENABLE_STARTUP_DTU;
	} else {
		/* Receive immediately. */
		rx_delayed = 0;
	}

	if (info->flags & MCPS802154_RX_INFO_AACK) {
		dw3000_enable_autoack(dw, false);
	} else {
		dw3000_disable_autoack(dw, false);
	}

	/* Calculate the timeout. */
	if (info->timeout_dtu == 0) {
		timeout_pac = dw->pre_timeout_pac;
	} else if (info->timeout_dtu != -1) {
		timeout_pac = dw->pre_timeout_pac +
			      dtu_to_pac(llhw, info->timeout_dtu);
	} else {
		/* No timeout. */
	}
	return dw3000_rx_enable(dw, rx_delayed, date_dtu, timeout_pac);
}

static int do_rx_disable(struct dw3000 *dw, void *in, void *out)
{
	return dw3000_rx_disable(dw);
}

static int rx_enable(struct mcps802154_llhw *llhw,
		     const struct mcps802154_rx_info *info)
{
	struct dw3000 *dw = llhw->priv;
	struct dw3000_stm_command cmd = { do_rx_enable, (void *)info, NULL };
	int ret;

	trace_dw3000_mcps_rx_enable(dw, info->flags, info->timeout_dtu);
	ret = dw3000_enqueue_generic(dw, &cmd);
	trace_dw3000_return_int(dw, ret);
	return ret;
}

static int rx_disable(struct mcps802154_llhw *llhw)
{
	struct dw3000 *dw = llhw->priv;
	struct dw3000_stm_command cmd = { do_rx_disable, NULL, NULL };
	int ret;

	trace_dw3000_mcps_rx_disable(dw);
	ret = dw3000_enqueue_generic(dw, &cmd);
	trace_dw3000_return_int(dw, ret);
	return ret;
}

static int rx_get_frame(struct mcps802154_llhw *llhw, struct sk_buff **skb,
			struct mcps802154_rx_frame_info *info)
{
	struct dw3000 *dw = llhw->priv;
	struct dw3000_rx *rx = &dw->rx;
	unsigned long flags;
	int ret = 0;
	u8 rx_flags;

	trace_dw3000_mcps_rx_get_frame(dw, info->flags);

	/* Sanity check parameters */
	if (unlikely(!info || !skb)) {
		ret = -EINVAL;
		goto error;
	}

	/* Acquire RX lock */
	spin_lock_irqsave(&rx->lock, flags);
	/* Check buffer available */
	if (unlikely(!rx->skb)) {
		spin_unlock_irqrestore(&rx->lock, flags);
		ret = -EAGAIN;
		goto error;
	}
	/* Give the last received frame we stored */
	*skb = rx->skb;
	rx->skb = NULL;
	rx_flags = rx->flags;
	/* Release RX lock */
	spin_unlock_irqrestore(&rx->lock, flags);

	if (info->flags & MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU) {
		if (dw3000_read_rx_timestamp(dw, &info->timestamp_rctu))
			info->flags &= ~MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU;
	}
	/* In case of auto-ack send. */
	if (rx_flags & DW3000_RX_FLAG_AACK)
		info->flags |= MCPS802154_RX_FRAME_INFO_AACK;
	/* In case of PDoA. */
	if (info->flags & MCPS802154_RX_FRAME_INFO_RANGING_PDOA)
		info->ranging_pdoa_rad_q11 = dw3000_readpdoa(dw);
	/* Keep only implemented. */
	info->flags &= (MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU |
			MCPS802154_RX_FRAME_INFO_AACK |
			MCPS802154_RX_FRAME_INFO_RANGING_PDOA);

	trace_dw3000_return_int_u32(dw, info->flags, (*skb)->len);
	return 0;

error:
	trace_dw3000_return_int_u32(dw, ret, 0);
	return ret;
}

static int rx_get_error_frame(struct mcps802154_llhw *llhw,
			      struct mcps802154_rx_frame_info *info)
{
	struct dw3000 *dw = llhw->priv;
	int ret = 0;

	trace_dw3000_mcps_rx_get_error_frame(dw, info->flags);
	/* Sanity check */
	if (unlikely(!info)) {
		ret = -EINVAL;
		goto error;
	}
	if (info->flags & MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU) {
		if (dw3000_read_rx_timestamp(dw, &info->timestamp_rctu))
			info->flags &= ~MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU;
	} else {
		/* Not implemented */
		info->flags = 0;
	}
error:
	trace_dw3000_return_int_u32(dw, ret, info->flags);
	return ret;
}

static int do_reset(struct dw3000 *dw, void *in, void *out)
{
	int rc;
	/* Disable the device before resetting it */
	rc = dw3000_disable(dw);
	if (rc) {
		dev_err(dw->dev, "device disable failed: %d\n", rc);
		return rc;
	}
	/* Soft reset */
	rc = dw3000_softreset(dw);
	if (rc != 0) {
		dev_err(dw->dev, "device reset failed: %d\n", rc);
		return rc;
	}
	/* Initialize & configure the device */
	rc = dw3000_init(dw);
	if (rc != 0) {
		dev_err(dw->dev, "device reset failed: %d\n", rc);
		return rc;
	}
	/* Enable the device */
	rc = dw3000_enable(dw);
	if (rc) {
		dev_err(dw->dev, "device enable failed: %d\n", rc);
		return rc;
	}
	return 0;
}

static int reset(struct mcps802154_llhw *llhw)
{
	struct dw3000 *dw = llhw->priv;
	struct dw3000_stm_command cmd = { do_reset, NULL, NULL };
	int ret;

	trace_dw3000_mcps_reset(dw);
	ret = dw3000_enqueue_generic(dw, &cmd);
	trace_dw3000_return_int(dw, ret);
	return ret;
}

static int do_get_dtu(struct dw3000 *dw, void *in, void *out)
{
	return dw3000_read_sys_time(dw, (u32 *)out);
}

static int get_current_timestamp_dtu(struct mcps802154_llhw *llhw,
				     u32 *timestamp_dtu)
{
	struct dw3000 *dw = llhw->priv;
	struct dw3000_stm_command cmd = { do_get_dtu, NULL, timestamp_dtu };
	int ret;

	trace_dw3000_mcps_get_timestamp(dw);
	ret = dw3000_enqueue_generic(dw, &cmd);
	trace_dw3000_return_int_u32(dw, ret, *timestamp_dtu);
	return ret;
}

static int do_get_rctu(struct dw3000 *dw, void *in, void *out)
{
	u32 systime;
	int ret = dw3000_read_sys_time(dw, &systime);

	if (!ret)
		*(u64 *)out = (u64)systime << 9;
	return ret;
}

static int get_current_timestamp_rctu(struct mcps802154_llhw *llhw,
				      u64 *timestamp_rctu)
{
	struct dw3000 *dw = llhw->priv;
	struct dw3000_stm_command cmd = { do_get_rctu, NULL, timestamp_rctu };
	int ret;

	trace_dw3000_mcps_get_rctu(dw);
	ret = dw3000_enqueue_generic(dw, &cmd);
	trace_dw3000_return_int_u64(dw, ret, *timestamp_rctu);
	return ret;
}

static inline u64 timestamp_dtu_to_rctu(struct mcps802154_llhw *llhw,
					u32 timestamp_dtu)
{
	return ((u64)timestamp_dtu) * DW3000_RCTU_PER_DTU;
}

static inline u32 timestamp_rctu_to_dtu(struct mcps802154_llhw *llhw,
					u64 timestamp_rctu)
{
	return (u32)(timestamp_rctu / DW3000_RCTU_PER_DTU);
}

static inline u64 align_tx_timestamp_rctu(struct mcps802154_llhw *llhw,
					  u64 timestamp_rctu)
{
	/* Aligned DTU date need to have the LSb zeroed, mask one extra bit. */
	const u64 bits_mask = DW3000_RCTU_PER_DTU * 2 - 1;

	return (timestamp_rctu + bits_mask) & ~bits_mask;
}

static inline s64 difference_timestamp_rctu(struct mcps802154_llhw *llhw,
					    u64 timestamp_a_rctu,
					    u64 timestamp_b_rctu)
{
	static const u64 rctu_rollover = 1ll << 40;
	static const u64 rctu_mask = rctu_rollover - 1;
	s64 diff_rctu = (timestamp_a_rctu - timestamp_b_rctu) & rctu_mask;

	if (diff_rctu & (rctu_rollover >> 1))
		diff_rctu = diff_rctu | ~rctu_mask;
	return diff_rctu;
}

static int compute_frame_duration_dtu(struct mcps802154_llhw *llhw,
				      int payload_bytes)
{
	struct dw3000 *dw = llhw->priv;
	const struct dw3000_prf_info *prf_info =
		&_prf_info[dw->config.txCode >= 9 ? DW3000_PRF_64M :
						    DW3000_PRF_16M];
	const struct dw3000_bitrate_info *bitrate_info =
		&_bitrate_info[dw->config.dataRate];
	/* STS part */
	const int sts_symb = dw->config.stsMode == DW3000_STS_MODE_OFF ?
				     0 :
				     32 << dw->config.stsLength;
	const int sts_chips = sts_symb * prf_info->chip_per_symb;
	/* PHR part. */
	static const int phr_tail_bits = 19 + 2;
	const int phr_chips = phr_tail_bits /* 1 bit/symbol */
			      * bitrate_info->phr_chip_per_symb;
	/* Data part, 48 Reed-Solomon bits per 330 bits. */
	const int data_bits = payload_bytes * 8;
	const int data_rs_bits = data_bits + (data_bits + 329) / 330 * 48;
	const int data_chips = data_rs_bits /* 1 bit/symbol */
			       * bitrate_info->data_chip_per_symb;
	/* Done, convert to dtu. */
	dev_dbg(dw->dev, "%s called\n", __func__);
	return llhw->shr_dtu +
	       (sts_chips + phr_chips + data_chips) / DW3000_CHIP_PER_DTU;
}

struct do_set_channel_params {
	u8 channel;
	u8 preamble_code;
};

static int do_set_channel(struct dw3000 *dw, void *in, void *out)
{
	struct dw3000_config *config = &dw->config;
	struct do_set_channel_params *params = in;
	/* Update configuration structure */
	config->chan = params->channel;
	config->txCode = params->preamble_code;
	config->rxCode = params->preamble_code;
	/* Reconfigure the chip with it */
	return dw3000_configure_chan(dw);
}

static int set_channel(struct mcps802154_llhw *llhw, u8 page, u8 channel,
		       u8 preamble_code)
{
	struct dw3000 *dw = llhw->priv;
	struct do_set_channel_params params = {
		.channel = channel, .preamble_code = preamble_code
	};
	struct dw3000_stm_command cmd = { do_set_channel, &params, NULL };
	int ret;

	trace_dw3000_mcps_set_channel(dw, page, channel, preamble_code);
	/* Check parameters early */
	if (page != 4 || (channel != 5 && channel != 9)) {
		ret = -EINVAL;
		goto error;
	}
	switch (preamble_code) {
	case 0:
		/* Set default value if MCPS don't give one to driver */
		params.preamble_code = 9;
		break;
	/* DW3000_PRF_16M */
	case 3:
	case 4:
	/* DW3000_PRF_64M */
	case 9:
	case 10:
	case 11:
	case 12:
		break;
	default:
		ret = -EINVAL;
		goto error;
	}
	ret = dw3000_enqueue_generic(dw, &cmd);
error:
	trace_dw3000_return_int(dw, ret);
	return ret;
}

static int set_hrp_uwb_params(struct mcps802154_llhw *llhw, int prf, int psr,
			      int sfd_selector, int phr_rate, int data_rate)
{
	struct dw3000 *dw = llhw->priv;

	dev_dbg(dw->dev, "%s called\n", __func__);
	return 0;
}

struct do_set_hw_addr_filt_params {
	struct ieee802154_hw_addr_filt *filt;
	unsigned long changed;
};

static int do_set_hw_addr_filt(struct dw3000 *dw, void *in, void *out)
{
	struct do_set_hw_addr_filt_params *params = in;
	struct ieee802154_hw_addr_filt *filt = params->filt;
	unsigned long changed = params->changed;
	int rc;

	if (changed & IEEE802154_AFILT_SADDR_CHANGED) {
		rc = dw3000_setshortaddr(dw, filt->short_addr);
		if (rc)
			goto spi_err;
	}
	if (changed & IEEE802154_AFILT_IEEEADDR_CHANGED) {
		rc = dw3000_seteui64(dw, filt->ieee_addr);
		if (rc)
			return rc;
	}
	if (changed & IEEE802154_AFILT_PANID_CHANGED) {
		rc = dw3000_setpanid(dw, filt->pan_id);
		if (rc)
			goto spi_err;
	}
	if (changed & IEEE802154_AFILT_PANC_CHANGED) {
		rc = dw3000_setpancoord(dw, filt->pan_coord);
		if (rc)
			goto spi_err;
	}
	return 0;

spi_err:
	return rc;
}

static int set_hw_addr_filt(struct mcps802154_llhw *llhw,
			    struct ieee802154_hw_addr_filt *filt,
			    unsigned long changed)
{
	struct dw3000 *dw = llhw->priv;
	struct do_set_hw_addr_filt_params params = { .filt = filt,
						     .changed = changed };
	struct dw3000_stm_command cmd = { do_set_hw_addr_filt, &params, NULL };
	int ret;

	trace_dw3000_mcps_set_hw_addr_filt(dw, (u8)changed);
	ret = dw3000_enqueue_generic(dw, &cmd);
	trace_dw3000_return_int(dw, ret);
	return ret;
}

static int set_txpower(struct mcps802154_llhw *llhw, s32 mbm)
{
	struct dw3000 *dw = llhw->priv;

	dev_dbg(dw->dev, "%s called\n", __func__);
	return 0;
}

static int set_cca_mode(struct mcps802154_llhw *llhw,
			const struct wpan_phy_cca *cca)
{
	struct dw3000 *dw = llhw->priv;

	dev_dbg(dw->dev, "%s called\n", __func__);
	return 0;
}

static int set_cca_ed_level(struct mcps802154_llhw *llhw, s32 mbm)
{
	struct dw3000 *dw = llhw->priv;

	dev_dbg(dw->dev, "%s called\n", __func__);
	return 0;
}

static int do_set_promiscuous_mode(struct dw3000 *dw, void *in, void *out)
{
	bool on = *(bool *)in;

	return dw3000_setpromiscuous(dw, on);
}

static int set_promiscuous_mode(struct mcps802154_llhw *llhw, bool on)
{
	struct dw3000 *dw = llhw->priv;
	struct dw3000_stm_command cmd = { do_set_promiscuous_mode, &on, NULL };

	dev_dbg(dw->dev, "%s called, (mode: %sabled)\n", __func__,
		(on) ? "en" : "dis");
	return dw3000_enqueue_generic(dw, &cmd);
}

static int set_calibration(struct mcps802154_llhw *llhw, const char *key,
			   void *value, size_t length)
{
	struct dw3000 *dw = llhw->priv;
	void *param;
	int len;
	/* Sanity checks */
	if (!key || !value || !length)
		return -EINVAL;
	/* Search parameter */
	len = dw3000_calib_parse_key(dw, key, &param);
	if (len < 0)
		return len;
	if (len > length)
		return -EINVAL;
	/* FIXME: This copy isn't big-endian compatible. */
	memcpy(param, value, len);
	/* One parameter has changed. */
	/* TODO: need reconfiguration? */
	return 0;
}

static int get_calibration(struct mcps802154_llhw *llhw, const char *key,
			   void *value, size_t length)
{
	struct dw3000 *dw = llhw->priv;
	void *param;
	int len;
	/* Sanity checks */
	if (!key)
		return -EINVAL;
	/* Calibration parameters */
	len = dw3000_calib_parse_key(dw, key, &param);
	if (len < 0)
		return len;
	if (len <= length)
		memcpy(value, param, len);
	else if (value && length)
		/* Provided buffer size isn't enough, return an error */
		return -ENOSPC;
	/* Return selected parameter length or error */
	return len;
}

static const char *const *list_calibration(struct mcps802154_llhw *llhw)
{
	return dw3000_calib_list_keys(llhw->priv);
}

static const struct mcps802154_ops dw3000_mcps_ops = {
	.start = start,
	.stop = stop,
	.tx_frame = tx_frame,
	.rx_enable = rx_enable,
	.rx_disable = rx_disable,
	.rx_get_frame = rx_get_frame,
	.rx_get_error_frame = rx_get_error_frame,
	.reset = reset,
	.get_current_timestamp_dtu = get_current_timestamp_dtu,
	.get_current_timestamp_rctu = get_current_timestamp_rctu,
	.timestamp_dtu_to_rctu = timestamp_dtu_to_rctu,
	.timestamp_rctu_to_dtu = timestamp_rctu_to_dtu,
	.align_tx_timestamp_rctu = align_tx_timestamp_rctu,
	.difference_timestamp_rctu = difference_timestamp_rctu,
	.compute_frame_duration_dtu = compute_frame_duration_dtu,
	.set_channel = set_channel,
	.set_hrp_uwb_params = set_hrp_uwb_params,
	.set_hw_addr_filt = set_hw_addr_filt,
	.set_txpower = set_txpower,
	.set_cca_mode = set_cca_mode,
	.set_cca_ed_level = set_cca_ed_level,
	.set_promiscuous_mode = set_promiscuous_mode,
	.set_calibration = set_calibration,
	.get_calibration = get_calibration,
	.list_calibration = list_calibration,
	MCPS802154_TESTMODE_CMD(dw3000_tm_cmd)
};

struct dw3000 *dw3000_mcps_alloc(struct device *dev)
{
	struct mcps802154_llhw *llhw;
	struct dw3000 *dw;

	dev_dbg(dev, "%s called\n", __func__);
	llhw = mcps802154_alloc_llhw(sizeof(*dw), &dw3000_mcps_ops);
	if (llhw == NULL)
		return NULL;
	dw = llhw->priv;
	dw->llhw = llhw;
	dw->dev = dev;
	dw3000_init_config(dw);

	/* Configure IEEE802154 HW capabilities */
	llhw->hw->flags =
		(IEEE802154_HW_TX_OMIT_CKSUM | IEEE802154_HW_AFILT |
		 IEEE802154_HW_PROMISCUOUS | IEEE802154_HW_RX_OMIT_CKSUM);
	llhw->flags = llhw->hw->flags;

	/* UWB High band 802.15.4a-2007. Only channels 5 & 9 for DW3000. */
	llhw->hw->phy->supported.channels[4] = (1 << 5) | (1 << 9);

	/* Set time related fields */
	llhw->dtu_freq_hz = DW3000_DTU_FREQ;
	llhw->dtu_rctu = DW3000_RCTU_PER_DTU;
	llhw->rstu_dtu = DW3000_DTU_PER_RSTU;
	llhw->anticip_dtu = 16 * (DW3000_DTU_FREQ / 1000);
	/* Set time related field that are configuration dependent */
	dw3000_update_timings(dw);
	/* Symbol is ~0.994us @ PRF16 or ~1.018us @ PRF64. Use 1. */
	llhw->hw->phy->symbol_duration = 1;

	/* Set extended address. */
	llhw->hw->phy->perm_extended_addr = 0xd6552cd6e41ceb57;

	/* Driver phy channel 5 on page 4 as default */
	llhw->hw->phy->current_channel = 5;
	llhw->hw->phy->current_page = 4;

	return dw;
}

void dw3000_mcps_free(struct dw3000 *dw)
{
	dev_dbg(dw->dev, "%s called\n", __func__);
	if (dw->llhw) {
		mcps802154_free_llhw(dw->llhw);
		dw->llhw = NULL;
	}
}

int dw3000_mcps_register(struct dw3000 *dw)
{
	dev_dbg(dw->dev, "%s called\n", __func__);
	return mcps802154_register_llhw(dw->llhw);
}

void dw3000_mcps_unregister(struct dw3000 *dw)
{
	dev_dbg(dw->dev, "%s called\n", __func__);
	mcps802154_unregister_llhw(dw->llhw);
}
