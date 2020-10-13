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
 * 802.15.4 mac common part sublayer, schedule management.
 *
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "mcps802154_i.h"
#include "trace.h"

void mcps802154_schedule_clear(struct mcps802154_local *local)
{
	int i;

	if (local->ca.schedule.regions) {
		for (i = 0; i < local->ca.schedule.n_regions; i++) {
			struct mcps802154_region *region =
				local->ca.schedule.regions[i];
			region->ops->free(region);
		}
		local->ca.schedule.n_regions = 0;
		kfree(local->ca.schedule.regions);
		local->ca.schedule.regions = NULL;
	}
}

int mcps802154_schedule_update(struct mcps802154_local *local,
			       u32 next_timestamp_dtu)
{
	struct mcps802154_schedule_update_local sulocal;
	struct mcps802154_schedule_update *su = &sulocal.schedule_update;
	struct mcps802154_schedule *sched = &local->ca.schedule;
	struct mcps802154_open_region_handler *orh;
	u32 expected_start_timestamp_dtu;
	int r;

	trace_schedule_update(local, next_timestamp_dtu);

	/* If no schedule at all, set sane values. */
	if (sched->n_regions == 0) {
		sched->start_timestamp_dtu = next_timestamp_dtu;
		sched->duration_dtu = 0;
		expected_start_timestamp_dtu = next_timestamp_dtu;
	} else {
		expected_start_timestamp_dtu =
			sched->start_timestamp_dtu + sched->duration_dtu;
	}
	sched->current_index = 0;

	/* Call region handler. */
	su->expected_start_timestamp_dtu = expected_start_timestamp_dtu;
	su->start_timestamp_dtu = sched->start_timestamp_dtu;
	su->duration_dtu = sched->duration_dtu;
	su->n_regions = sched->n_regions;
	sulocal.local = local;
	orh = local->ca.schedule_region_handler;
	r = orh->handler->update_schedule(orh, su, next_timestamp_dtu);
	if (r) {
		mcps802154_schedule_clear(local);
		return r;
	}

	/* Check we have a valid schedule. */
	if (sched->n_regions == 0 ||
	    is_before_dtu(sched->start_timestamp_dtu,
			  expected_start_timestamp_dtu)) {
		mcps802154_schedule_clear(local);
		return -EOPNOTSUPP;
	}

	trace_schedule_update_done(local, sched);

	return 0;
}

int mcps802154_schedule_set_start(
	const struct mcps802154_schedule_update *schedule_update,
	u32 start_timestamp_dtu)
{
	struct mcps802154_schedule_update_local *sulocal =
		schedule_update_to_local(schedule_update);
	struct mcps802154_schedule_update *su = &sulocal->schedule_update;
	struct mcps802154_schedule *sched = &sulocal->local->ca.schedule;

	if (is_before_dtu(start_timestamp_dtu,
			  schedule_update->expected_start_timestamp_dtu))
		return -EINVAL;

	su->start_timestamp_dtu = sched->start_timestamp_dtu =
		start_timestamp_dtu;

	return 0;
}
EXPORT_SYMBOL(mcps802154_schedule_set_start);

int mcps802154_schedule_recycle(
	const struct mcps802154_schedule_update *schedule_update,
	size_t n_keeps, int last_region_duration_dtu)
{
	struct mcps802154_schedule_update_local *sulocal =
		schedule_update_to_local(schedule_update);
	struct mcps802154_schedule_update *su = &sulocal->schedule_update;
	struct mcps802154_schedule *sched = &sulocal->local->ca.schedule;
	struct mcps802154_region *last_region;
	int i;

	if (n_keeps > sched->n_regions)
		return -EINVAL;

	if (n_keeps == 0 &&
	    last_region_duration_dtu != MCPS802154_DURATION_NO_CHANGE)
		return -EINVAL;

	/* Release dropped regions. */
	for (i = n_keeps; i < sched->n_regions; i++) {
		struct mcps802154_region *region = sched->regions[i];
		region->ops->free(region);
	}
	su->n_regions = sched->n_regions = n_keeps;

	/* Update last region. */
	last_region = sched->n_regions ? sched->regions[sched->n_regions - 1] :
					 NULL;
	if (last_region_duration_dtu != MCPS802154_DURATION_NO_CHANGE) {
		last_region->duration_dtu = last_region_duration_dtu;
	}

	/* Update schedule duration. */
	if (!last_region || last_region->duration_dtu == 0) {
		su->duration_dtu = sched->duration_dtu = 0;
	} else {
		su->duration_dtu = sched->duration_dtu =
			last_region->start_dtu + last_region->duration_dtu;
	}

	return 0;
}
EXPORT_SYMBOL(mcps802154_schedule_recycle);

struct mcps802154_region *mcps802154_schedule_add_region(
	const struct mcps802154_schedule_update *schedule_update,
	size_t region_ops_idx, int start_dtu, int duration_dtu)
{
	struct mcps802154_schedule_update_local *sulocal =
		schedule_update_to_local(schedule_update);
	struct mcps802154_schedule_update *su = &sulocal->schedule_update;
	struct mcps802154_schedule *sched = &sulocal->local->ca.schedule;
	struct mcps802154_region *last_region =
		sched->n_regions ? sched->regions[sched->n_regions - 1] : NULL;
	struct mcps802154_open_region_handler *orh;
	const struct mcps802154_region_ops *region_ops;
	struct mcps802154_region *region, **newregions;

	if (start_dtu < 0 || duration_dtu < 0)
		return NULL;

	/* Can not add a region after an endless region. */
	if (last_region && last_region->duration_dtu == 0)
		return NULL;

	/* Regions can not overlap. */
	if (last_region &&
	    start_dtu < last_region->start_dtu + last_region->duration_dtu)
		return NULL;

	/* Allocate and fill region. */
	orh = sulocal->local->ca.schedule_region_handler;
	if (region_ops_idx >= orh->handler->n_regions_ops)
		return NULL;
	region_ops = orh->handler->regions_ops[region_ops_idx];
	region = region_ops->alloc(orh);
	if (!region)
		return NULL;
	region->start_dtu = start_dtu;
	region->duration_dtu = duration_dtu;
	region->ops = region_ops;
	region->orh = orh;

	/* Add to schedule. */
	newregions = krealloc(
		sched->regions,
		sizeof(sched->regions[0]) * (sched->n_regions + 1), GFP_KERNEL);
	if (!newregions)
		goto err_free_region;
	newregions[sched->n_regions] = region;
	sched->regions = newregions;
	su->n_regions = sched->n_regions = sched->n_regions + 1;

	/* Update schedule duration. */
	if (region->duration_dtu == 0) {
		su->duration_dtu = sched->duration_dtu = 0;
	} else {
		su->duration_dtu = sched->duration_dtu =
			region->start_dtu + region->duration_dtu;
	}

	return region;
err_free_region:
	region->ops->free(region);
	return NULL;
}
EXPORT_SYMBOL(mcps802154_schedule_add_region);

void mcps802154_schedule_invalidate(struct mcps802154_llhw *llhw)
{
	struct mcps802154_local *local = llhw_to_local(llhw);
	if (likely(local->started))
		mcps802154_ca_invalidate_schedule(local);
}
EXPORT_SYMBOL(mcps802154_schedule_invalidate);
