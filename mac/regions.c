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
 * 802.15.4 mac common part sublayer, handle region handlers.
 *
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include "regions.h"
#include "mcps802154_i.h"

static LIST_HEAD(registered_region_handlers);
static DEFINE_MUTEX(registered_region_handlers_lock);

int mcps802154_region_handler_register(
	struct mcps802154_region_handler *region_handler)
{
	struct mcps802154_region_handler *rh;
	int r = 0;
	int i;

	if (WARN_ON(!region_handler->owner || !region_handler->name ||
		    (region_handler->n_regions_ops &&
		     !region_handler->regions_ops) ||
		    !region_handler->open || !region_handler->close))
		return -EINVAL;

	for (i = 0; i < region_handler->n_regions_ops; i++) {
		const struct mcps802154_region_ops *region_ops =
			region_handler->regions_ops[i];
		if (WARN_ON(!region_ops || !region_ops->name ||
			    !region_ops->alloc || !region_ops->get_access ||
			    !region_ops->free))
			return -EINVAL;
	}

	mutex_lock(&registered_region_handlers_lock);

	list_for_each_entry (rh, &registered_region_handlers,
			     registered_entry) {
		if (WARN_ON(strcmp(rh->name, region_handler->name) == 0)) {
			r = -EBUSY;
			goto unlock;
		}
	}

	list_add(&region_handler->registered_entry,
		 &registered_region_handlers);

unlock:
	mutex_unlock(&registered_region_handlers_lock);

	return r;
}
EXPORT_SYMBOL(mcps802154_region_handler_register);

void mcps802154_region_handler_unregister(
	struct mcps802154_region_handler *region_handler)
{
	mutex_lock(&registered_region_handlers_lock);
	list_del(&region_handler->registered_entry);
	mutex_unlock(&registered_region_handlers_lock);
}
EXPORT_SYMBOL(mcps802154_region_handler_unregister);

struct mcps802154_open_region_handler *
mcps802154_region_handler_open(struct mcps802154_local *local, const char *name)
{
	struct mcps802154_region_handler *rh;
	struct mcps802154_open_region_handler *orh;
	bool found = false;

	mutex_lock(&registered_region_handlers_lock);

	list_for_each_entry (rh, &registered_region_handlers,
			     registered_entry) {
		if (strcmp(rh->name, name) == 0) {
			if (try_module_get(rh->owner))
				found = true;
			break;
		}
	}

	mutex_unlock(&registered_region_handlers_lock);

	if (!found)
		return NULL;

	orh = rh->open(&local->llhw);
	if (!orh) {
		module_put(rh->owner);
	} else {
		orh->handler = rh;
		list_add(&orh->open_entry, &local->open_region_handlers);
	}

	return orh;
}

void mcps802154_region_handler_close(struct mcps802154_open_region_handler *orh)
{
	struct mcps802154_region_handler *rh;

	list_del(&orh->open_entry);
	rh = orh->handler;
	rh->close(orh);
	module_put(rh->owner);
}

void mcps802154_region_handler_close_all(struct mcps802154_local *local)
{
	struct mcps802154_open_region_handler *orh, *tmp;
	list_for_each_entry_safe (orh, tmp, &local->open_region_handlers,
				  open_entry) {
		mcps802154_region_handler_close(orh);
	}
}
