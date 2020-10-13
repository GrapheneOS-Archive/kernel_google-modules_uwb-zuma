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
 * 802.15.4 mac common part sublayer, net link definitions.
 */

#ifndef MCPS802154_NL_H
#define MCPS802154_NL_H

#include <linux/types.h>

struct mcps802154_llhw;

#define MCPS802154_NL_RANGING_REQUESTS_MAX 16

struct mcps802154_nl_ranging_request {
	int id;
	int frequency_hz;
	__le64 peer_extended_addr;
	__le64 remote_peer_extended_addr;
};

/**
 * mcps802154_nl_ranging_report() - Report a ranging result, called from ranging
 * code.
 * @llhw: Low-level device pointer.
 * @id: Ranging identifier.
 * @tof_rctu: Time of flight, or INT_MIN.
 * @local_pdoa_rad_q11: Local Phase Difference Of Arrival, or INT_MIN.
 * @remote_pdoa_rad_q11: Remote Phase Difference  Of Arrival, or INT_MIN.
 *
 * If this returns -ECONNREFUSED, the receiver is not listening anymore, ranging
 * can be stopped.
 *
 * Return: 0 or error.
 */
int mcps802154_nl_ranging_report(struct mcps802154_llhw *llhw, int id,
				 int tof_rctu, int local_pdoa_rad_q11,
				 int remote_pdoa_rad_q11);

int mcps802154_nl_init(void);
void mcps802154_nl_exit(void);

#endif /* MCPS802154_NL_H */
