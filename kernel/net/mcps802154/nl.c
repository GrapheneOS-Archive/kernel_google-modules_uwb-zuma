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
 * 802.15.4 mac common part sublayer, netlink.
 */

#include <linux/rtnetlink.h>
#include <net/genetlink.h>

#include <net/mcps802154_nl.h>

#include "mcps802154_i.h"
#include "nl.h"

#define ATTR_STRING_SIZE 20
#define ATTR_STRING_POLICY                                          \
	{                                                           \
		.type = NLA_NUL_STRING, .len = ATTR_STRING_SIZE - 1 \
	}

/* Used to report ranging result, this should later be different per device. */
static u32 ranging_report_portid;

static struct genl_family mcps802154_nl_family;

static const struct nla_policy mcps802154_nl_ranging_request_policy
	[MCPS802154_RANGING_REQUEST_ATTR_MAX + 1] = {
		[MCPS802154_RANGING_REQUEST_ATTR_ID] = { .type = NLA_U32 },
		[MCPS802154_RANGING_REQUEST_ATTR_FREQUENCY_HZ] = { .type = NLA_U32 },
		[MCPS802154_RANGING_REQUEST_ATTR_PEER] = { .type = NLA_U64 },
		[MCPS802154_RANGING_REQUEST_ATTR_REMOTE_PEER] = { .type = NLA_U64 },
	};

static const struct nla_policy mcps802154_nl_policy[MCPS802154_ATTR_MAX + 1] = {
	[MCPS802154_ATTR_HW] = { .type = NLA_U32 },
	[MCPS802154_ATTR_WPAN_PHY_NAME] = ATTR_STRING_POLICY,
	[MCPS802154_ATTR_SCHEDULE_REGION_HANDLER_NAME] = ATTR_STRING_POLICY,
	[MCPS802154_ATTR_RANGING_REQUESTS] =
		NLA_POLICY_NESTED_ARRAY(mcps802154_nl_ranging_request_policy),
	[MCPS802154_ATTR_SCHEDULE_REGION_HANDLER_PARAMS] = { .type = NLA_NESTED },
};

/**
 * mcps802154_nl_send_hw() - Append device information to a netlink message.
 * @local: MCPS private data.
 * @msg: Message to write to.
 * @portid: Destination port address.
 * @seq: Message sequence number.
 * @flags: Message flags (0 or NLM_F_MULTI).
 *
 * Return: 0 or error.
 */
static int mcps802154_nl_send_hw(struct mcps802154_local *local,
				 struct sk_buff *msg, u32 portid, u32 seq,
				 int flags)
{
	void *hdr;

	hdr = genlmsg_put(msg, portid, seq, &mcps802154_nl_family, flags,
			  MCPS802154_CMD_NEW_HW);
	if (!hdr)
		return -ENOBUFS;

	if (nla_put_u32(msg, MCPS802154_ATTR_HW, local->hw_idx) ||
	    nla_put_string(msg, MCPS802154_ATTR_WPAN_PHY_NAME,
			   wpan_phy_name(local->hw->phy)))
		goto error;

	if (local->ca.schedule_region_handler &&
	    nla_put_string(msg, MCPS802154_ATTR_SCHEDULE_REGION_HANDLER_NAME,
			   local->ca.schedule_region_handler->handler->name))
		goto error;

	genlmsg_end(msg, hdr);
	return 0;
error:
	genlmsg_cancel(msg, hdr);
	return -EMSGSIZE;
}

/**
 * mcps802154_nl_get_hw() - Request information about a device.
 * @skb: Request message.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int mcps802154_nl_get_hw(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg;
	struct mcps802154_local *local = info->user_ptr[0];

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	if (mcps802154_nl_send_hw(local, msg, info->snd_portid, info->snd_seq,
				  0)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return genlmsg_reply(msg, info);
}

/**
 * mcps802154_nl_dump_hw() - Dump information on all devices.
 * @skb: Allocated message for response.
 * @cb: Netlink callbacks.
 *
 * Return: Size of response message, or error.
 */
static int mcps802154_nl_dump_hw(struct sk_buff *skb,
				 struct netlink_callback *cb)
{
	int start_idx = cb->args[0];
	int r = 0;
	struct mcps802154_local *local;

	rtnl_lock();
	local = mcps802154_get_first_by_idx(start_idx);
	if (local) {
		cb->args[0] = local->hw_idx + 1;
		r = mcps802154_nl_send_hw(local, skb,
					  NETLINK_CB(cb->skb).portid,
					  cb->nlh->nlmsg_seq, NLM_F_MULTI);
	}
	rtnl_unlock();

	return r ? r : skb->len;
}

/**
 * mcps802154_nl_set_params_region_handler() - Set region handler parameters.
 * @skb: Request message.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int mcps802154_nl_set_params_region_handler(struct sk_buff *skb,
						   struct genl_info *info)
{
	struct mcps802154_local *local = info->user_ptr[0];
	int r;
	struct nlattr *params_attr =
		info->attrs[MCPS802154_ATTR_SCHEDULE_REGION_HANDLER_PARAMS];
	struct nlattr *name_attr =
		info->attrs[MCPS802154_ATTR_SCHEDULE_REGION_HANDLER_NAME];
	char name[ATTR_STRING_SIZE];

	if (!name_attr || !params_attr)
		return -EINVAL;
	nla_strlcpy(name, name_attr, sizeof(name));

	mutex_lock(&local->fsm_lock);
	r = mcps802154_ca_set_schedule_region_handler_parameters(
		local, name, params_attr, info->extack, false);
	mutex_unlock(&local->fsm_lock);

	return r;
}

/**
 * mcps802154_nl_set_schedule_region_handler() - Set region handler used to
 * manage the schedule.
 * @skb: Request message.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int mcps802154_nl_set_schedule_region_handler(struct sk_buff *skb,
						     struct genl_info *info)
{
	struct mcps802154_local *local = info->user_ptr[0];
	struct nlattr *params_attr =
		info->attrs[MCPS802154_ATTR_SCHEDULE_REGION_HANDLER_PARAMS];
	char name[ATTR_STRING_SIZE];
	int r;

	if (!info->attrs[MCPS802154_ATTR_SCHEDULE_REGION_HANDLER_NAME])
		return -EINVAL;
	nla_strlcpy(name,
		    info->attrs[MCPS802154_ATTR_SCHEDULE_REGION_HANDLER_NAME],
		    sizeof(name));

	mutex_lock(&local->fsm_lock);
	if (local->started)
		r = -EBUSY;
	else if (!params_attr)
		r = mcps802154_ca_set_schedule_region_handler(local, name);
	else
		r = mcps802154_ca_set_schedule_region_handler_parameters(
			local, name, params_attr, info->extack, true);
	mutex_unlock(&local->fsm_lock);

	return r;
}

/**
 * mcps802154_nl_set_ranging_requests() - Set ranging requests for a device.
 * @skb: Request message.
 * @info: Request information.
 *
 * Return: 0 or error.
 */
static int mcps802154_nl_set_ranging_requests(struct sk_buff *skb,
					      struct genl_info *info)
{
	struct mcps802154_local *local = info->user_ptr[0];
	struct nlattr *request;
	struct nlattr *attrs[MCPS802154_RANGING_REQUEST_ATTR_MAX + 1];
	struct mcps802154_nl_ranging_request
		requests[MCPS802154_NL_RANGING_REQUESTS_MAX];
	unsigned int n_requests = 0;
	int r, rem;

	if (!local->ca.schedule_region_handler ||
	    !local->ca.schedule_region_handler->handler->ranging_setup)
		return -EOPNOTSUPP;

	if (!info->attrs[MCPS802154_ATTR_RANGING_REQUESTS])
		return -EINVAL;

	nla_for_each_nested (
		request, info->attrs[MCPS802154_ATTR_RANGING_REQUESTS], rem) {
		if (n_requests >= MCPS802154_NL_RANGING_REQUESTS_MAX)
			return -EINVAL;

		r = nla_parse_nested(attrs, MCPS802154_RANGING_REQUEST_ATTR_MAX,
				     request,
				     mcps802154_nl_ranging_request_policy,
				     info->extack);
		if (r)
			return r;

		if (!attrs[MCPS802154_RANGING_REQUEST_ATTR_ID] ||
		    !attrs[MCPS802154_RANGING_REQUEST_ATTR_FREQUENCY_HZ] ||
		    !attrs[MCPS802154_RANGING_REQUEST_ATTR_PEER])
			return -EINVAL;

		requests[n_requests].id =
			nla_get_s32(attrs[MCPS802154_RANGING_REQUEST_ATTR_ID]);
		requests[n_requests].frequency_hz = nla_get_s32(
			attrs[MCPS802154_RANGING_REQUEST_ATTR_FREQUENCY_HZ]);
		requests[n_requests].peer_extended_addr = nla_get_le64(
			attrs[MCPS802154_RANGING_REQUEST_ATTR_PEER]);
		requests[n_requests].remote_peer_extended_addr = 0;

		if (attrs[MCPS802154_RANGING_REQUEST_ATTR_REMOTE_PEER])
			requests[n_requests]
				.remote_peer_extended_addr = nla_get_le64(
				attrs[MCPS802154_RANGING_REQUEST_ATTR_REMOTE_PEER]);

		n_requests++;
	}

	mutex_lock(&local->fsm_lock);
	r = local->ca.schedule_region_handler->handler->ranging_setup(
		local->ca.schedule_region_handler, requests, n_requests);
	mutex_unlock(&local->fsm_lock);
	if (r)
		return r;

	/* TODO: store per device. */
	ranging_report_portid = info->snd_portid;

	return 0;
}

/**
 * mcps802154_nl_send_ranging_report() - Append ranging result to a netlink
 * message.
 * @local: MCPS private data.
 * @msg: Message to write to.
 * @portid: Destination port address.
 * @id: Ranging identifier.
 * @tof_rctu: Time of flight.
 * @local_pdoa_rad_q11: Local Phase Difference Of Arrival.
 * @remote_pdoa_rad_q11: Remote Phase Difference  Of Arrival.
 *
 * Return: 0 or error.
 */
static int mcps802154_nl_send_ranging_report(struct mcps802154_local *local,
					     struct sk_buff *msg, u32 portid,
					     int id, int tof_rctu,
					     int local_pdoa_rad_q11,
					     int remote_pdoa_rad_q11)
{
	void *hdr;
	struct nlattr *result;

	hdr = genlmsg_put(msg, ranging_report_portid, 0, &mcps802154_nl_family,
			  0, MCPS802154_CMD_RANGING_REPORT);
	if (!hdr)
		return -ENOBUFS;

	if (nla_put_u32(msg, MCPS802154_ATTR_HW, local->hw_idx))
		goto error;

	result = nla_nest_start(msg, MCPS802154_ATTR_RANGING_RESULT);
	if (!result)
		goto error;

	if (nla_put_u32(msg, MCPS802154_RANGING_RESULT_ATTR_ID, id) ||
	    nla_put_s32(msg, MCPS802154_RANGING_RESULT_ATTR_TOF_RCTU,
			tof_rctu) ||
	    nla_put_s32(msg, MCPS802154_RANGING_RESULT_ATTR_LOCAL_PDOA_RAD_Q11,
			local_pdoa_rad_q11) ||
	    nla_put_s32(msg, MCPS802154_RANGING_RESULT_ATTR_REMOTE_PDOA_RAD_Q11,
			remote_pdoa_rad_q11))
		goto error;

	nla_nest_end(msg, result);

	genlmsg_end(msg, hdr);
	return 0;
error:
	genlmsg_cancel(msg, hdr);
	return -EMSGSIZE;
}

int mcps802154_nl_ranging_report(struct mcps802154_llhw *llhw, int id,
				 int tof_rctu, int local_pdoa_rad_q11,
				 int remote_pdoa_rad_q11)
{
	struct mcps802154_local *local = llhw_to_local(llhw);
	struct sk_buff *msg;
	int r;

	if (ranging_report_portid == 0)
		return -ECONNREFUSED;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	if (mcps802154_nl_send_ranging_report(local, msg, ranging_report_portid,
					      id, tof_rctu, local_pdoa_rad_q11,
					      remote_pdoa_rad_q11)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	r = genlmsg_unicast(wpan_phy_net(local->hw->phy), msg,
			    ranging_report_portid);
	if (r == -ECONNREFUSED)
		ranging_report_portid = 0;

	return r;
}

enum mcps802154_nl_internal_flags {
	MCPS802154_NL_NEED_HW = 1,
};

/**
 * mcps802154_get_from_info() - Retrieve private data from netlink request.
 * information.
 * @info: Request information.
 *
 * Return: Found MCPS data, or error pointer.
 */
static struct mcps802154_local *mcps802154_get_from_info(struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	int hw_idx;
	struct mcps802154_local *local;

	ASSERT_RTNL();

	if (!attrs[MCPS802154_ATTR_HW])
		return ERR_PTR(-EINVAL);

	hw_idx = nla_get_u32(attrs[MCPS802154_ATTR_HW]);

	local = mcps802154_get_first_by_idx(hw_idx);
	if (!local || local->hw_idx != hw_idx)
		return ERR_PTR(-ENODEV);

	if (!net_eq(wpan_phy_net(local->hw->phy), genl_info_net(info)))
		return ERR_PTR(-ENODEV);

	return local;
}

/**
 * mcps802154_nl_pre_doit() - Called before single requests (but not dump).
 * @ops: Command to be executed ops structure.
 * @skb: Request message.
 * @info: Request information.
 *
 * Set MCPS private data in user_ptr[0] if needed, and lock RTNL to make it
 * stick.
 *
 * Return: 0 or error.
 */
static int mcps802154_nl_pre_doit(const struct genl_ops *ops,
				  struct sk_buff *skb, struct genl_info *info)
{
	struct mcps802154_local *local;

	if (ops->internal_flags & MCPS802154_NL_NEED_HW) {
		rtnl_lock();
		local = mcps802154_get_from_info(info);
		if (IS_ERR(local)) {
			rtnl_unlock();
			return PTR_ERR(local);
		}
		info->user_ptr[0] = local;
	}

	return 0;
}

/**
 * mcps802154_nl_post_doit() - Called after single requests (but not dump).
 * @ops: Command to be executed ops structure.
 * @skb: Request message.
 * @info: Request information.
 *
 * Release RTNL if needed.
 */
static void mcps802154_nl_post_doit(const struct genl_ops *ops,
				    struct sk_buff *skb, struct genl_info *info)
{
	if (ops->internal_flags & MCPS802154_NL_NEED_HW)
		rtnl_unlock();
}

static const struct genl_ops mcps802154_nl_ops[] = {
	{
		.cmd = MCPS802154_CMD_GET_HW,
		.doit = mcps802154_nl_get_hw,
		.dumpit = mcps802154_nl_dump_hw,
		.internal_flags = MCPS802154_NL_NEED_HW,
	},
	{
		.cmd = MCPS802154_CMD_SET_SCHEDULE_REGION_HANDLER,
		.doit = mcps802154_nl_set_schedule_region_handler,
		.flags = GENL_ADMIN_PERM,
		.internal_flags = MCPS802154_NL_NEED_HW,
	},
	{
		.cmd = MCPS802154_CMD_SET_RANGING_REQUESTS,
		.doit = mcps802154_nl_set_ranging_requests,
		.flags = GENL_ADMIN_PERM,
		.internal_flags = MCPS802154_NL_NEED_HW,
	},
	{
		.cmd = MCPS802154_CMD_SET_SCHEDULE_REGION_HANDLER_PARAMS,
		.doit = mcps802154_nl_set_params_region_handler,
		.flags = GENL_ADMIN_PERM,
		.internal_flags = MCPS802154_NL_NEED_HW,
	},
};

static struct genl_family mcps802154_nl_family __ro_after_init = {
	.name = MCPS802154_GENL_NAME,
	.version = 1,
	.maxattr = MCPS802154_ATTR_MAX,
	.policy = mcps802154_nl_policy,
	.netnsok = true,
	.pre_doit = mcps802154_nl_pre_doit,
	.post_doit = mcps802154_nl_post_doit,
	.ops = mcps802154_nl_ops,
	.n_ops = ARRAY_SIZE(mcps802154_nl_ops),
	.module = THIS_MODULE,
};

int __init mcps802154_nl_init(void)
{
	return genl_register_family(&mcps802154_nl_family);
}

void __exit mcps802154_nl_exit(void)
{
	genl_unregister_family(&mcps802154_nl_family);
}
