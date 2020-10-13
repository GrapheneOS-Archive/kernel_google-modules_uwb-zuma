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
 * 802.15.4 mac common part sublayer, default data path regions.
 *
 */
#include <linux/slab.h>
#include <linux/errno.h>

#include <linux/netdevice.h>

#include <net/mcps802154_schedule.h>

#include "mcps802154_i.h"

#include "default_region.h"
#include "warn_return.h"

struct mcps802154_default_local {
	struct mcps802154_open_region_handler orh;
	struct mcps802154_llhw *llhw;
	struct mcps802154_region region;
	struct mcps802154_access access;
};

static inline struct mcps802154_default_local *
orh_to_dlocal(struct mcps802154_open_region_handler *orh)
{
	return container_of(orh, struct mcps802154_default_local, orh);
}

static inline struct mcps802154_default_local *
access_to_dlocal(struct mcps802154_access *access)
{
	return container_of(access, struct mcps802154_default_local, access);
}

static void simple_rx_frame(struct mcps802154_access *access, int frame_idx,
			    struct sk_buff *skb,
			    const struct mcps802154_rx_frame_info *info)
{
	struct mcps802154_default_local *dlocal = access_to_dlocal(access);
	struct mcps802154_local *local = llhw_to_local(dlocal->llhw);
	ieee802154_rx_irqsafe(local->hw, skb, info->lqi);
}

static struct sk_buff *simple_tx_get_frame(struct mcps802154_access *access,
					   int frame_idx)
{
	struct mcps802154_default_local *dlocal = access_to_dlocal(access);
	struct mcps802154_local *local = llhw_to_local(dlocal->llhw);
	return skb_dequeue(&local->ca.queue);
}

static void simple_tx_return(struct mcps802154_access *access, int frame_idx,
			     struct sk_buff *skb,
			     enum mcps802154_access_tx_return_reason reason)
{
	struct mcps802154_default_local *dlocal = access_to_dlocal(access);
	struct mcps802154_local *local = llhw_to_local(dlocal->llhw);
	if (reason == MCPS802154_ACCESS_TX_RETURN_REASON_FAILURE) {
		local->ca.retries++;
		if (local->ca.retries <= local->pib.mac_max_frame_retries) {
			/* Retry the frame. */
			skb_queue_head(&local->ca.queue, skb);
		} else {
			local->ca.retries = 0;
			ieee802154_wake_queue(local->hw);
			dev_kfree_skb_any(skb);
			atomic_dec(&local->ca.n_queued);
		}
	} else if (reason == MCPS802154_ACCESS_TX_RETURN_REASON_CANCEL) {
		skb_queue_head(&local->ca.queue, skb);
	} else {
		local->ca.retries = 0;
		ieee802154_xmit_complete(local->hw, skb, false);
		atomic_dec(&local->ca.n_queued);
	}
}

static void simple_access_done(struct mcps802154_access *access)
{
	/* Nothing. */
}

struct mcps802154_access_ops simple_access_ops = {
	.rx_frame = simple_rx_frame,
	.tx_get_frame = simple_tx_get_frame,
	.tx_return = simple_tx_return,
	.access_done = simple_access_done,
};

static struct mcps802154_region *
simple_alloc(struct mcps802154_open_region_handler *orh)
{
	struct mcps802154_default_local *dlocal = orh_to_dlocal(orh);
	return &dlocal->region;
}

static struct mcps802154_access *
simple_get_access(const struct mcps802154_region *region,
		  u32 next_timestamp_dtu, int next_in_region_dtu)
{
	struct mcps802154_default_local *dlocal = orh_to_dlocal(region->orh);
	struct mcps802154_local *local = llhw_to_local(dlocal->llhw);
	dlocal->access.method = skb_queue_empty(&local->ca.queue) ?
					MCPS802154_ACCESS_METHOD_IMMEDIATE_RX :
					MCPS802154_ACCESS_METHOD_IMMEDIATE_TX;
	dlocal->access.ops = &simple_access_ops;
	return &dlocal->access;
}

static void simple_free(struct mcps802154_region *region)
{
	/* Nothing. */
}

static const struct mcps802154_region_ops mcps802154_default_simple_region_ops = {
	.name = "simple",
	.alloc = simple_alloc,
	.get_access = simple_get_access,
	.free = simple_free,
};

static struct mcps802154_open_region_handler *
mcps802154_default_region_handler_open(struct mcps802154_llhw *llhw)
{
	struct mcps802154_default_local *dlocal;
	dlocal = kmalloc(sizeof(*dlocal), GFP_KERNEL);
	if (!dlocal)
		return NULL;
	dlocal->llhw = llhw;
	return &dlocal->orh;
}

static void mcps802154_default_region_handler_close(
	struct mcps802154_open_region_handler *orh)
{
	struct mcps802154_default_local *dlocal = orh_to_dlocal(orh);
	kfree(dlocal);
}

static int mcps802154_default_region_handler_update_schedule(
	struct mcps802154_open_region_handler *orh,
	const struct mcps802154_schedule_update *schedule_update,
	u32 next_timestamp_dtu)
{
	struct mcps802154_region *region;
	int r;

	r = mcps802154_schedule_set_start(
		schedule_update, schedule_update->expected_start_timestamp_dtu);
	/* Can not fail, only possible error is invalid parameters. */
	WARN_RETURN(r);

	r = mcps802154_schedule_recycle(schedule_update, 0,
					MCPS802154_DURATION_NO_CHANGE);
	/* Can not fail, only possible error is invalid parameters. */
	WARN_RETURN(r);

	region = mcps802154_schedule_add_region(schedule_update, 0, 0, 0);
	if (!region)
		return -ENOMEM;

	return 0;
}

const static struct mcps802154_region_ops
	*const mcps802154_default_regions_ops[] = {
		&mcps802154_default_simple_region_ops,
	};

static struct mcps802154_region_handler mcps802154_default_region_handler = {
	.owner = THIS_MODULE,
	.name = "default",
	.n_regions_ops = ARRAY_SIZE(mcps802154_default_regions_ops),
	.regions_ops = mcps802154_default_regions_ops,
	.open = mcps802154_default_region_handler_open,
	.close = mcps802154_default_region_handler_close,
	.update_schedule = mcps802154_default_region_handler_update_schedule,
};

int __init mcps802154_default_region_init(void)
{
	return mcps802154_region_handler_register(
		&mcps802154_default_region_handler);
}

void __exit mcps802154_default_region_exit(void)
{
	mcps802154_region_handler_unregister(
		&mcps802154_default_region_handler);
}
