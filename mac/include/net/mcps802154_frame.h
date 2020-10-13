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
 * MCPS interface, tools to handle frames from a region handler.
 *
 */

#ifndef NET_MCPS802154_FRAME_H
#define NET_MCPS802154_FRAME_H

#include <linux/skbuff.h>

struct mcps802154_llhw;

/**
 * mcps802154_frame_alloc() - Allocate a buffer for TX.
 * @llhw: Low-level device pointer.
 * @size: Header and payload size.
 * @flags: Allocation mask.
 *
 * This is to allocate a buffer for sending a frame to the low-level driver
 * directly.  Additional space is reserved for low-level driver headroom and for
 * checksum.
 *
 * Return: Allocated buffer, or NULL.
 */
struct sk_buff *mcps802154_frame_alloc(struct mcps802154_llhw *llhw,
				       unsigned int size, gfp_t flags);

/**
 * mcps802154_get_extended_addr() - Get current extended address.
 * @llhw: Low-level device pointer.
 *
 * Return: Extended address.
 */
__le64 mcps802154_get_extended_addr(struct mcps802154_llhw *llhw);

/**
 * mcps802154_get_pan_id() - Get current PAN identifier.
 * @llhw: Low-level device pointer.
 *
 * Return: PAN ID.
 */
__le16 mcps802154_get_pan_id(struct mcps802154_llhw *llhw);

/**
 * mcps802154_get_short_addr() - Get current short address.
 * @llhw: Low-level device pointer.
 *
 * Return: Short address.
 */
__le16 mcps802154_get_short_addr(struct mcps802154_llhw *llhw);

/**
 * mcps802154_timestamp_dtu_to_rctu() - Convert a timestamp in device time unit
 * to a timestamp in ranging counter time unit.
 * @llhw: Low-level device pointer.
 * @timestamp_dtu: Timestamp value in device time unit.
 *
 * Return: Timestamp value in ranging counter time unit.
 */
u64 mcps802154_timestamp_dtu_to_rctu(struct mcps802154_llhw *llhw,
				     u32 timestamp_dtu);

/**
 * mcps802154_timestamp_rctu_to_dtu() - Convert a timestamp in ranging counter
 * time unit to a timestamp in device time unit.
 * @llhw: Low-level device pointer.
 * @timestamp_rctu: Timestamp value in ranging counter time unit.
 *
 * Return: Timestamp value in device time unit.
 */
u32 mcps802154_timestamp_rctu_to_dtu(struct mcps802154_llhw *llhw,
				     u64 timestamp_rctu);

/**
 * mcps802154_align_tx_timestamp_rctu() - Align a transmission timestamp so that
 * the transmission can be done at the exact timestamp value (RDEV only).
 * @llhw: Low-level device pointer.
 * @timestamp_rctu: Timestamp value to align.
 *
 * Return: Aligned timestamp.
 */
u64 mcps802154_align_tx_timestamp_rctu(struct mcps802154_llhw *llhw,
				       u64 timestamp_rctu);

/**
 * mcps802154_difference_timestamp_rctu() - Compute the difference between two
 * timestamp values.
 * @llhw: Low-level device pointer.
 * @timestamp_a_rctu: Timestamp A value.
 * @timestamp_b_rctu: Timestamp B value.
 *
 * Return: Difference between A and B.
 */
s64 mcps802154_difference_timestamp_rctu(struct mcps802154_llhw *llhw,
					 u64 timestamp_a_rctu,
					 u64 timestamp_b_rctu);

#endif /* NET_MCPS802154_FRAME_H */
