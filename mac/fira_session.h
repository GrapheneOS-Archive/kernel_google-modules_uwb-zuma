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

#ifndef NET_MCPS802154_FIRA_SESSION_H
#define NET_MCPS802154_FIRA_SESSION_H

#include "fira_region.h"

struct fira_session_params {
	/* Main parameters. */
	enum fira_device_type device_type;
	__le16 controller_short_addr;
	/* Timings parameters. */
	int initiation_time_ms;
	int slot_duration_dtu;
	int block_duration_dtu;
	int round_duration_slots;
	/* Behaviour parameters. */
	int priority;
	/* List of controlees. */
	struct fira_controlee controlees[FIRA_CONTROLEES_MAX];
	size_t n_controlees;
};

/**
 * struct fira_session - Session information.
 */
struct fira_session {
	/**
	 * @id: Session identifier.
	 */
	u32 id;
	/**
	 * @entry: Entry in list of sessions.
	 */
	struct list_head entry;
	/**
	 * @params: Session parameters, mostly read only while the session is
	 * active.
	 */
	struct fira_session_params params;
	/**
	 * @block_start_dtu: Timestamp of the current or previous block. All
	 * other fields are referring to this same block.
	 */
	u32 block_start_dtu;
	/**
	 * @block_index: Block index of the reference block.
	 */
	u32 block_index;
	/**
	 * @sts_index: STS index value at reference block start.
	 */
	u32 sts_index;
	/**
	 * @round_index: Round index of the reference block.
	 */
	int round_index;
	/**
	 * @next_round_index: Round index of the block after the reference block.
	 */
	int next_round_index;
	/**
	 * @event_portid: port identifier to use for notifications.
	 */
	u32 event_portid;
};

/**
 * fira_session_new() - Create a new session.
 * @local: FiRa context.
 * @session_id: Session identifier, must be unique.
 *
 * Return: The new session or NULL on error.
 */
struct fira_session *fira_session_new(struct fira_local *local, u32 session_id);

/**
 * fira_session_free() - Remove a session.
 * @local: FiRa context.
 * @session: Session to remove, must be inactive.
 */
void fira_session_free(struct fira_local *local, struct fira_session *session);

/**
 * fira_session_get() - Get a session by its identifier.
 * @local: FiRa context.
 * @session_id: Session identifier.
 * @active: When session is found set to true if active, false if inactive.
 *
 * Return: The session or NULL if not found.
 */
struct fira_session *fira_session_get(struct fira_local *local, u32 session_id,
				      bool *active);

/**
 * fira_session_new_controlees() - Add new controlees.
 * @local: FiRa context.
 * @session: Session.
 * @controlees: Controlees information.
 * @n_controlees: Number of controlees.
 *
 * Return: 0 or error.
 */
int fira_session_new_controlees(struct fira_local *local,
				struct fira_session *session,
				const struct fira_controlee *controlees,
				size_t n_controlees);

/**
 * fira_session_new_controlees() - Remove controlees.
 * @local: FiRa context.
 * @session: Session.
 * @controlees: Controlees information.
 * @n_controlees: Number of controlees.
 *
 * Return: 0 or error.
 */
int fira_session_del_controlees(struct fira_local *local,
				struct fira_session *session,
				const struct fira_controlee *controlees,
				size_t n_controlees);

/**
 * fira_session_is_ready() - Test whether a session is ready to be started.
 * @local: FiRa context.
 * @session: Session to test.
 *
 * Return: true if the session can be started.
 */
bool fira_session_is_ready(struct fira_local *local,
			   struct fira_session *session);

/**
 * fira_session_next() - Find the next session to use after the given timestamp.
 * @local: FiRa context.
 * @next_timestamp_dtu: Next access opportunity.
 *
 * Return: The session or NULL if none.
 */
struct fira_session *fira_session_next(struct fira_local *local,
				       u32 next_timestamp_dtu);

#endif /* NET_MCPS802154_FIRA_SESSION_H */
