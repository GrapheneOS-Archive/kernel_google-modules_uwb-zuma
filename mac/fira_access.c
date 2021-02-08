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
 * FiRa ranging, access.
 *
 */

#include "fira_access.h"
#include "fira_session.h"
#include "fira_frame.h"

#include <linux/string.h>
#include <net/mcps802154_frame.h>

#include "warn_return.h"

#define FIRA_FRAME_MAX_SIZE 127

/**
 * fira_access_setup_frame() - Fill an access frame from a FiRa slot.
 * @local: FiRa context.
 * @frame: Access frame.
 * @slot: Corresponding slot.
 * @frame_dtu: frame transmission or reception date.
 * @is_tx: true for TX.
 * @is_rframe: true if a ranging frame.
 */
static void fira_access_setup_frame(struct fira_local *local,
				    struct mcps802154_access_frame *frame,
				    const struct fira_slot *slot, u32 frame_dtu,
				    bool is_tx, bool is_rframe)
{
	if (is_tx) {
		u8 flags = MCPS802154_TX_FRAME_TIMESTAMP_DTU;

		if (is_rframe) {
			struct fira_ranging_info *ranging_info;

			ranging_info =
				&local->ranging_info[slot->ranging_index];
			ranging_info->timestamps_rctu[slot->message_id] =
				mcps802154_tx_timestamp_dtu_to_rmarker_rctu(
					local->llhw, frame_dtu) +
				local->llhw->tx_rmarker_offset_rctu;

			flags |= MCPS802154_TX_FRAME_RANGING |
				 MCPS802154_TX_FRAME_SP3;
		}
		*frame = (struct mcps802154_access_frame){
			.is_tx = true,
				.tx_frame_info = {
					.timestamp_dtu = frame_dtu,
					.flags = flags,
				},
		};
	} else {
		u8 flags = MCPS802154_RX_INFO_TIMESTAMP_DTU;
		u16 request = 0;

		if (is_rframe) {
			flags |= MCPS802154_RX_INFO_RANGING |
				 MCPS802154_RX_INFO_SP3;
			request = MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU;
		}
		*frame = (struct mcps802154_access_frame){
			.is_tx = false,
				.rx = {
					.info = {
						.timestamp_dtu = frame_dtu,
						.flags = flags,
					},
					.frame_info_flags_request = request,
				},
		};
	}
}

static void fira_rx_frame_ranging(struct fira_local *local,
				  const struct fira_slot *slot,
				  const struct mcps802154_rx_frame_info *info)
{
	struct fira_ranging_info *ranging_info =
		&local->ranging_info[slot->ranging_index];

	if (!info || !(info->flags & MCPS802154_RX_FRAME_INFO_TIMESTAMP_RCTU)) {
		ranging_info->failed = true;
		return;
	}

	ranging_info->timestamps_rctu[slot->message_id] =
		info->timestamp_rctu - local->llhw->rx_rmarker_offset_rctu;
}

static void fira_rx_frame_control(struct fira_local *local,
				  const struct fira_slot *slot,
				  struct sk_buff *skb,
				  const struct mcps802154_rx_frame_info *info)
{
	struct fira_ranging_info *ranging_info =
		&local->ranging_info[slot->ranging_index];
	struct fira_session *session = local->current_session;
	struct mcps802154_ie_get_context ie_get = {};
	u32 sts_index;
	unsigned int n_slots;
	int last_slot_index;
	int i;

	if (!info || !(info->flags & MCPS802154_RX_FRAME_INFO_TIMESTAMP_DTU))
		goto failed_without_info;
	if (!fira_frame_header_check(local, skb, &ie_get, &sts_index))
		goto failed;

	if (!fira_frame_control_payload_check(local, skb, &ie_get, &n_slots))
		goto failed;

	session->block_start_dtu = info->timestamp_dtu;
	session->sts_index = sts_index;

	last_slot_index = 0;
	for (i = 1; i < n_slots; i++) {
		const struct fira_slot *slot = &local->slots[i];
		struct mcps802154_access_frame *frame = &local->frames[i];
		bool is_tx, is_rframe;
		u32 frame_dtu;

		is_tx = slot->tx_controlee_index != -1;
		is_rframe = slot->message_id <= FIRA_MESSAGE_ID_RFRAME_MAX;
		frame_dtu = info->timestamp_dtu +
			    session->params.slot_duration_dtu * slot->index;
		last_slot_index = slot->index;

		fira_access_setup_frame(local, frame, slot, frame_dtu, is_tx,
					is_rframe);
	}
	local->access.timestamp_dtu = info->timestamp_dtu;
	local->access.duration_dtu =
		session->params.slot_duration_dtu * (last_slot_index + 1);
	local->access.n_frames = n_slots;

	return;
failed:
	local->access.timestamp_dtu = info->timestamp_dtu;
	local->access.duration_dtu = session->params.slot_duration_dtu;
failed_without_info:
	ranging_info->failed = true;
}

static void fira_rx_frame_measurement_report(
	struct fira_local *local, const struct fira_slot *slot,
	struct sk_buff *skb, const struct mcps802154_rx_frame_info *info)
{
	struct fira_ranging_info *ranging_info =
		&local->ranging_info[slot->ranging_index];
	struct fira_session *session = local->current_session;
	struct mcps802154_ie_get_context ie_get = {};
	u32 sts_index;

	if (!info)
		goto failed;
	if (!fira_frame_header_check(local, skb, &ie_get, &sts_index) ||
	    sts_index != session->sts_index + slot->index)
		goto failed;

	if (!fira_frame_measurement_report_payload_check(local, slot, skb,
							 &ie_get))
		goto failed;

	return;
failed:
	ranging_info->failed = true;
}

static void
fira_rx_frame_result_report(struct fira_local *local,
			    const struct fira_slot *slot, struct sk_buff *skb,
			    const struct mcps802154_rx_frame_info *info)
{
	struct fira_ranging_info *ranging_info =
		&local->ranging_info[slot->ranging_index];
	struct fira_session *session = local->current_session;
	struct mcps802154_ie_get_context ie_get = {};
	u32 sts_index;

	if (!info)
		goto failed;
	if (!fira_frame_header_check(local, skb, &ie_get, &sts_index) ||
	    sts_index != session->sts_index + slot->index)
		goto failed;

	if (!fira_frame_result_report_payload_check(local, slot, skb, &ie_get))
		goto failed;

	return;
failed:
	ranging_info->failed = true;
}

static void fira_rx_frame(struct mcps802154_access *access, int frame_idx,
			  struct sk_buff *skb,
			  const struct mcps802154_rx_frame_info *info)
{
	struct fira_local *local = access_to_local(access);
	const struct fira_slot *slot = &local->slots[frame_idx];

	switch (slot->message_id) {
	case FIRA_MESSAGE_ID_RANGING_INITIATION:
	case FIRA_MESSAGE_ID_RANGING_RESPONSE:
	case FIRA_MESSAGE_ID_RANGING_FINAL:
		fira_rx_frame_ranging(local, slot, info);
		break;
	case FIRA_MESSAGE_ID_CONTROL:
		fira_rx_frame_control(local, slot, skb, info);
		break;
	case FIRA_MESSAGE_ID_MEASUREMENT_REPORT:
		fira_rx_frame_measurement_report(local, slot, skb, info);
		break;
	case FIRA_MESSAGE_ID_RESULT_REPORT:
		fira_rx_frame_result_report(local, slot, skb, info);
		break;
	case FIRA_MESSAGE_ID_CONTROL_UPDATE:
		break;
	default:
		WARN_UNREACHABLE_DEFAULT();
	}

	if (skb)
		kfree_skb(skb);

	/* Stop round on error. */
	if (local->ranging_info[slot->ranging_index].failed)
		access->n_frames = frame_idx + 1;

	if (frame_idx == access->n_frames - 1)
		fira_report(local);
}

static struct sk_buff *fira_tx_get_frame(struct mcps802154_access *access,
					 int frame_idx)
{
	struct fira_local *local = access_to_local(access);
	const struct fira_slot *slot = &local->slots[frame_idx];
	struct sk_buff *skb;

	if (slot->message_id <= FIRA_MESSAGE_ID_RFRAME_MAX)
		return NULL;

	skb = mcps802154_frame_alloc(local->llhw, FIRA_FRAME_MAX_SIZE,
				     GFP_KERNEL);
	WARN_RETURN_NULL(!skb);

	fira_frame_header_put(local, slot, skb);

	switch (slot->message_id) {
	case FIRA_MESSAGE_ID_CONTROL:
		fira_frame_control_payload_put(local, slot, skb);
		break;
	case FIRA_MESSAGE_ID_MEASUREMENT_REPORT:
		fira_frame_measurement_report_payload_put(local, slot, skb);
		break;
	case FIRA_MESSAGE_ID_RESULT_REPORT:
		fira_frame_result_report_payload_put(local, slot, skb);
		break;
	case FIRA_MESSAGE_ID_CONTROL_UPDATE:
		break;
	default: /* LCOV_EXCL_START */
		kfree_skb(skb);
		skb = NULL;
		WARN_UNREACHABLE_DEFAULT();
		/* LCOV_EXCL_STOP */
	}

	if (frame_idx == access->n_frames - 1)
		fira_report(local);

	return skb;
}

static void fira_tx_return(struct mcps802154_access *access, int frame_idx,
			   struct sk_buff *skb,
			   enum mcps802154_access_tx_return_reason reason)
{
	kfree_skb(skb);
}

static void fira_access_done(struct mcps802154_access *access)
{
	/* Nothing. */
}

struct mcps802154_access_ops fira_access_ops = {
	.rx_frame = fira_rx_frame,
	.tx_get_frame = fira_tx_get_frame,
	.tx_return = fira_tx_return,
	.access_done = fira_access_done,
};

static struct mcps802154_access *fira_access_nothing(struct fira_local *local)
{
	struct mcps802154_access *access;

	access = &local->access;
	access->method = MCPS802154_ACCESS_METHOD_NOTHING;
	access->ops = &fira_access_ops;
	access->duration_dtu = 0;

	return access;
}

static struct mcps802154_access *
fira_access_controller(struct fira_local *local, struct fira_session *session)
{
	struct mcps802154_access *access;
	struct mcps802154_access_frame *frame;
	struct fira_slot *s;
	struct fira_ranging_info *ri;
	u32 frame_dtu;
	int i;

	/* Only unicast for the moment. */
	local->src_short_addr = mcps802154_get_short_addr(local->llhw);
	local->dst_short_addr = session->params.controlees[0].short_addr;

	access = &local->access;
	access->method = MCPS802154_ACCESS_METHOD_MULTI;
	access->ops = &fira_access_ops;
	access->timestamp_dtu = session->block_start_dtu;
	access->frames = local->frames;

	ri = local->ranging_info;

	memset(ri, 0, sizeof(*ri));
	ri->short_addr = session->params.controlees[0].short_addr;
	ri++;

	local->n_ranging_info = ri - local->ranging_info;

	s = local->slots;

	s->index = 0;
	s->tx_controlee_index = -1;
	s->ranging_index = 0;
	s->message_id = FIRA_MESSAGE_ID_CONTROL;
	s++;
	s->index = 1;
	s->tx_controlee_index = -1;
	s->ranging_index = 0;
	s->message_id = FIRA_MESSAGE_ID_RANGING_INITIATION;
	s++;
	s->index = 2;
	s->tx_controlee_index = 0;
	s->ranging_index = 0;
	s->message_id = FIRA_MESSAGE_ID_RANGING_RESPONSE;
	s++;
	s->index = 3;
	s->tx_controlee_index = -1;
	s->ranging_index = 0;
	s->message_id = FIRA_MESSAGE_ID_RANGING_FINAL;
	s++;
	s->index = 4;
	s->tx_controlee_index = -1;
	s->ranging_index = 0;
	s->message_id = FIRA_MESSAGE_ID_MEASUREMENT_REPORT;
	s++;
	s->index = 5;
	s->tx_controlee_index = 0;
	s->ranging_index = 0;
	s->message_id = FIRA_MESSAGE_ID_RESULT_REPORT;
	s++;
	access->n_frames = s - local->slots;

	frame_dtu = access->timestamp_dtu;

	for (i = 0; i < access->n_frames; i++) {
		bool is_tx, is_rframe;

		s = &local->slots[i];
		frame = &local->frames[i];

		is_tx = s->tx_controlee_index == -1;
		is_rframe = s->message_id <= FIRA_MESSAGE_ID_RFRAME_MAX;

		fira_access_setup_frame(local, frame, s, frame_dtu, is_tx,
					is_rframe);

		frame_dtu += session->params.slot_duration_dtu;
	}
	access->duration_dtu = frame_dtu - access->timestamp_dtu;

	return access;
}

static struct mcps802154_access *
fira_access_controlee(struct fira_local *local, struct fira_session *session)
{
	struct mcps802154_access *access;
	struct mcps802154_access_frame *frame;
	struct fira_slot *s;
	struct fira_ranging_info *ri;

	/* Only unicast for the moment. */
	local->src_short_addr = mcps802154_get_short_addr(local->llhw);
	local->dst_short_addr = session->params.controller_short_addr;

	access = &local->access;
	access->method = MCPS802154_ACCESS_METHOD_MULTI;
	access->ops = &fira_access_ops;
	access->timestamp_dtu = session->block_start_dtu;
	access->duration_dtu = 0;
	access->frames = local->frames;
	access->n_frames = 1;

	ri = local->ranging_info;
	memset(ri, 0, sizeof(*ri));
	ri->short_addr = session->params.controller_short_addr;
	local->n_ranging_info = 1;

	s = local->slots;
	s->index = 0;
	s->tx_controlee_index = -1;
	s->ranging_index = 0;
	s->message_id = FIRA_MESSAGE_ID_CONTROL;

	frame = local->frames;
	*frame = (struct mcps802154_access_frame){
		.is_tx = false,
		.rx = {
			.info = {
				.timestamp_dtu = access->timestamp_dtu,
				.timeout_dtu = -1,
				.flags = MCPS802154_RX_INFO_TIMESTAMP_DTU,
			},
			.frame_info_flags_request
				= MCPS802154_RX_FRAME_INFO_TIMESTAMP_DTU,
		},
	};

	return access;
}

struct mcps802154_access *fira_get_access(struct mcps802154_region *region,
					  u32 next_timestamp_dtu,
					  int next_in_region_dtu,
					  int region_duration_dtu)
{
	struct fira_local *local = region_to_local(region);
	struct fira_session *session;

	session = fira_session_next(local, next_timestamp_dtu);
	local->current_session = session;

	if (!session)
		return fira_access_nothing(local);
	if (session->params.device_type == FIRA_DEVICE_TYPE_CONTROLLER)
		return fira_access_controller(local, session);
	else
		return fira_access_controlee(local, session);
}
