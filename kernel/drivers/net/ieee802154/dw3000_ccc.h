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

/* Main defines */
#define DW3000_CCC_VER_ID 1
#define DW3000_CCC_SIGNATURE_STR "QORVO"
#define DW3000_CCC_SIGNATURE_LEN 5
#define DW3000_CCC_MAX_NB_TLV 12
#define DW3000_CCC_MARGIN_TIME_RELEASE_MS 8
/* This constant comes from STM32 CCC code, with this comment:
 *
 * Starting time reference (resolution 1 microsecond) for ranging.
 * Offset value relative to initial LE Connection Complete event.
 * UWB_T0 is used to set the delay between the sess_srt command and the prepoll message.
 * Following the CCC , uwb_time0 is in us (microsec) unit.
 */
#define DW3000_CCC_DEFAULT_CCC_UWB_TIME0_US 65536

/* Scratch memory */
#define DW3000_CCC_SCRATCH_AP_OFFSET (0U)
#define DW3000_CCC_SCRATCH_AP_SIZE (64U)
#define DW3000_CCC_SCRATCH_NFCC_OFFSET (64U)
#define DW3000_CCC_SCRATCH_NFCC_SIZE (63U)

struct ccc_raw_msg {
	u8 signature[DW3000_CCC_SIGNATURE_LEN];
	u8 ver_id;
	u8 seqnum;
	u8 nb_tlv;
	u8 tlvs[]; /* its addr points to TLVs start */
} __attribute__((packed));

struct ccc_msg {
	union {
		u8 rawbuf[DW3000_CCC_SCRATCH_AP_SIZE];
		struct ccc_raw_msg raw;
	};
	u8 tlvs_len; /* len of TLVs in bytes */
} __attribute__((packed));

struct ccc_data {
	/* current round data */
	u32 nextslot;
	u32 diff_ms;
	struct ccc_slots *slots;

	/* session data (persistent) */
	u32 round_count;
};

struct ccc_test_config {
	/* test parameters */
	enum { DW3000_CCC_TEST_DIRECT,
	       DW3000_CCC_TEST_WAIT,
	       DW3000_CCC_TEST_LATE,
	       DW3000_CCC_TEST_CONFLICT,
	       DW3000_CCC_TEST_SLEEP_OFFSET,
	} mode;

	u32 margin_ms;
	u32 RRcount;
	u32 conflit_slot_idx;
	u32 offset_ms;

	/* CCC channel */
	u8 channel;
	/* CCC init session_time0  */
	u32 session_time0;
	/* CCC init first slots */
	u32 start, end;
	/* test callback data */
	struct ccc_data data;
};

typedef int (*ccc_callback)(struct dw3000 *dw, struct ccc_msg *in, void *arg);
int dw3000_ccc_process_received_msg(struct dw3000 *dw, struct ccc_msg *in,
				    void *arg);
int dw3000_ccc_testmode_process_received_msg(struct dw3000 *dw,
					     struct ccc_msg *in, void *arg);

int dw3000_ccc_start(struct dw3000 *dw, u8 chan, u32 session_time0, u32 start,
		     u32 end);
int dw3000_ccc_testmode_start(struct dw3000 *dw, struct ccc_test_config *conf);

int dw3000_ccc_write_msg_on_wakeup(struct dw3000 *dw);

#endif /* __DW3000_CCC_H */
