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
 * 802.15.4 mac common part sublayer, fira ranging region.
 *
 */

#ifndef NET_FIRA_REGION_H
#define NET_FIRA_REGION_H

#include <net/mcps802154_schedule.h>

#define FIRA_VUPPER64_SIZE 8
#define FIRA_KEY_SIZE_MAX 32
#define FIRA_KEY_SIZE_MIN 16
#define FIRA_SLOT_DURATION_RSTU_DEFAULT 2400
#define FIRA_BLOCK_DURATION_MS_DEFAULT 200
#define FIRA_ROUND_DURATION_SLOTS_DEFAULT 30
#define FIRA_PRIORITY_MAX 100
#define FIRA_PRIORITY_DEFAULT 50
#define FIRA_IN_BAND_TERMINATION_ATTEMPT_COUNT_MAX 10
#define FIRA_IN_BAND_TERMINATION_ATTEMPT_COUNT_MIN 1
#define FIRA_BOOLEAN_MAX 1
#define FIRA_CONTROLEES_MAX 16
#define FIRA_FRAMES_MAX (3 + 3 * FIRA_CONTROLEES_MAX)
#define FIRA_CONTROLEE_FRAMES_MAX (3 + 3 + 1)
#define FIRA_RX_ANTENNA_PAIR_INVALID 0xff

enum fira_device_type {
	FIRA_DEVICE_TYPE_CONTROLEE,
	FIRA_DEVICE_TYPE_CONTROLLER,
};

enum fira_device_role {
	FIRA_DEVICE_ROLE_RESPONDER,
	FIRA_DEVICE_ROLE_INITIATOR,
};

enum fira_ranging_round_usage {
	FIRA_RANGING_ROUND_USAGE_OWR,
	FIRA_RANGING_ROUND_USAGE_SSTWR,
	FIRA_RANGING_ROUND_USAGE_DSTWR,
};

enum fira_multi_node_mode {
	FIRA_MULTI_NODE_MODE_UNICAST,
	FIRA_MULTI_NODE_MODE_ONE_TO_MANY,
	FIRA_MULTI_NODE_MODE_MANY_TO_MANY,
};

enum fira_measurement_report {
	FIRA_MEASUREMENT_REPORT_AT_RESPONDER,
	FIRA_MEASUREMENT_REPORT_AT_INITIATOR,
};

enum fira_embedded_mode {
	FIRA_EMBEDDED_MODE_DEFERRED,
	FIRA_EMBEDDED_MODE_NON_DEFERRED,
};

enum fira_rframe_config {
	FIRA_RFRAME_CONFIG_SP0,
	FIRA_RFRAME_CONFIG_SP1,
	FIRA_RFRAME_CONFIG_SP2,
	FIRA_RFRAME_CONFIG_SP3,
};

enum fira_prf_mode {
	FIRA_PRF_MODE_BPRF,
	FIRA_PRF_MODE_HPRF,
};

enum fira_preambule_duration {
	FIRA_PREAMBULE_DURATION_64,
	FIRA_PREAMBULE_DURATION_32,
};

enum fira_sfd_id {
	FIRA_SFD_ID_0,
	FIRA_SFD_ID_1,
	FIRA_SFD_ID_2,
	FIRA_SFD_ID_3,
	FIRA_SFD_ID_4,
};

enum fira_sts_segments {
	FIRA_STS_SEGMENTS_0,
	FIRA_STS_SEGMENTS_1,
	FIRA_STS_SEGMENTS_2,
};

enum fira_psdu_data_rate {
	FIRA_PSDU_DATA_RATE_6M81,
	FIRA_PSDU_DATA_RATE_7M80,
	FIRA_PSDU_DATA_RATE_27M2,
	FIRA_PSDU_DATA_RATE_31M2,
};

enum fira_phr_data_rate {
	FIRA_PHR_DATA_RATE_850k,
	FIRA_PHR_DATA_RATE_6M81,
};

enum fira_mac_fcs_type {
	FIRA_MAC_FCS_TYPE_CRC_16,
	FIRA_MAC_FCS_TYPE_CRC_32,
};

enum fira_rx_antenna_switch {
	FIRA_RX_ANTENNA_SWITCH_BETWEEN_ROUND,
	FIRA_RX_ANTENNA_SWITCH_DURING_ROUND,
	FIRA_RX_ANTENNA_SWITCH_TWO_RANGING,
};

enum fira_sts_config {
	FIRA_STS_CONFIG_STATIC,
	FIRA_STS_CONFIG_DYNAMIC,
	FIRA_STS_CONFIG_DYNAMIC_INDIVIDUAL_KEY,
};

struct fira_controlee {
	u32 sub_session_id;
	__le16 short_addr;
	u16 sub_session_key_len;
	char sub_session_key[FIRA_KEY_SIZE_MAX];
	bool sub_session;
};

/**
 * enum fira_message_id - Message identifiers, used in internal state and in
 * messages.
 * @FIRA_MESSAGE_ID_RANGING_INITIATION: Initial ranging message.
 * @FIRA_MESSAGE_ID_RANGING_RESPONSE: Response ranging message.
 * @FIRA_MESSAGE_ID_RANGING_FINAL: Final ranging message, only for DS-TWR.
 * @FIRA_MESSAGE_ID_CONTROL: Control message, sent by the controller.
 * @FIRA_MESSAGE_ID_MEASUREMENT_REPORT: Deferred report of ranging measures.
 * @FIRA_MESSAGE_ID_RESULT_REPORT: Report computed ranging result.
 * @FIRA_MESSAGE_ID_CONTROL_UPDATE: Message to change hopping.
 * @FIRA_MESSAGE_ID_RFRAME_MAX: Maximum identifier of message transmitted using
 * an RFRAME.
 * @FIRA_MESSAGE_ID_MAX: Maximum message identifier.
 */
enum fira_message_id {
	FIRA_MESSAGE_ID_RANGING_INITIATION = 0,
	FIRA_MESSAGE_ID_RANGING_RESPONSE = 1,
	FIRA_MESSAGE_ID_RANGING_FINAL = 2,
	FIRA_MESSAGE_ID_CONTROL = 3,
	FIRA_MESSAGE_ID_MEASUREMENT_REPORT = 4,
	FIRA_MESSAGE_ID_RESULT_REPORT = 5,
	FIRA_MESSAGE_ID_CONTROL_UPDATE = 6,
	FIRA_MESSAGE_ID_RFRAME_MAX = FIRA_MESSAGE_ID_RANGING_FINAL,
	FIRA_MESSAGE_ID_MAX = FIRA_MESSAGE_ID_CONTROL_UPDATE,
};

/**
 * struct fira_slot - Information on an active slot.
 */
struct fira_slot {
	/**
	 * @index: Index of this slot, add it to the block STS index to get the
	 * slot STS index. Note: there can be holes for a controlee as only
	 * relevant slots are recorded.
	 */
	int index;
	/**
	 * @tx_controlee_index: Index of the controlee transmitting in this
	 * slot, or -1 for the controller.
	 */
	int tx_controlee_index;
	/**
	 * @ranging_index: Index of the ranging in the ranging information
	 * table, -1 if none.
	 */
	int ranging_index;
	/**
	 * @message_id: Identifier of the message exchanged in this slot.
	 */
	enum fira_message_id message_id;
	/**
	 * @tx_ant: Tx antenna selection.
	 */
	int tx_ant;
	/**
	 * @rx_ant_pair: Rx antenna selection.
	 */
	int rx_ant_pair;
};

/**
 * struct fira_aoa_info - Ranging AoA information.
 */
struct fira_local_aoa_info {
	/**
	 * @pdoa_2pi: Phase Difference of Arrival.
	 */
	s16 pdoa_2pi;
	/**
	 * @aoa_2pi: Angle of Arrival.
	 */
	s16 aoa_2pi;
	/**
	 * @rx_ant_pair: Antenna pair index.
	 */
	u8 rx_ant_pair;
	/**
	 * @present: true if AoA information is present.
	 */
	bool present;
};

/**
 * struct fira_ranging_info - Ranging information.
 */
struct fira_ranging_info {
	/**
	 * @timestamps_rctu: Timestamps of the ranging messages.
	 */
	u64 timestamps_rctu[FIRA_MESSAGE_ID_RFRAME_MAX + 1];
	/**
	 * @tof_rctu: Computed Time of Flight.
	 */
	int tof_rctu;
	/**
	 * @local_aoa: Local ranging AoA information
	 */
	struct fira_local_aoa_info local_aoa;
	/**
	 * @local_aoa_azimuth: Azimuth ranging AoA information
	 */
	struct fira_local_aoa_info local_aoa_azimuth;
	/**
	 * @local_aoa_elevation: Elevation ranging AoA information
	 */
	struct fira_local_aoa_info local_aoa_elevation;
	/**
	 * @remote_aoa_azimuth_2pi: Remote azimuth AoA.
	 */
	s16 remote_aoa_azimuth_2pi;
	/**
	 * @remote_aoa_elevation_pi: Remote elevation AoA.
	 */
	s16 remote_aoa_elevation_pi;
	/**
	 * @remote_aoa_azimuth_fom: Remote azimuth FoM.
	 */
	u8 remote_aoa_azimuth_fom;
	/**
	 * @remote_aoa_elevation_fom: Remote elevation FoM.
	 */
	u8 remote_aoa_elevation_fom;
	/**
	 * @short_addr: Peer short address.
	 */
	__le16 short_addr;
	/**
	 * @failed: true if this ranging is failed (frame not received, bad
	 * frame, bad measurements...)
	 */
	bool failed;
	/**
	 * @tof_present: true if time of flight information is present.
	 */
	bool tof_present;
	/**
	 * @remote_aoa_azimuth_present: true if azimuth AoA information is present.
	 */
	bool remote_aoa_azimuth_present;
	/**
	 * @remote_aoa_elevation_present: true if elevation AoA information is present.
	 */
	bool remote_aoa_elevation_present;
	/**
	 * @remote_aoa_fom_present: true if FoM for AoA is present.
	 */
	bool remote_aoa_fom_present;
};

/**
 * struct fira_local - Local context.
 */
struct fira_local {
	/**
	 * @region: Region instance returned to MCPS.
	 */
	struct mcps802154_region region;
	/**
	 * @llhw: Low-level device pointer.
	 */
	struct mcps802154_llhw *llhw;
	/**
	 * @access: Access returned to MCPS.
	 */
	struct mcps802154_access access;
	/**
	 * @frames: Access frames referenced from access.
	 */
	struct mcps802154_access_frame frames[FIRA_FRAMES_MAX];
	/**
	 * @current_session: Pointer to the current session.
	 */
	struct fira_session *current_session;
	/**
	 * @slots: Descriptions of each active slots for the current session.
	 * When controller, this is filled when the access is requested. When
	 * controlee, the first slot is filled when the access is requested and
	 * the other slots are filled when the control message is received.
	 */
	struct fira_slot slots[FIRA_FRAMES_MAX];
	/**
	 * @ranging_info: Information on ranging for the current session. Index
	 * in the table is determined by the order of the ranging messages.
	 * First ranging exchange is put at index 0. When a message is shared
	 * between several exchanges, its information is stored at index 0.
	 * Reset when access is requested.
	 */
	struct fira_ranging_info ranging_info[FIRA_CONTROLEES_MAX];
	/**
	 * @n_ranging_info: Number of element in the ranging information table.
	 */
	int n_ranging_info;
	/**
	 * @n_ranging_valid: Number of valid ranging in the current ranging
	 * information table.
	 */
	int n_ranging_valid;
	/**
	 * @src_short_addr: Source address for the current session (actually
	 * never put as a source address in a frame, but used for control
	 * message).
	 */
	__le16 src_short_addr;
	/**
	 * @dst_short_addr: Destination address for the current session. When
	 * controller, this is broadcast of the address of the only controlee.
	 * When controlee, this is the address of the controller.
	 */
	__le16 dst_short_addr;
	/**
	 * @inactive_sessions: List of inactive sessions.
	 */
	struct list_head inactive_sessions;
	/**
	 * @active_sessions: List of active sessions.
	 */
	struct list_head active_sessions;
};

static inline struct fira_local *
region_to_local(struct mcps802154_region *region)
{
	return container_of(region, struct fira_local, region);
}

static inline struct fira_local *
access_to_local(struct mcps802154_access *access)
{
	return container_of(access, struct fira_local, access);
}

void fira_report(struct fira_local *local);

#endif /* NET_FIRA_REGION_H */