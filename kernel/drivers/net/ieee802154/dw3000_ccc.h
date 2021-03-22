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
 */
#ifndef __DW3000_CCC_H
#define __DW3000_CCC_H

/* Scratch memory */
#define DW3000_CCC_SCRATCH_AP_OFFSET (0U)
#define DW3000_CCC_SCRATCH_AP_SIZE (64U)
#define DW3000_CCC_SCRATCH_NFCC_OFFSET (64U)
#define DW3000_CCC_SCRATCH_NFCC_SIZE (63U)
#define DW3000_CCC_MSGSIZE (DW3000_CCC_SCRATCH_AP_SIZE + 1) /* for tlv len */

int dw3000_ccc_process_received_msg(struct dw3000 *dw, u8 *buffer);
int dw3000_ccc_start(struct dw3000 *dw, u32 session_time0, u32 start, u32 end);

#endif /* __DW3000_CCC_H */
