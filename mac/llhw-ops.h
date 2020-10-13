/*
 * 802.15.4 mac common part sublayer, low level driver operations.
 *
 * Copyright 2020 Sevenhugs
 */

#ifndef LLHW_OPS_H
#define LLHW_OPS_H

#include "mcps802154_i.h"
#include "trace.h"

static inline int llhw_start(struct mcps802154_local *local)
{
	int r;
	trace_llhw_start(local);
	r = local->ops->start(&local->llhw);
	trace_llhw_return_int(local, r);
	return r;
}

static inline void llhw_stop(struct mcps802154_local *local)
{
	trace_llhw_stop(local);
	local->ops->stop(&local->llhw);
	trace_llhw_return_void(local);
}

static inline int llhw_tx_frame(struct mcps802154_local *local,
				struct sk_buff *skb,
				const struct mcps802154_tx_frame_info *info)
{
	int r;
	trace_llhw_tx_frame(local, info);
	r = local->ops->tx_frame(&local->llhw, skb, info);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_rx_enable(struct mcps802154_local *local,
				 const struct mcps802154_rx_info *info)
{
	int r;
	trace_llhw_rx_enable(local, info);
	r = local->ops->rx_enable(&local->llhw, info);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_rx_disable(struct mcps802154_local *local)
{
	int r;
	trace_llhw_rx_disable(local);
	r = local->ops->rx_disable(&local->llhw);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_rx_get_frame(struct mcps802154_local *local,
				    struct sk_buff **skb,
				    struct mcps802154_rx_frame_info *info)
{
	int r;
	trace_llhw_rx_get_frame(local, info);
	r = local->ops->rx_get_frame(&local->llhw, skb, info);
	trace_llhw_return_rx_frame(local, r, info);
	return r;
}

static inline int llhw_rx_get_error_frame(struct mcps802154_local *local,
					  struct mcps802154_rx_frame_info *info)
{
	int r;
	trace_llhw_rx_get_error_frame(local, info);
	r = local->ops->rx_get_error_frame(&local->llhw, info);
	trace_llhw_return_rx_frame(local, r, info);
	return r;
}

static inline int llhw_reset(struct mcps802154_local *local)
{
	int r;
	trace_llhw_reset(local);
	r = local->ops->reset(&local->llhw);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_get_current_timestamp_dtu(struct mcps802154_local *local,
						 u32 *timestamp_dtu)
{
	int r;
	trace_llhw_get_current_timestamp_dtu(local);
	r = local->ops->get_current_timestamp_dtu(&local->llhw, timestamp_dtu);
	trace_llhw_return_timestamp_dtu(local, r, *timestamp_dtu);
	return r;
}

static inline int
llhw_get_current_timestamp_rctu(struct mcps802154_local *local,
				u64 *timestamp_rctu)
{
	int r;
	trace_llhw_get_current_timestamp_rctu(local);
	r = local->ops->get_current_timestamp_rctu(&local->llhw,
						   timestamp_rctu);
	trace_llhw_return_timestamp_rctu(local, r, *timestamp_rctu);
	return r;
}

static inline u64 llhw_timestamp_dtu_to_rctu(struct mcps802154_local *local,
					     u32 timestamp_dtu)
{
	return local->ops->timestamp_dtu_to_rctu(&local->llhw, timestamp_dtu);
}

static inline u32 llhw_timestamp_rctu_to_dtu(struct mcps802154_local *local,
					     u64 timestamp_rctu)
{
	return local->ops->timestamp_rctu_to_dtu(&local->llhw, timestamp_rctu);
}

static inline u64 llhw_align_tx_timestamp_rctu(struct mcps802154_local *local,
					       u64 timestamp_rctu)
{
	return local->ops->align_tx_timestamp_rctu(&local->llhw,
						   timestamp_rctu);
}

static inline s64 llhw_difference_timestamp_rctu(struct mcps802154_local *local,
						 u64 timestamp_a_rctu,
						 u64 timestamp_b_rctu)
{
	return local->ops->difference_timestamp_rctu(
		&local->llhw, timestamp_a_rctu, timestamp_b_rctu);
}

static inline int
llhw_compute_frame_duration_dtu(struct mcps802154_local *local,
				int payload_bytes)
{
	return local->ops->compute_frame_duration_dtu(&local->llhw,
						      payload_bytes);
}

static inline int llhw_set_channel(struct mcps802154_local *local, u8 page,
				   u8 channel, u8 preamble_code)
{
	int r;
	trace_llhw_set_channel(local, page, channel, preamble_code);
	r = local->ops->set_channel(&local->llhw, page, channel, preamble_code);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_set_hrp_uwb_params(struct mcps802154_local *local,
					  int prf, int psr, int sfd_selector,
					  int phr_rate, int data_rate)
{
	int r;
	trace_llhw_set_hrp_uwb_params(local, prf, psr, sfd_selector, phr_rate,
				      data_rate);
	r = local->ops->set_hrp_uwb_params(&local->llhw, prf, psr, sfd_selector,
					   phr_rate, data_rate);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_set_hw_addr_filt(struct mcps802154_local *local,
					struct ieee802154_hw_addr_filt *filt,
					unsigned long changed)
{
	int r;
	trace_llhw_set_hw_addr_filt(local, filt, changed);
	r = local->ops->set_hw_addr_filt(&local->llhw, filt, changed);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_set_txpower(struct mcps802154_local *local, s32 mbm)
{
	int r;
	trace_llhw_set_txpower(local, mbm);
	r = local->ops->set_txpower(&local->llhw, mbm);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_set_cca_mode(struct mcps802154_local *local,
				    const struct wpan_phy_cca *cca)
{
	int r;
	trace_llhw_set_cca_mode(local, cca);
	r = local->ops->set_cca_mode(&local->llhw, cca);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_set_cca_ed_level(struct mcps802154_local *local, s32 mbm)
{
	int r;
	trace_llhw_set_cca_ed_level(local, mbm);
	r = local->ops->set_cca_ed_level(&local->llhw, mbm);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_set_promiscuous_mode(struct mcps802154_local *local,
					    bool on)
{
	int r;
	trace_llhw_set_promiscuous_mode(local, on);
	r = local->ops->set_promiscuous_mode(&local->llhw, on);
	trace_llhw_return_int(local, r);
	return r;
}

static inline int llhw_set_scanning_mode(struct mcps802154_local *local,
					 bool on)
{
	int r;
	trace_llhw_set_scanning_mode(local, on);
	r = local->ops->set_scanning_mode(&local->llhw, on);
	trace_llhw_return_int(local, r);
	return r;
}

#endif /* LLHW_OPS_H */