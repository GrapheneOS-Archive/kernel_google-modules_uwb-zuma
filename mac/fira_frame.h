/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2020-2022 Qorvo US, Inc.
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
 * Qorvo. Please contact Qorvo to inquire about licensing terms.
 */

#ifndef FIRA_FRAME_H
#define FIRA_FRAME_H

#include <linux/types.h>

struct fira_local;
struct fira_session;
struct fira_slot;
struct sk_buff;
struct mcps802154_ie_get_context;
struct fira_session_params;

/**
 * fira_frame_check_n_controlees() - Check the number of wanted
 * controlees.
 * @session: Current session.
 * @n_controlees: Wanted number of controlees.
 * @active: Is the session (supposed to be) active?
 *
 * Return: true if number of controlees fits.
 *
 * For an inactive session, the number of controlees is limited by the list
 * size, aka FIRA_CONTROLEES_MAX.
 * For an active session, it depends on the space left in messages, which is
 * determined by the session parameters.
 */
bool fira_frame_check_n_controlees(const struct fira_session *session,
				   size_t n_controlees, bool active);

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
 * fira_frame_rframe_payload_put() - Check availability of a custom data
 * payload, write it to tx frame.
 * @local: FiRa context.
 * @skb: Frame buffer.
 */
void fira_frame_rframe_payload_put(struct fira_local *local,
				   struct sk_buff *skb);

/**
 * fira_frame_header_check() - Check and consume FiRa header.
 * @local: FiRa context.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must be zero initialized.
 * @sts_index: STS index read from header.
 * @session_id: Session id read from header.
 *
 * Return: true if header is correct.
 */
bool fira_frame_header_check(const struct fira_local *local,
			     struct sk_buff *skb,
			     struct mcps802154_ie_get_context *ie_get,
			     u32 *sts_index, u32 *session_id);

/**
 * fira_frame_control_payload_check() - Check FiRa frame payload for a control
 * message.
 * @local: FiRa context.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must have been used to read header first.
 * @n_slots: Pointer where to store number of used slots.
 * @stop_ranging: True if the message indicates that the ranging must be stopped.
 * @block_stride_len: Pointer where to store number of blocks to stride.
 *
 * Return: true if message is correct. Extra payload is accepted.
 */
bool fira_frame_control_payload_check(struct fira_local *local,
				      struct sk_buff *skb,
				      struct mcps802154_ie_get_context *ie_get,
				      unsigned int *n_slots, bool *stop_ranging,
				      int *block_stride_len);

/**
 * fira_frame_measurement_report_payload_check() - Check FiRa frame payload for
 * a measurement report message.
 * @local: FiRa context.
 * @slot: Slot information.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must have been used to read header first.
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
 * @ie_get: Context used to read IE, must have been used to read header first.
 *
 * Return: true if message is correct. Extra payload is accepted.
 */
bool fira_frame_result_report_payload_check(
	struct fira_local *local, const struct fira_slot *slot,
	struct sk_buff *skb, struct mcps802154_ie_get_context *ie_get);

/**
 * fira_frame_rframe_payload_check() - Parse custom data from ranging frame.
 * @local: FiRa context.
 * @slot: Slot information.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must have been used to read header first.
 *
 * Return: true if message is correct. Extra payload is accepted.
 */
bool fira_frame_rframe_payload_check(struct fira_local *local,
				     const struct fira_slot *slot,
				     struct sk_buff *skb,
				     struct mcps802154_ie_get_context *ie_get);

/**
 * fira_frame_encrypt() - Terminate a frame and encrypt.
 * @local: FiRa context.
 * @slot: Corresponding slot.
 * @skb: Frame buffer.
 *
 * Return: 0 or error.
 */
int fira_frame_encrypt(struct fira_local *local, const struct fira_slot *slot,
		       struct sk_buff *skb);

/**
 * fira_frame_decrypt() - Decrypt payload.
 * @local: FiRa context.
 * @session: Session.
 * @slot: Corresponding slot.
 * @skb: Frame buffer, with header in front of data.
 * @header_len: Length of the MAC header, used for authentication.
 *
 * Return: 0 or error, -EBADMSG if not authenticated.
 */
int fira_frame_decrypt(struct fira_local *local, struct fira_session *session,
		       const struct fira_slot *slot, struct sk_buff *skb,
		       unsigned int header_len);

/**
 * fira_rx_frame_control_header_check() - Check control frame and consume
 * header.
 * @local: FiRa context.
 * @slot: Corresponding slot.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must be zero initialized.
 * @sts_index: STS index received.
 *
 * Return: Session context or NULL.
 */
struct fira_session *fira_rx_frame_control_header_check(
	struct fira_local *local, const struct fira_slot *slot,
	struct sk_buff *skb, struct mcps802154_ie_get_context *ie_get,
	u32 *sts_index);

/**
 * fira_frame_header_check_decrypt() - Check and consume header, and decrypt
 * payload.
 * @local: FiRa context.
 * @slot: Corresponding slot.
 * @skb: Frame buffer.
 * @ie_get: Context used to read IE, must be zero initialized.
 *
 * Return: 0 or error.
 */
int fira_frame_header_check_decrypt(struct fira_local *local,
				    const struct fira_slot *slot,
				    struct sk_buff *skb,
				    struct mcps802154_ie_get_context *ie_get);

#endif /* FIRA_FRAME_H */
