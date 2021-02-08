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
 * FiRa ranging, frame composition and parsing.
 *
 */

#ifndef FIRA_FRAME_H
#define FIRA_FRAME_H

#include <linux/types.h>

struct fira_local;
struct fira_slot;
struct sk_buff;
struct mcps802154_ie_get_context;

/**
 * fira_frame_header_put() - Fill FiRa frame header.
 * @local: FiRa context.
 * @slot: Slot information.
 * @skb: Frame buffer.
 */
void fira_frame_header_put(const struct fira_local *local,
			   const struct fira_slot *slot, struct sk_buff *skb);

/**
 * fira_frame_control_payload_put() - Fill FiRa frame payload for a control
 * message.
 * @local: FiRa context.
 * @slot: Slot information.
 * @skb: Frame buffer.
 */
void fira_frame_control_payload_put(const struct fira_local *local,
				    const struct fira_slot *slot,
				    struct sk_buff *skb);

/**
 * fira_frame_measurement_report_payload_put() - Fill FiRa frame payload for
 * a measurement report message.
 * @local: FiRa context.
 * @slot: Slot information.
 * @skb: Frame buffer.
 */
void fira_frame_measurement_report_payload_put(const struct fira_local *local,
					       const struct fira_slot *slot,
					       struct sk_buff *skb);

/**
 * fira_frame_result_report_payload_put() - Fill FiRa frame payload for a result
 * report message.
 * @local: FiRa context.
 * @slot: Slot information.
 * @skb: Frame buffer.
 */
void fira_frame_result_report_payload_put(const struct fira_local *local,
					  const struct fira_slot *slot,
					  struct sk_buff *skb);

/**
 * fira_frame_header_check() - Check and consume FiRa header.
 * @local: FiRa context.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must be zero initialized. When this
 * function returns, it contain the last decoded IE which can be the first
 * payload element.
 * @sts_index: STS index read from header.
 *
 * If this function returns true, caller needs to check the last decoded IE.
 *
 * Return: true if header is correct.
 */
bool fira_frame_header_check(const struct fira_local *local,
			     struct sk_buff *skb,
			     struct mcps802154_ie_get_context *ie_get,
			     u32 *sts_index);

/**
 * fira_frame_control_payload_check() - Check FiRa frame payload for a control
 * message.
 * @local: FiRa context.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must have been used to read header first,
 * should contain the first non header IE or nothing.
 * @n_slots: Pointer where to store number of used slots.
 *
 * Return: true if message is correct. Extra payload is accepted.
 */
bool fira_frame_control_payload_check(struct fira_local *local,
				      struct sk_buff *skb,
				      struct mcps802154_ie_get_context *ie_get,
				      unsigned int *n_slots);

/**
 * fira_frame_measurement_report_payload_check() - Check FiRa frame payload for
 * a measurement report message.
 * @local: FiRa context.
 * @slot: Slot information.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must have been used to read header first,
 * should contain the first non header IE or nothing.
 *
 * Return: true if message is correct. Extra payload is accepted.
 */
bool fira_frame_measurement_report_payload_check(
	struct fira_local *local, const struct fira_slot *slot,
	struct sk_buff *skb, struct mcps802154_ie_get_context *ie_get);

/**
 * fira_frame_result_report_payload_check() - Check FiRa frame payload for
 * a result report message.
 * @local: FiRa context.
 * @slot: Slot information.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must have been used to read header first,
 * should contain the first non header IE or nothing.
 *
 * Return: true if message is correct. Extra payload is accepted.
 */
bool fira_frame_result_report_payload_check(
	struct fira_local *local, const struct fira_slot *slot,
	struct sk_buff *skb, struct mcps802154_ie_get_context *ie_get);

#endif /* FIRA_FRAME_H */
