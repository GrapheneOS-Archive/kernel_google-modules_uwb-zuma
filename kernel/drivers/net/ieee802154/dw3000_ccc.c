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

#include "dw3000.h"
#include "dw3000_core.h"
#include "dw3000_ccc_mailbox.h"
#include "dw3000_ccc.h"

/* Main defines */
#define CCC_VER_ID 1
#define CCC_SIGNATURE_STR "QORVO"
#define CCC_SIGNATURE_LEN 5
#define CCC_MAX_NB_TLV 12
#define CCC_MARGIN_TIME_RELEASE_ms 8

/* TLVs types */
#define TLV_SESSION_TIME0 1
#define TLV_SLOT_LIST 2
#define TLV_UWBCNT_OFFS 3
#define TLV_ERROR 4

/* TLVs len helpers */
#define TLV_TYPELEN_LEN 2 /* type 1 byte, len 1 byte */
#define TLV_U32_LEN (4 + 1) /* u32 + ack/nack */
#define TLV_SLOTS_LEN(nbslots) \
	(1 + (8 * (nbslots)) + 1) /* nslots + slots + ack/nack */
#define TLV_MAX_NB_SLOTS 4

/* Error codes for TLV_ERROR type*/
#define CCC_ERR_LATE_SPIMAVAIL 0
#define CCC_ERR_SLOT_CONFLICT 1
#define CCC_ERR_CODE_SZ 2

#define MSG_HEADER_LEN (CCC_SIGNATURE_LEN + 3)
#define MSG_LEN(x) (MSG_HEADER_LEN + (x).tlvs_len)
#define MSG_NEXT_TLV(x) (struct ccc_tlv *)((x)->raw.tlvs + (x)->tlvs_len)

#define HANDOVER_MARGIN_MS 100

struct ccc_raw_msg {
	u8 signature[CCC_SIGNATURE_LEN];
	u8 ver_id;
	u8 seqnum;
	u8 nb_tlv;
	u8 tlvs[]; /* its addr points to TLVs start */
} __attribute__((packed));

struct ccc_tlv {
	u8 type;
	u8 len;
	u8 tlv[]; /* its addr points to TLV data start */
} __attribute__((packed));

struct ccc_msg {
	union {
		u8 rawbuf[DW3000_CCC_SCRATCH_AP_SIZE];
		struct ccc_raw_msg raw;
	};
	u8 tlvs_len; /* len of TLVs in bytes */
} __attribute__((packed));

struct ccc_slots {
	u8 nb_slots;
	u32 slots[TLV_MAX_NB_SLOTS][2];
} __attribute__((packed));

static void ccc_prepare_header(struct ccc_msg *msg, u32 seqnum)
{
	memcpy(msg->raw.signature, CCC_SIGNATURE_STR, CCC_SIGNATURE_LEN);
	msg->raw.ver_id = CCC_VER_ID;
	msg->raw.seqnum = seqnum;
	msg->raw.nb_tlv = 0;
	msg->tlvs_len = 0;
}

static void ccc_add_tlv_u32(struct ccc_msg *msg, u8 type, u32 value)
{
	struct ccc_tlv *tlv;
	u32 *v;

	tlv = MSG_NEXT_TLV(msg);
	msg->raw.nb_tlv++;
	tlv->type = type;
	tlv->len = 4;
	v = (u32 *)&tlv->tlv;
	*v = value;
	msg->tlvs_len += TLV_TYPELEN_LEN + TLV_U32_LEN;
}

static void ccc_add_tlv_slots(struct ccc_msg *msg, u32 start, u32 end)
{
	struct ccc_tlv *tlv;
	struct ccc_slots *slots;

	tlv = MSG_NEXT_TLV(msg);
	msg->raw.nb_tlv++;
	tlv->type = TLV_SLOT_LIST;
	tlv->len = TLV_SLOTS_LEN(1);
	slots = (struct ccc_slots *)&tlv->tlv;
	slots->nb_slots = 1;
	slots->slots[0][0] = start;
	slots->slots[0][1] = end;
	msg->tlvs_len += TLV_TYPELEN_LEN + tlv->len;
}

int dw3000_ccc_process_received_msg(struct dw3000 *dw, char *buffer)
{
	struct ccc_msg *in;
	struct ccc_msg out;
	u8 seqnum;
	u32 curtime;
	u32 nextslot = 0;
	u32 diff_ms = 0;
	int i;
	int rc;

	in = (struct ccc_msg *)buffer;
	in->tlvs_len = 0; /* start parsing at first tlv */

	/* check signature */
	if (memcmp(in->raw.signature, CCC_SIGNATURE_STR, CCC_SIGNATURE_LEN)) {
		dev_err(dw->dev,
			"CCC: signature not found while reading scratch mem");
		return 1;
	}

	/* check AP_NFCC Interface Version ID */
	if (in->raw.ver_id != CCC_VER_ID) {
		dev_warn(dw->dev,
			 "CCC: Interface version mismatch : %d expecting %d\n",
			 in->raw.ver_id, CCC_VER_ID);
	}

	/* TODO: sequential on our side instead of getting sequence number and
	 * increase for our answer */
	seqnum = in->raw.seqnum + 1;

	/* Read number of TLVs */
	if (in->raw.nb_tlv > CCC_MAX_NB_TLV) {
		dev_err(dw->dev, "CCC: read nb_tlv = %d exceeds max = %d\n",
			in->raw.nb_tlv, CCC_MAX_NB_TLV);
		return 1;
	}

	/* if no tlv, assume the session ended */
	if (in->raw.nb_tlv == 0) {
		dev_dbg(dw->dev, "CCC: stopping coexistence\n");
		return dw3000_ccc_disable(dw);
	}

	/* process tlvs */
	for (i = 0; i < in->raw.nb_tlv; i++) {
		struct ccc_tlv *tlv;
		struct ccc_slots *slots;
		u8 s;

		tlv = MSG_NEXT_TLV(in);
		in->tlvs_len += tlv->len;
		switch (tlv->type) {
		case TLV_SLOT_LIST:
			slots = (struct ccc_slots *)&tlv->tlv;
			for (s = 0; s < slots->nb_slots; s++) {
				/* TODO use slots */
			}
			if (slots->nb_slots > 0)
				nextslot = slots->slots[0][0];
			break;
		case TLV_ERROR:
			dev_err(dw->dev, "CCC: nfcc sent an error");
			break;
		default:
			dev_err(dw->dev, "CCC: unexpected TLV type\n");
			break;
		}
	}

	/* Wait for next slot, use a margin, as it looks like we do have a
	 * lot of fluctuations on NFCC side, creating TXLATE when trying small
	 * margins, then send a message without TLV.
	 */
	rc = dw3000_read_sys_time(dw, &curtime);
	if (rc)
		return rc;
	if (nextslot > 0)
		diff_ms = (nextslot - curtime) / (dw->llhw->dtu_freq_hz / 1000);
	if (diff_ms > HANDOVER_MARGIN_MS)
		msleep(diff_ms - HANDOVER_MARGIN_MS);
	ccc_prepare_header(&out, seqnum);
	rc = dw3000_ccc_write(dw, out.rawbuf, MSG_LEN(out));
	return rc;
}

static int ccc_write_first_msg(struct dw3000 *dw, u32 session_time0, u32 start,
			       u32 end)
{
	int rc;
	u32 start_time_dtu;
	u32 absolute_session_time0_dtu;
	u64 session_time0_dtu;
	struct ccc_msg msg;

	rc = dw3000_read_sys_time(dw, &start_time_dtu);
	if (rc)
		return rc;

	ccc_prepare_header(&msg, 0);
	/* session_time0 is the offset of the NFCC ranging start time in
	 * microseconds.  The absolute NFCC ranging start time is transmitted
	 * to the NFCC in DTU.
	 */
	session_time0_dtu =
		((u64)session_time0 * (u64)(dw->llhw->dtu_freq_hz)) / 1000000;
	absolute_session_time0_dtu = start_time_dtu + session_time0_dtu;

	ccc_add_tlv_u32(&msg, TLV_SESSION_TIME0, absolute_session_time0_dtu);

	if (start && end) {
		start *= (u64)(dw->llhw->dtu_freq_hz) / 1000000;
		start += start_time_dtu;
		end *= (u64)(dw->llhw->dtu_freq_hz) / 1000000;
		end += start_time_dtu;
		ccc_add_tlv_slots(&msg, start, end);
	}

	return dw3000_ccc_write(dw, msg.rawbuf, MSG_LEN(msg));
}

int dw3000_ccc_start(struct dw3000 *dw, u32 session_time0, u32 start, u32 end)
{
	dw3000_ccc_enable(dw);
	dev_dbg(dw->dev, "CCC: starting coexistence\n");
	return ccc_write_first_msg(dw, session_time0, start, end);
}
