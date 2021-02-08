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
 * FiRa sessions management.
 *
 */

#include "fira_session.h"

#include <linux/errno.h>
#include <linux/ieee802154.h>

struct fira_session *fira_session_new(struct fira_local *local, u32 session_id)
{
	struct fira_session *session;

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return NULL;

	session->id = session_id;
	session->params.controller_short_addr = IEEE802154_ADDR_SHORT_BROADCAST;
	session->params.initiation_time_ms =
		local->llhw->anticip_dtu / (local->llhw->dtu_freq_hz / 1000);
	session->params.slot_duration_dtu =
		FIRA_SLOT_DURATION_RSTU_DEFAULT * local->llhw->rstu_dtu;
	session->params.block_duration_dtu = FIRA_BLOCK_DURATION_MS_DEFAULT *
					     (local->llhw->dtu_freq_hz / 1000);
	session->params.round_duration_slots =
		FIRA_ROUND_DURATION_SLOTS_DEFAULT;
	session->params.priority = FIRA_PRIORITY_DEFAULT;

	list_add(&session->entry, &local->inactive_sessions);

	return session;
}

void fira_session_free(struct fira_local *local, struct fira_session *session)
{
	list_del(&session->entry);
	kfree(session);
}

struct fira_session *fira_session_get(struct fira_local *local, u32 session_id,
				      bool *active)
{
	struct fira_session *session;

	list_for_each_entry (session, &local->inactive_sessions, entry) {
		if (session->id == session_id) {
			*active = false;
			return session;
		}
	}

	list_for_each_entry (session, &local->active_sessions, entry) {
		if (session->id == session_id) {
			*active = true;
			return session;
		}
	}

	return NULL;
}

int fira_session_new_controlees(struct fira_local *local,
				struct fira_session *session,
				const struct fira_controlee *controlees,
				size_t n_controlees)
{
	int i, j;

	if (session->params.n_controlees + n_controlees > FIRA_CONTROLEES_MAX)
		return -EINVAL;

	for (i = 0; i < n_controlees; i++) {
		for (j = 0; j < session->params.n_controlees; j++) {
			if (controlees[i].short_addr ==
			    session->params.controlees[j].short_addr)
				return -EINVAL;
		}
	}

	for (i = 0; i < n_controlees; i++)
		session->params.controlees[session->params.n_controlees++] =
			controlees[i];

	return 0;
}

int fira_session_del_controlees(struct fira_local *local,
				struct fira_session *session,
				const struct fira_controlee *controlees,
				size_t n_controlees)
{
	size_t ii, io, j;

	for (ii = 0, io = 0; ii < session->params.n_controlees; ii++) {
		bool remove = false;
		struct fira_controlee *c = &session->params.controlees[ii];

		for (j = 0; j < n_controlees && !remove; j++) {
			if (c->short_addr == controlees[j].short_addr)
				remove = true;
		}

		if (!remove) {
			if (io != ii)
				session->params.controlees[io] = *c;
			io++;
		}
	}
	session->params.n_controlees = io;

	return 0;
}

bool fira_session_is_ready(struct fira_local *local,
			   struct fira_session *session)
{
	int round_duration_dtu = session->params.slot_duration_dtu *
				 session->params.round_duration_slots;

	return session->params.slot_duration_dtu != 0 &&
	       session->params.block_duration_dtu != 0 &&
	       session->params.round_duration_slots != 0 &&
	       round_duration_dtu < session->params.block_duration_dtu;
}

static void fira_session_update(struct fira_local *local,
				struct fira_session *session,
				u32 next_timestamp_dtu)
{
	s32 diff_dtu = session->block_start_dtu - next_timestamp_dtu;

	if (diff_dtu < 0) {
		int block_duration_dtu = session->params.block_duration_dtu;
		int block_duration_slots =
			block_duration_dtu / session->params.slot_duration_dtu;
		int add_blocks;

		add_blocks =
			(-diff_dtu + block_duration_dtu) / block_duration_dtu;
		session->block_start_dtu += add_blocks * block_duration_dtu;
		session->block_index += add_blocks;
		session->sts_index += add_blocks * block_duration_slots;
	}
}

struct fira_session *fira_session_next(struct fira_local *local,
				       u32 next_timestamp_dtu)
{
	struct fira_session *session;

	if (list_empty(&local->active_sessions))
		return NULL;

	/* Support only one session for the moment. */
	session = list_first_entry(&local->active_sessions, struct fira_session,
				   entry);
	fira_session_update(local, session, next_timestamp_dtu);
	return session;
}
