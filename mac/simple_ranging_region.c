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
 *
 * 802.15.4 mac common part sublayer, simple ranging using old Sevenhugs
 * protocol.
 *
 */

#include <asm/unaligned.h>
#include <linux/errno.h>
#include <linux/ieee802154.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/limits.h>
#include <net/af_ieee802154.h>

#include <net/mcps802154_schedule.h>
#include <net/mcps802154_frame.h>
#include <net/simple_ranging_region_nl.h>

#include "nl.h"
#include "warn_return.h"

#define TWR_SLOT_MS_TO_RCTU 67108864ull
#define TWR_SLOT_MS_MAX 64
#define TWR_SLOT_DEFAULT_RCTU (16 * TWR_SLOT_MS_TO_RCTU)

#define TWR_FUNCTION_CODE_POLL 0x40
#define TWR_FUNCTION_CODE_RESP 0x41
#define TWR_FUNCTION_CODE_FINAL 0x42
#define TWR_FUNCTION_CODE_REPORT 0x43

#define TWR_INITIATOR_REGION 0
#define TWR_RESPONDER_REGION 1

enum twr_frames {
	TWR_FRAME_POLL,
	TWR_FRAME_RESP,
	TWR_FRAME_FINAL,
	TWR_FRAME_REPORT,
	N_TWR_FRAMES,
};

struct simple_ranging_initiator {
	u64 poll_tx_timestamp_rctu;
	u64 final_tx_timestamp_rctu;
	int tof_half_tag_rctu;
	int local_pdoa_rad_q11;
	s16 remote_pdoa_rad_q11;
};

struct simple_ranging_responder {
	u64 poll_rx_timestamp_rctu;
	u64 resp_tx_timestamp_rctu;
	s16 local_pdoa_rad_q11;
	int tof_x4_rctu;
};

struct simple_ranging_local {
	struct mcps802154_open_region_handler orh;
	struct mcps802154_llhw *llhw;
	struct mcps802154_access access;
	struct mcps802154_access_frame frames[N_TWR_FRAMES];
	struct mcps802154_nl_ranging_request
		requests[MCPS802154_NL_RANGING_REQUESTS_MAX];
	unsigned int n_requests;
	unsigned int request_idx;
	int frequency_hz;
	struct mcps802154_nl_ranging_request current_request;
	unsigned long long slot_duration_rctu;
	bool is_responder;
	union {
		struct simple_ranging_initiator initiator;
		struct simple_ranging_responder responder;
	};
};

static inline struct simple_ranging_local *
orh_to_local(struct mcps802154_open_region_handler *orh)
{
	return container_of(orh, struct simple_ranging_local, orh);
}

static inline struct simple_ranging_local *
access_to_local(struct mcps802154_access *access)
{
	return container_of(access, struct simple_ranging_local, access);
}

struct twr_region {
	struct mcps802154_region region;
	struct simple_ranging_local *local;
};

/* Requests and reports. */

static void twr_requests_clear(struct simple_ranging_local *local)
{
	local->requests[0].id = 0;
	local->requests[0].frequency_hz = 1;
	local->requests[0].peer_extended_addr = 1;
	local->requests[0].remote_peer_extended_addr = 0;
	local->n_requests = 1;
	local->request_idx = 0;
	local->frequency_hz = 1;
}

static void twr_request_start(struct simple_ranging_local *local)
{
	if (local->request_idx >= local->n_requests)
		local->request_idx = 0;
	local->current_request = local->requests[local->request_idx];
}

static void twr_report(struct simple_ranging_local *local, int tof_rctu,
		       int local_pdoa_rad_q11, int remote_pdoa_rad_q11)
{
	struct mcps802154_nl_ranging_request *request = &local->current_request;
	int r;

	r = mcps802154_nl_ranging_report(local->llhw, request->id, tof_rctu,
					 local_pdoa_rad_q11,
					 remote_pdoa_rad_q11);

	if (r == -ECONNREFUSED)
		twr_requests_clear(local);
	else
		local->request_idx++;
}

/* Frames. */

#define TWR_FRAME_HEADER_SIZE                                             \
	(IEEE802154_FC_LEN + IEEE802154_SEQ_LEN + IEEE802154_PAN_ID_LEN + \
	 IEEE802154_EXTENDED_ADDR_LEN * 2)
#define TWR_FRAME_POLL_SIZE 1
#define TWR_FRAME_RESP_SIZE 3
#define TWR_FRAME_FINAL_SIZE 5
#define TWR_FRAME_REPORT_SIZE 5
#define TWR_FRAME_MAX_SIZE (TWR_FRAME_HEADER_SIZE + TWR_FRAME_REPORT_SIZE)

void twr_frame_header_fill_buf(u8 *buf, __le16 pan_id, __le64 dst, __le64 src)
{
	u16 fc = (IEEE802154_FC_TYPE_DATA | IEEE802154_FC_INTRA_PAN |
		  (IEEE802154_ADDR_LONG << IEEE802154_FC_DAMODE_SHIFT) |
		  (1 << IEEE802154_FC_VERSION_SHIFT) |
		  (IEEE802154_ADDR_LONG << IEEE802154_FC_SAMODE_SHIFT));
	u8 seq = 0;
	size_t pos = 0;
	put_unaligned_le16(fc, buf + pos);
	pos += IEEE802154_FC_LEN;
	buf[pos] = seq;
	pos += IEEE802154_SEQ_LEN;
	memcpy(buf + pos, &pan_id, sizeof(pan_id));
	pos += IEEE802154_PAN_ID_LEN;
	memcpy(buf + pos, &dst, sizeof(dst));
	pos += IEEE802154_EXTENDED_ADDR_LEN;
	memcpy(buf + pos, &src, sizeof(src));
}

void twr_frame_header_put(struct sk_buff *skb, __le16 pan_id, __le64 dst,
			  __le64 src)
{
	twr_frame_header_fill_buf(skb_put(skb, TWR_FRAME_HEADER_SIZE), pan_id,
				  dst, src);
}

bool twr_frame_header_check(struct sk_buff *skb, __le16 pan_id, __le64 dst,
			    __le64 src)
{
	u8 buf[TWR_FRAME_HEADER_SIZE];
	if (!pskb_may_pull(skb, TWR_FRAME_HEADER_SIZE))
		return false;
	twr_frame_header_fill_buf(buf, pan_id, dst, src);
	if (memcmp(skb->data, buf, TWR_FRAME_HEADER_SIZE) != 0)
		return false;
	return true;
}

bool twr_frame_header_check_no_src(struct sk_buff *skb, __le16 pan_id,
				   __le64 dst)
{
	/* Ignore source. */
	const int check_size =
		TWR_FRAME_HEADER_SIZE - IEEE802154_EXTENDED_ADDR_LEN;
	u8 buf[TWR_FRAME_HEADER_SIZE];
	if (!pskb_may_pull(skb, TWR_FRAME_HEADER_SIZE))
		return false;
	twr_frame_header_fill_buf(buf, pan_id, dst, 0);
	if (memcmp(skb->data, buf, check_size) != 0)
		return false;
	return true;
}

void twr_frame_poll_put(struct sk_buff *skb)
{
	u8 function_code = TWR_FUNCTION_CODE_POLL;
	skb_put_u8(skb, function_code);
}

bool twr_frame_poll_check(struct sk_buff *skb)
{
	u8 function_code;
	if (!pskb_may_pull(skb, TWR_FRAME_POLL_SIZE))
		return false;
	function_code = skb->data[0];
	if (function_code != TWR_FUNCTION_CODE_POLL)
		return false;
	return true;
}

void twr_frame_resp_put(struct sk_buff *skb, s16 local_pdoa_rad_q11)
{
	u8 function_code = TWR_FUNCTION_CODE_RESP;
	skb_put_u8(skb, function_code);
	put_unaligned_le16(local_pdoa_rad_q11, skb_put(skb, 2));
}

bool twr_frame_resp_check(struct sk_buff *skb, s16 *remote_pdoa_rad_q11)
{
	u8 function_code;
	if (!pskb_may_pull(skb, TWR_FRAME_RESP_SIZE))
		return false;
	function_code = skb->data[0];
	if (function_code != TWR_FUNCTION_CODE_RESP)
		return false;
	*remote_pdoa_rad_q11 = get_unaligned_le16(skb->data + 1);
	return true;
}

void twr_frame_final_put(struct sk_buff *skb, s32 tof_half_tag_rctu)
{
	u8 function_code = TWR_FUNCTION_CODE_FINAL;
	skb_put_u8(skb, function_code);
	put_unaligned_le32(tof_half_tag_rctu, skb_put(skb, 4));
}

bool twr_frame_final_check(struct sk_buff *skb, s32 *tof_half_tag_rctu)
{
	u8 function_code;
	if (!pskb_may_pull(skb, TWR_FRAME_FINAL_SIZE))
		return false;
	function_code = skb->data[0];
	if (function_code != TWR_FUNCTION_CODE_FINAL)
		return false;
	*tof_half_tag_rctu = get_unaligned_le32(skb->data + 1);
	return true;
}

void twr_frame_report_put(struct sk_buff *skb, s32 tof_x4_rctu)
{
	u8 function_code = TWR_FUNCTION_CODE_REPORT;
	skb_put_u8(skb, function_code);
	put_unaligned_le32(tof_x4_rctu, skb_put(skb, 4));
}

bool twr_frame_report_check(struct sk_buff *skb, s32 *tof_x4_rctu)
{
	u8 function_code;
	if (!pskb_may_pull(skb, TWR_FRAME_REPORT_SIZE))
		return false;
	function_code = skb->data[0];
	if (function_code != TWR_FUNCTION_CODE_REPORT)
		return false;
	*tof_x4_rctu = get_unaligned_le32(skb->data + 1);
	return true;
}

/* Access responder. */

static void twr_responder_rx_frame(struct mcps802154_access *access,
				   int frame_idx, struct sk_buff *skb,
				   const struct mcps802154_rx_frame_info *info)
{
	struct simple_ranging_local *local = access_to_local(access);
	struct mcps802154_nl_ranging_request *request = &local->current_request;

	if (!skb) {
		goto fail;

	} else {
		if (frame_idx == TWR_FRAME_POLL) {
			u64 resp_tx_start_rctu;
			if (!twr_frame_header_check_no_src(
				    skb, mcps802154_get_pan_id(local->llhw),
				    mcps802154_get_extended_addr(local->llhw)))
				goto fail_free_skb;
			request->peer_extended_addr = get_unaligned_le64(
				skb->data + TWR_FRAME_HEADER_SIZE -
				IEEE802154_EXTENDED_ADDR_LEN);
			skb_pull(skb, TWR_FRAME_HEADER_SIZE);

			if (!twr_frame_poll_check(skb))
				goto fail_free_skb;
			if (!(info->flags &
			      MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU))
				goto fail_free_skb;
			if (!(info->flags &
			      MCPS802154_RX_FRAME_INFO_RANGING_PDOA))
				local->responder.local_pdoa_rad_q11 = S16_MIN;
			else
				local->responder.local_pdoa_rad_q11 =
					info->ranging_pdoa_rad_q11;
			local->responder.poll_rx_timestamp_rctu =
				info->timestamp_rctu -
				local->llhw->rx_rmarker_offset_rctu;
			resp_tx_start_rctu = mcps802154_align_tx_timestamp_rctu(
				local->llhw,
				local->responder.poll_rx_timestamp_rctu +
					local->slot_duration_rctu);
			local->responder.resp_tx_timestamp_rctu =
				resp_tx_start_rctu +
				local->llhw->tx_rmarker_offset_rctu;
			/* Set the timings for the next frames. */
			access->frames[TWR_FRAME_RESP]
				.tx_frame_info.timestamp_rctu =
				resp_tx_start_rctu;
			access->frames[TWR_FRAME_FINAL].rx.info.timestamp_rctu =
				resp_tx_start_rctu + local->slot_duration_rctu;
			access->frames[TWR_FRAME_REPORT]
				.tx_frame_info.timestamp_rctu =
				resp_tx_start_rctu +
				2 * local->slot_duration_rctu;
		} else {
			int tof_half_rctu;
			u64 final_rx_timestamp_rctu;
			if (WARN_ON(frame_idx != TWR_FRAME_FINAL))
				goto fail_free_skb;
			if (!twr_frame_header_check(
				    skb, mcps802154_get_pan_id(local->llhw),
				    mcps802154_get_extended_addr(local->llhw),
				    request->peer_extended_addr))
				goto fail_free_skb;
			skb_pull(skb, TWR_FRAME_HEADER_SIZE);
			if (!twr_frame_final_check(skb, &tof_half_rctu))
				goto fail_free_skb;
			if (!(info->flags &
			      MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU))
				goto fail_free_skb;
			final_rx_timestamp_rctu =
				info->timestamp_rctu -
				local->llhw->rx_rmarker_offset_rctu;
			local->responder.tof_x4_rctu =
				tof_half_rctu -
				mcps802154_difference_timestamp_rctu(
					local->llhw,
					local->responder.resp_tx_timestamp_rctu,
					local->responder.poll_rx_timestamp_rctu) +
				mcps802154_difference_timestamp_rctu(
					local->llhw, final_rx_timestamp_rctu,
					local->responder.resp_tx_timestamp_rctu);
		}
	}

	kfree_skb(skb);
	return;
fail_free_skb:
	kfree_skb(skb);
fail:
	access->n_frames = frame_idx + 1;
}

static struct sk_buff *
twr_responder_tx_get_frame(struct mcps802154_access *access, int frame_idx)
{
	struct simple_ranging_local *local = access_to_local(access);
	struct mcps802154_nl_ranging_request *request = &local->current_request;
	struct sk_buff *skb = mcps802154_frame_alloc(
		local->llhw, TWR_FRAME_MAX_SIZE, GFP_KERNEL);
	twr_frame_header_put(skb, mcps802154_get_pan_id(local->llhw),
			     request->peer_extended_addr,
			     mcps802154_get_extended_addr(local->llhw));
	if (frame_idx == TWR_FRAME_RESP) {
		twr_frame_resp_put(skb, local->responder.local_pdoa_rad_q11);
	} else {
		if (!WARN_ON(frame_idx != TWR_FRAME_REPORT))
			twr_frame_report_put(skb, local->responder.tof_x4_rctu);
	}
	return skb;
}

static void
twr_responder_tx_return(struct mcps802154_access *access, int frame_idx,
			struct sk_buff *skb,
			enum mcps802154_access_tx_return_reason reason)
{
	kfree_skb(skb);
}

static void twr_responder_access_done(struct mcps802154_access *access)
{
	/* Nothing. */
}

struct mcps802154_access_ops twr_responder_access_ops = {
	.rx_frame = twr_responder_rx_frame,
	.tx_get_frame = twr_responder_tx_get_frame,
	.tx_return = twr_responder_tx_return,
	.access_done = twr_responder_access_done,
};

/* Access initiator. */

static void twr_rx_frame(struct mcps802154_access *access, int frame_idx,
			 struct sk_buff *skb,
			 const struct mcps802154_rx_frame_info *info)
{
	struct simple_ranging_local *local = access_to_local(access);
	struct mcps802154_nl_ranging_request *request = &local->current_request;
	u64 resp_rx_timestamp_rctu;

	if (!skb) {
		goto fail;

	} else {
		if (!twr_frame_header_check(
			    skb, mcps802154_get_pan_id(local->llhw),
			    mcps802154_get_extended_addr(local->llhw),
			    request->peer_extended_addr))
			goto fail_free_skb;
		skb_pull(skb, TWR_FRAME_HEADER_SIZE);

		if (frame_idx == TWR_FRAME_RESP) {
			if (!twr_frame_resp_check(
				    skb, &local->initiator.remote_pdoa_rad_q11))
				goto fail_free_skb;
			if (!(info->flags &
			      MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU))
				goto fail_free_skb;
			if (!(info->flags &
			      MCPS802154_RX_FRAME_INFO_RANGING_PDOA))
				local->initiator.local_pdoa_rad_q11 = INT_MIN;
			else
				local->initiator.local_pdoa_rad_q11 =
					info->ranging_pdoa_rad_q11;

			resp_rx_timestamp_rctu =
				info->timestamp_rctu -
				local->llhw->rx_rmarker_offset_rctu;
			local->initiator.tof_half_tag_rctu =
				mcps802154_difference_timestamp_rctu(
					local->llhw, resp_rx_timestamp_rctu,
					local->initiator.poll_tx_timestamp_rctu) -
				mcps802154_difference_timestamp_rctu(
					local->llhw,
					local->initiator.final_tx_timestamp_rctu,
					resp_rx_timestamp_rctu);
		} else {
			int report_tof_x4_rctu;
			if (WARN_ON(frame_idx != TWR_FRAME_REPORT))
				goto fail_free_skb;
			if (!twr_frame_report_check(skb, &report_tof_x4_rctu))
				goto fail_free_skb;

			twr_report(local, report_tof_x4_rctu / 4,
				   local->initiator.local_pdoa_rad_q11,
				   local->initiator.remote_pdoa_rad_q11);
		}
	}

	kfree_skb(skb);
	return;
fail_free_skb:
	kfree_skb(skb);
fail:
	twr_report(local, INT_MIN, INT_MIN, INT_MIN);
	access->n_frames = frame_idx + 1;
}

static struct sk_buff *twr_tx_get_frame(struct mcps802154_access *access,
					int frame_idx)
{
	struct simple_ranging_local *local = access_to_local(access);
	struct mcps802154_nl_ranging_request *request = &local->current_request;
	struct sk_buff *skb = mcps802154_frame_alloc(
		local->llhw, TWR_FRAME_MAX_SIZE, GFP_KERNEL);
	twr_frame_header_put(skb, mcps802154_get_pan_id(local->llhw),
			     request->peer_extended_addr,
			     mcps802154_get_extended_addr(local->llhw));
	if (frame_idx == TWR_FRAME_POLL) {
		twr_frame_poll_put(skb);
	} else {
		if (!WARN_ON(frame_idx != TWR_FRAME_FINAL))
			twr_frame_final_put(skb,
					    local->initiator.tof_half_tag_rctu);
	}
	return skb;
}

static void twr_tx_return(struct mcps802154_access *access, int frame_idx,
			  struct sk_buff *skb,
			  enum mcps802154_access_tx_return_reason reason)
{
	kfree_skb(skb);
}

static void twr_access_done(struct mcps802154_access *access)
{
	/* Nothing. */
}

struct mcps802154_access_ops twr_access_ops = {
	.rx_frame = twr_rx_frame,
	.tx_get_frame = twr_tx_get_frame,
	.tx_return = twr_tx_return,
	.access_done = twr_access_done,
};

/* Region common functions. */

static struct mcps802154_region *
twr_alloc(struct mcps802154_open_region_handler *orh)
{
	struct twr_region *r;
	r = kmalloc(sizeof(*r), GFP_KERNEL);
	if (!r)
		return NULL;
	r->local = orh_to_local(orh);
	return &r->region;
}

static void twr_free(struct mcps802154_region *region)
{
	kfree(container_of(region, struct twr_region, region));
}

/* Region responder. */

static struct mcps802154_access *
twr_responder_get_access(const struct mcps802154_region *region,
			 u32 next_timestamp_dtu, int next_in_region_dtu)
{
	struct twr_region *r = container_of(region, struct twr_region, region);
	struct mcps802154_access *access = &r->local->access;
	u32 start_dtu = next_timestamp_dtu + r->local->llhw->shr_dtu;
	access->method = MCPS802154_ACCESS_METHOD_MULTI;
	access->ops = &twr_responder_access_ops;
	access->n_frames = ARRAY_SIZE(r->local->frames);
	access->frames = r->local->frames;

	access->frames[TWR_FRAME_POLL].is_tx = false;
	access->frames[TWR_FRAME_POLL].rx.info.timestamp_dtu = start_dtu;
	access->frames[TWR_FRAME_POLL].rx.info.timeout_dtu = -1;
	access->frames[TWR_FRAME_POLL].rx.info.flags =
		MCPS802154_RX_INFO_TIMESTAMP_DTU | MCPS802154_RX_INFO_RANGING |
		MCPS802154_RX_INFO_ENABLE_STS;
	access->frames[TWR_FRAME_POLL].rx.frame_info_flags_request =
		MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU |
		MCPS802154_RX_FRAME_INFO_RANGING_PDOA;

	access->frames[TWR_FRAME_RESP].is_tx = true;
	access->frames[TWR_FRAME_RESP].tx_frame_info.flags =
		MCPS802154_TX_FRAME_TIMESTAMP_RCTU |
		MCPS802154_TX_FRAME_RANGING | MCPS802154_TX_FRAME_ENABLE_STS;
	access->frames[TWR_FRAME_RESP].tx_frame_info.rx_enable_after_tx_dtu = 0;
	access->frames[TWR_FRAME_RESP]
		.tx_frame_info.rx_enable_after_tx_timeout_dtu = 0;

	access->frames[TWR_FRAME_FINAL].is_tx = false;
	access->frames[TWR_FRAME_FINAL].rx.info.flags =
		MCPS802154_RX_INFO_TIMESTAMP_RCTU | MCPS802154_RX_INFO_RANGING |
		MCPS802154_RX_INFO_ENABLE_STS;
	access->frames[TWR_FRAME_FINAL].rx.info.timeout_dtu = 0;
	access->frames[TWR_FRAME_FINAL].rx.frame_info_flags_request =
		MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU;

	access->frames[TWR_FRAME_REPORT].is_tx = true;
	access->frames[TWR_FRAME_REPORT].tx_frame_info.flags =
		MCPS802154_TX_FRAME_TIMESTAMP_RCTU |
		MCPS802154_TX_FRAME_RANGING | MCPS802154_TX_FRAME_ENABLE_STS;
	access->frames[TWR_FRAME_REPORT].tx_frame_info.rx_enable_after_tx_dtu =
		0;
	access->frames[TWR_FRAME_REPORT]
		.tx_frame_info.rx_enable_after_tx_timeout_dtu = 0;

	return access;
}

static const struct mcps802154_region_ops
	simple_ranging_twr_responder_region_ops = {
		.name = "twr_resp",
		.alloc = twr_alloc,
		.get_access = twr_responder_get_access,
		.free = twr_free,
	};

/* Region initiator. */

static struct mcps802154_access *
twr_get_access(const struct mcps802154_region *region, u32 next_timestamp_dtu,
	       int next_in_region_dtu)
{
	struct twr_region *r = container_of(region, struct twr_region, region);
	struct mcps802154_access *access = &r->local->access;
	u64 start_rctu_unaligned, start_rctu;
	const int slots_dtu = r->local->slot_duration_rctu * N_TWR_FRAMES /
			      r->local->llhw->dtu_rctu;
	/* Only start a ranging request if we have enough time to end it. */
	if (next_in_region_dtu + slots_dtu > region->duration_dtu)
		return NULL;
	start_rctu_unaligned = mcps802154_timestamp_dtu_to_rctu(
		r->local->llhw, next_timestamp_dtu + r->local->llhw->shr_dtu);
	start_rctu = mcps802154_align_tx_timestamp_rctu(r->local->llhw,
							start_rctu_unaligned);
	twr_request_start(r->local);
	access->method = MCPS802154_ACCESS_METHOD_MULTI;
	access->ops = &twr_access_ops;
	access->n_frames = ARRAY_SIZE(r->local->frames);
	access->frames = r->local->frames;
	/* Hard-coded! */
	access->frames[TWR_FRAME_POLL].is_tx = true;
	access->frames[TWR_FRAME_POLL].tx_frame_info.timestamp_rctu =
		start_rctu;
	access->frames[TWR_FRAME_POLL].tx_frame_info.flags =
		MCPS802154_TX_FRAME_TIMESTAMP_RCTU |
		MCPS802154_TX_FRAME_RANGING | MCPS802154_TX_FRAME_ENABLE_STS;
	access->frames[TWR_FRAME_POLL].tx_frame_info.rx_enable_after_tx_dtu = 0;
	access->frames[TWR_FRAME_POLL]
		.tx_frame_info.rx_enable_after_tx_timeout_dtu = 0;
	r->local->initiator.poll_tx_timestamp_rctu =
		start_rctu + r->local->llhw->tx_rmarker_offset_rctu;

	access->frames[TWR_FRAME_RESP].is_tx = false;
	access->frames[TWR_FRAME_RESP].rx.info.timestamp_rctu =
		start_rctu + r->local->slot_duration_rctu;
	access->frames[TWR_FRAME_RESP].rx.info.timeout_dtu = 0;
	access->frames[TWR_FRAME_RESP].rx.info.flags =
		MCPS802154_RX_INFO_TIMESTAMP_RCTU | MCPS802154_RX_INFO_RANGING |
		MCPS802154_RX_INFO_ENABLE_STS;
	access->frames[TWR_FRAME_RESP].rx.frame_info_flags_request =
		MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU |
		MCPS802154_RX_FRAME_INFO_RANGING_PDOA;

	access->frames[TWR_FRAME_FINAL].is_tx = true;
	access->frames[TWR_FRAME_FINAL].tx_frame_info.timestamp_rctu =
		start_rctu + 2 * r->local->slot_duration_rctu;
	access->frames[TWR_FRAME_FINAL].tx_frame_info.flags =
		MCPS802154_TX_FRAME_TIMESTAMP_RCTU |
		MCPS802154_TX_FRAME_RANGING | MCPS802154_TX_FRAME_ENABLE_STS;
	access->frames[TWR_FRAME_FINAL].tx_frame_info.rx_enable_after_tx_dtu =
		0;
	access->frames[TWR_FRAME_FINAL]
		.tx_frame_info.rx_enable_after_tx_timeout_dtu = 0;
	r->local->initiator.final_tx_timestamp_rctu =
		start_rctu + 2 * r->local->slot_duration_rctu +
		r->local->llhw->tx_rmarker_offset_rctu;

	access->frames[TWR_FRAME_REPORT].is_tx = false;
	access->frames[TWR_FRAME_REPORT].rx.info.timestamp_rctu =
		start_rctu + 3 * r->local->slot_duration_rctu;
	access->frames[TWR_FRAME_REPORT].rx.info.timeout_dtu = 0;
	access->frames[TWR_FRAME_REPORT].rx.info.flags =
		MCPS802154_RX_INFO_TIMESTAMP_RCTU |
		MCPS802154_RX_INFO_ENABLE_STS;
	access->frames[TWR_FRAME_REPORT].rx.frame_info_flags_request = 0;
	return access;
}

static const struct mcps802154_region_ops simple_ranging_twr_region_ops = {
	.name = "twr",
	.alloc = twr_alloc,
	.get_access = twr_get_access,
	.free = twr_free,
};

/* Region handler. */

static struct mcps802154_open_region_handler *
simple_ranging_region_handler_open(struct mcps802154_llhw *llhw)
{
	struct simple_ranging_local *local;

	local = kzalloc(sizeof(*local), GFP_KERNEL);
	if (!local)
		return NULL;
	local->llhw = llhw;
	local->slot_duration_rctu = TWR_SLOT_DEFAULT_RCTU;
	local->is_responder = false;

	twr_requests_clear(local);
	return &local->orh;
}

static void
simple_ranging_region_handler_close(struct mcps802154_open_region_handler *orh)
{
	struct simple_ranging_local *local = orh_to_local(orh);
	kfree(local);
}

static int simple_ranging_region_handler_update_schedule(
	struct mcps802154_open_region_handler *orh,
	const struct mcps802154_schedule_update *schedule_update,
	u32 next_timestamp_dtu)
{
	struct simple_ranging_local *local = orh_to_local(orh);
	struct mcps802154_region *region;
	const int slot_dtu = local->slot_duration_rctu / local->llhw->dtu_rctu;
	int twr_slots = local->n_requests * N_TWR_FRAMES;
	int r;

	if (schedule_update->n_regions) {
		int schedule_duration_slots = local->llhw->dtu_freq_hz /
					      slot_dtu / local->frequency_hz;
		/* This treatment is done only for initiator.
		   Responder region never enters here. As it is an infinite
		   region. */
		WARN_ON(local->is_responder);
		if (schedule_duration_slots < twr_slots)
			schedule_duration_slots = twr_slots;

		r = mcps802154_schedule_set_start(
			schedule_update,
			schedule_update->expected_start_timestamp_dtu +
				(schedule_duration_slots - twr_slots) *
					slot_dtu);
		WARN_RETURN(r);
	}

	r = mcps802154_schedule_recycle(schedule_update, 0,
					MCPS802154_DURATION_NO_CHANGE);
	WARN_RETURN(r);

	if (local->is_responder) {
		region = mcps802154_schedule_add_region(
			schedule_update, TWR_RESPONDER_REGION, 0, 0);
	} else {
		region = mcps802154_schedule_add_region(schedule_update,
							TWR_INITIATOR_REGION, 0,
							twr_slots * slot_dtu);
	}
	if (!region)
		return -ENOMEM;

	return 0;
}

static int simple_ranging_region_handler_ranging_setup(
	struct mcps802154_open_region_handler *orh,
	const struct mcps802154_nl_ranging_request *requests,
	unsigned int n_requests)
{
	struct simple_ranging_local *local = orh_to_local(orh);
	if (local->is_responder) {
		return -EOPNOTSUPP;
	} else {
		int max_frequency_hz = 1;
		int i;

		if (n_requests > MCPS802154_NL_RANGING_REQUESTS_MAX)
			return -EINVAL;

		for (i = 0; i < n_requests; i++) {
			if (requests[i].remote_peer_extended_addr)
				return -EOPNOTSUPP;
			local->requests[i] = requests[i];
			if (requests[i].frequency_hz > max_frequency_hz)
				max_frequency_hz = requests[i].frequency_hz;
		}
		local->n_requests = n_requests;
		local->frequency_hz = max_frequency_hz;

		return 0;
	}
}

static int simple_ranging_region_handler_set_parameters(
	struct mcps802154_open_region_handler *orh,
	const struct nlattr *params_attr, struct netlink_ext_ack *extack)
{
	struct simple_ranging_local *local = orh_to_local(orh);
	struct nlattr *attrs[SIMPLE_RANGING_REGION_SET_PARAMETERS_ATTR_MAX + 1];
	int r;
	static const struct nla_policy nla_policy[SIMPLE_RANGING_REGION_SET_PARAMETERS_ATTR_MAX +
						  1] = {
		[SIMPLE_RANGING_REGION_SET_PARAMETERS_ATTR_SLOT_DURATION_MS] = { .type = NLA_U32 },
		[SIMPLE_RANGING_REGION_SET_PARAMETERS_ATTR_NODE_TYPE] = { .type = NLA_U32 },
	};

	r = nla_parse_nested(attrs,
			     SIMPLE_RANGING_REGION_SET_PARAMETERS_ATTR_MAX,
			     params_attr, nla_policy, extack);
	if (r)
		return r;

	if (attrs[SIMPLE_RANGING_REGION_SET_PARAMETERS_ATTR_SLOT_DURATION_MS]) {
		int slot_duration_ms = nla_get_u32(
			attrs[SIMPLE_RANGING_REGION_SET_PARAMETERS_ATTR_SLOT_DURATION_MS]);

		if (!slot_duration_ms ||
		    (slot_duration_ms & (slot_duration_ms - 1)) ||
		    slot_duration_ms > TWR_SLOT_MS_MAX)
			return -EINVAL;

		local->slot_duration_rctu =
			slot_duration_ms * TWR_SLOT_MS_TO_RCTU;
	}
	if (attrs[SIMPLE_RANGING_REGION_SET_PARAMETERS_ATTR_NODE_TYPE]) {
		u32 type = nla_get_u32(
			attrs[SIMPLE_RANGING_REGION_SET_PARAMETERS_ATTR_NODE_TYPE]);

		if (type > 1)
			return -EINVAL;

		local->is_responder = type == 1 ? true : false;
		mcps802154_schedule_invalidate(local->llhw);
	}

	return 0;
}

static const struct mcps802154_region_ops *const simple_ranging_regions_ops[] = {
	&simple_ranging_twr_region_ops,
	&simple_ranging_twr_responder_region_ops,
};

static struct mcps802154_region_handler simple_ranging_region_handler = {
	.owner = THIS_MODULE,
	.name = "simple-ranging",
	.n_regions_ops = ARRAY_SIZE(simple_ranging_regions_ops),
	.regions_ops = simple_ranging_regions_ops,
	.open = simple_ranging_region_handler_open,
	.close = simple_ranging_region_handler_close,
	.update_schedule = simple_ranging_region_handler_update_schedule,
	.ranging_setup = simple_ranging_region_handler_ranging_setup,
	.set_parameters = simple_ranging_region_handler_set_parameters,
};

int __init simple_ranging_region_init(void)
{
	return mcps802154_region_handler_register(
		&simple_ranging_region_handler);
}

void __exit simple_ranging_region_exit(void)
{
	mcps802154_region_handler_unregister(&simple_ranging_region_handler);
}
