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
 * 802.15.4 mac common part sublayer, fira ranging call procedures.
 *
 */

#include <linux/errno.h>
#include <linux/string.h>

#include <net/fira_region_nl.h>
#include <net/mcps802154_frame.h>

#include "fira_session.h"
#include "fira_region_call.h"

static const struct nla_policy fira_call_nla_policy[FIRA_CALL_ATTR_MAX + 1] = {
	[FIRA_CALL_ATTR_SESSION_ID] = { .type = NLA_U32 },
	[FIRA_CALL_ATTR_SESSION_PARAMS] = { .type = NLA_NESTED },
	[FIRA_CALL_ATTR_CONTROLEES] = { .type = NLA_NESTED_ARRAY },
};

static const struct nla_policy fira_session_param_nla_policy[FIRA_SESSION_PARAM_ATTR_MAX +
							     1] = {
	/* clang-format off */
	[FIRA_SESSION_PARAM_ATTR_DEVICE_TYPE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_DEVICE_TYPE_CONTROLLER
	},
	[FIRA_SESSION_PARAM_ATTR_DEVICE_ROLE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_DEVICE_ROLE_INITIATOR
	},
	[FIRA_SESSION_PARAM_ATTR_RANGING_ROUND_USAGE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_RANGING_ROUND_USAGE_DSTWR
	},
	[FIRA_SESSION_PARAM_ATTR_MULTI_NODE_MODE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_MULTI_NODE_MODE_MANY_TO_MANY
	},
	[FIRA_SESSION_PARAM_ATTR_SHORT_ADDR] = { .type = NLA_U16 },
	[FIRA_SESSION_PARAM_ATTR_DESTINATION_SHORT_ADDR] = { .type = NLA_U16 },
	[FIRA_SESSION_PARAM_ATTR_INITIATION_TIME_MS] = { .type = NLA_U32 },
	[FIRA_SESSION_PARAM_ATTR_SLOT_DURATION_RSTU] = { .type = NLA_U32 },
	[FIRA_SESSION_PARAM_ATTR_BLOCK_DURATION_MS] = { .type = NLA_U32 },
	[FIRA_SESSION_PARAM_ATTR_ROUND_DURATION_SLOTS] = { .type = NLA_U32 },
	[FIRA_SESSION_PARAM_ATTR_BLOCK_STRIDING_VALUE] = { .type = NLA_U32 },
	[FIRA_SESSION_PARAM_ATTR_MAX_RR_RETRY] = { .type = NLA_U32 },
	[FIRA_SESSION_PARAM_ATTR_ROUND_HOPPING] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_BLOCK_STRIDING] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_PRIORITY] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_PRIORITY_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_RESULT_REPORT_PHASE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_MR_AT_INITIATOR] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_MEASUREMENT_REPORT_AT_INITIATOR
	},
	[FIRA_SESSION_PARAM_ATTR_EMBEDDED_MODE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_EMBEDDED_MODE_NON_DEFERRED
	},
	[FIRA_SESSION_PARAM_ATTR_IN_BAND_TERMINATION_ATTEMPT_COUNT] = {
		.type = NLA_U32, .validation_type = NLA_VALIDATE_RANGE,
		.min = FIRA_IN_BAND_TERMINATION_ATTEMPT_COUNT_MIN,
		.max = FIRA_IN_BAND_TERMINATION_ATTEMPT_COUNT_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_CHANNEL_NUMBER] = { .type = NLA_U8 },
	[FIRA_SESSION_PARAM_ATTR_PREAMBLE_CODE_INDEX] = { .type = NLA_U8 },
	[FIRA_SESSION_PARAM_ATTR_RFRAME_CONFIG] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_RFRAME_CONFIG_SP3
	},
	[FIRA_SESSION_PARAM_ATTR_PRF_MODE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_PRF_MODE_HPRF
	},
	[FIRA_SESSION_PARAM_ATTR_PREAMBLE_DURATION] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_PREAMBULE_DURATION_32
	},
	[FIRA_SESSION_PARAM_ATTR_SFD_ID] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_SFD_ID_4
	},
	[FIRA_SESSION_PARAM_ATTR_NUMBER_OF_STS_SEGMENTS] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_STS_SEGMENTS_2
	},
	[FIRA_SESSION_PARAM_ATTR_PSDU_DATA_RATE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_PSDU_DATA_RATE_31M2
	},
	[FIRA_SESSION_PARAM_ATTR_BPRF_PHR_DATA_RATE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_PHR_DATA_RATE_6M81
	},
	[FIRA_SESSION_PARAM_ATTR_MAC_FCS_TYPE] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_MAC_FCS_TYPE_CRC_32
	},
	[FIRA_SESSION_PARAM_ATTR_TX_ADAPTIVE_PAYLOAD_POWER] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_RX_ANTENNA_SELECTION] = { .type = NLA_U8 },
	[FIRA_SESSION_PARAM_ATTR_RX_ANTENNA_AZIMUTH] = { .type = NLA_U8 },
	[FIRA_SESSION_PARAM_ATTR_RX_ANTENNA_ELEVATION] = { .type = NLA_U8 },
	[FIRA_SESSION_PARAM_ATTR_TX_ANTENNA_SELECTION] = { .type = NLA_U8 },
	[FIRA_SESSION_PARAM_ATTR_RX_ANTENNA_SWITCH] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_RX_ANTENNA_SWITCH_TWO_RANGING
	},
	[FIRA_SESSION_PARAM_ATTR_STS_CONFIG] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_STS_CONFIG_DYNAMIC_INDIVIDUAL_KEY
	},
	[FIRA_SESSION_PARAM_ATTR_SUB_SESSION_ID] = { .type = NLA_U32 },
	[FIRA_SESSION_PARAM_ATTR_VUPPER64] = { .type = NLA_U64 },
	[FIRA_SESSION_PARAM_ATTR_SESSION_KEY] = {
		.type = NLA_BINARY, .len = FIRA_KEY_SIZE_MAX },
	[FIRA_SESSION_PARAM_ATTR_SUB_SESSION_KEY] = {
		.type = NLA_BINARY, .len = FIRA_KEY_SIZE_MAX },
	[FIRA_SESSION_PARAM_ATTR_KEY_ROTATION] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX },
	[FIRA_SESSION_PARAM_ATTR_KEY_ROTATION_RATE] = { .type = NLA_U8 },
	[FIRA_SESSION_PARAM_ATTR_AOA_RESULT_REQ] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_REPORT_TOF] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_REPORT_AOA_AZIMUTH] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_REPORT_AOA_ELEVATION] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX
	},
	[FIRA_SESSION_PARAM_ATTR_REPORT_AOA_FOM] = {
		.type = NLA_U8, .validation_type = NLA_VALIDATE_MAX,
		.max = FIRA_BOOLEAN_MAX
	},
	/* clang-format on */
};

/**
 * fira_session_init() - Initialize Fira session.
 * @local: FiRa context.
 * @session_id: Fira session id.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int fira_session_init(struct fira_local *local, u32 session_id,
			     const struct genl_info *info)
{
	bool active;
	struct fira_session *session;

	session = fira_session_get(local, session_id, &active);
	if (session)
		return -EBUSY;

	session = fira_session_new(local, session_id);
	if (!session)
		return -ENOMEM;

	return 0;
}

/**
 * fira_session_start() - Start Fira session.
 * @local: FiRa context.
 * @session_id: Fira session id.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int fira_session_start(struct fira_local *local, u32 session_id,
			      const struct genl_info *info)
{
	bool active;
	struct fira_session *session;

	session = fira_session_get(local, session_id, &active);
	if (!session)
		return -ENOENT;

	if (!fira_session_is_ready(local, session))
		return -EINVAL;

	if (!active) {
		u32 now_dtu;
		int initiation_time_dtu;
		int r;

		r = mcps802154_get_current_timestamp_dtu(local->llhw, &now_dtu);
		if (r)
			return r;

		initiation_time_dtu = session->params.initiation_time_ms *
				      (local->llhw->dtu_freq_hz / 1000);
		session->block_start_dtu = now_dtu + initiation_time_dtu;
		session->block_index = 0;
		session->sts_index = 0;
		session->round_index = 0;
		session->next_round_index = 0;
		list_move(&session->entry, &local->active_sessions);

		mcps802154_reschedule(local->llhw);
	}

	session->event_portid = info->snd_portid;

	return 0;
}

/**
 * fira_session_stop() - Stop Fira session.
 * @local: FiRa context.
 * @session_id: Fira session id.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int fira_session_stop(struct fira_local *local, u32 session_id,
			     const struct genl_info *info)
{
	bool active;
	struct fira_session *session;

	session = fira_session_get(local, session_id, &active);
	if (!session)
		return -ENOENT;

	if (active) {
		/* TODO: Stop fira session. */
		list_move(&session->entry, &local->inactive_sessions);
	}
	return 0;
}

/**
 * fira_session_deinit() - Deinitialize Fira session.
 * @local: FiRa context.
 * @session_id: Fira session id.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int fira_session_deinit(struct fira_local *local, u32 session_id,
			       const struct genl_info *info)
{
	bool active;
	struct fira_session *session;

	session = fira_session_get(local, session_id, &active);
	if (!session)
		return -ENOENT;
	if (active)
		return -EBUSY;

	fira_session_free(local, session);
	return 0;
}

/**
 * fira_session_set_parameters() - Set Fira session parameters.
 * @local: FiRa context.
 * @session_id: Fira session id.
 * @params: Nested attribute containing session parameters.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int fira_session_set_parameters(struct fira_local *local, u32 session_id,
				       const struct nlattr *params,
				       const struct genl_info *info)
{
	struct nlattr *attrs[FIRA_SESSION_PARAM_ATTR_MAX + 1];
	bool active;
	struct fira_session *session;
	int r;

	session = fira_session_get(local, session_id, &active);
	if (!session)
		return -ENOENT;
	if (active)
		return -EBUSY;

	if (!params)
		return -EINVAL;

	r = nla_parse_nested(attrs, FIRA_SESSION_PARAM_ATTR_MAX, params,
			     fira_session_param_nla_policy, info->extack);
	if (r)
		return r;

#define P(attr, member, type, conv)                                     \
	do {                                                            \
		int x;                                                  \
		if (attrs[FIRA_SESSION_PARAM_ATTR_##attr]) {            \
			x = nla_get_##type(                             \
				attrs[FIRA_SESSION_PARAM_ATTR_##attr]); \
			session->params.member = conv;                  \
		}                                                       \
	} while (0)
	P(DEVICE_TYPE, device_type, u8, x);
	P(DESTINATION_SHORT_ADDR, controller_short_addr, u16, x);
	P(INITIATION_TIME_MS, initiation_time_ms, u32, x);
	P(SLOT_DURATION_RSTU, slot_duration_dtu, u32,
	  x * local->llhw->rstu_dtu);
	P(BLOCK_DURATION_MS, block_duration_dtu, u32,
	  x * (local->llhw->dtu_freq_hz / 1000));
	P(ROUND_DURATION_SLOTS, round_duration_slots, u32, x);
	P(PRIORITY, priority, u8, x);
	/* TODO: set all fira session parameters. */
#undef P

	return 0;
}

/**
 * fira_manage_controlees() - Manage controlees.
 * @local: FiRa context.
 * @call_id: Fira call id.
 * @session_id: Fira session id.
 * @params: Nested attribute containing controlee parameters.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int fira_manage_controlees(struct fira_local *local, u32 call_id,
				  u32 session_id, const struct nlattr *params,
				  const struct genl_info *info)
{
	static const struct nla_policy new_controlee_nla_policy[FIRA_CALL_CONTROLEE_ATTR_MAX +
								1] = {
		[FIRA_CALL_CONTROLEE_ATTR_SHORT_ADDR] = { .type = NLA_U16 },
		[FIRA_CALL_CONTROLEE_ATTR_SUB_SESSION_ID] = { .type = NLA_U32 },
		[FIRA_CALL_CONTROLEE_ATTR_SUB_SESSION_KEY] = { .type = NLA_BINARY,
							       .len = FIRA_KEY_SIZE_MAX },
	};
	struct nlattr *request;
	struct fira_controlee controlees[FIRA_CONTROLEES_MAX];
	struct nlattr *attrs[FIRA_CALL_CONTROLEE_ATTR_MAX + 1];
	int r, rem, i, n_controlees = 0;
	struct fira_session *session;
	bool active;

	if (!params)
		return -EINVAL;

	nla_for_each_nested (request, params, rem) {
		if (n_controlees >= FIRA_CONTROLEES_MAX)
			return -EINVAL;

		r = nla_parse_nested(attrs, FIRA_CALL_CONTROLEE_ATTR_MAX,
				     request, new_controlee_nla_policy,
				     info->extack);
		if (r)
			return r;

		if (!attrs[FIRA_CALL_CONTROLEE_ATTR_SHORT_ADDR] ||
		    (!attrs[FIRA_CALL_CONTROLEE_ATTR_SUB_SESSION_ID] ^
		     !attrs[FIRA_CALL_CONTROLEE_ATTR_SUB_SESSION_KEY]))
			return -EINVAL;

		controlees[n_controlees].short_addr = nla_get_le16(
			attrs[FIRA_CALL_CONTROLEE_ATTR_SHORT_ADDR]);
		if (attrs[FIRA_CALL_CONTROLEE_ATTR_SUB_SESSION_ID]) {
			if (call_id == FIRA_CALL_DEL_CONTROLEE)
				return -EINVAL;
			controlees[n_controlees].sub_session = true;
			controlees[n_controlees].sub_session_id = nla_get_u32(
				attrs[FIRA_CALL_CONTROLEE_ATTR_SUB_SESSION_ID]);
			memcpy(controlees[n_controlees].sub_session_key,
			       nla_data(
				       attrs[FIRA_CALL_CONTROLEE_ATTR_SUB_SESSION_KEY]),
			       nla_len(attrs[FIRA_CALL_CONTROLEE_ATTR_SUB_SESSION_KEY]));
			controlees[n_controlees].sub_session_key_len = nla_len(
				attrs[FIRA_CALL_CONTROLEE_ATTR_SUB_SESSION_KEY]);
		} else
			controlees[n_controlees].sub_session = false;

		for (i = 0; i < n_controlees; i++) {
			if (controlees[n_controlees].short_addr ==
			    controlees[i].short_addr)
				return -EINVAL;
		}

		n_controlees++;
	}
	if (!n_controlees)
		return -EINVAL;

	session = fira_session_get(local, session_id, &active);
	if (!session)
		return -ENOENT;
	if (active)
		/* TODO: handle live update. */
		return -EBUSY;

	if (call_id == FIRA_CALL_DEL_CONTROLEE)
		return fira_session_del_controlees(local, session, controlees,
						   n_controlees);
	return fira_session_new_controlees(local, session, controlees,
					   n_controlees);
}

int fira_get_capabilities(struct fira_local *local,
			  const struct genl_info *info)
{
	/* TODO: inform netlink about capabilities. */
	return 0;
}

int fira_session_control(struct fira_local *local, u32 call_id,
			 const struct nlattr *params,
			 const struct genl_info *info)
{
	u32 session_id;
	struct nlattr *attrs[FIRA_CALL_ATTR_MAX + 1];
	int r;

	if (!params)
		return -EINVAL;
	r = nla_parse_nested(attrs, FIRA_CALL_ATTR_MAX, params,
			     fira_call_nla_policy, info->extack);

	if (r)
		return r;

	if (!attrs[FIRA_CALL_ATTR_SESSION_ID])
		return -EINVAL;

	session_id = nla_get_u32(attrs[FIRA_CALL_ATTR_SESSION_ID]);

	switch (call_id) {
	case FIRA_CALL_SESSION_INIT:
		return fira_session_init(local, session_id, info);
	case FIRA_CALL_SESSION_START:
		return fira_session_start(local, session_id, info);
	case FIRA_CALL_SESSION_STOP:
		return fira_session_stop(local, session_id, info);
	case FIRA_CALL_SESSION_DEINIT:
		return fira_session_deinit(local, session_id, info);
	case FIRA_CALL_SESSION_SET_PARAMS:
		return fira_session_set_parameters(
			local, session_id, attrs[FIRA_CALL_ATTR_SESSION_PARAMS],
			info);
	/* FIRA_CALL_NEW_CONTROLEE and FIRA_CALL_DEL_CONTROLEE. */
	default:
		return fira_manage_controlees(local, call_id, session_id,
					      attrs[FIRA_CALL_ATTR_CONTROLEES],
					      info);
	}
}
