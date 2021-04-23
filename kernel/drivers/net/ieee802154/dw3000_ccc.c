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

#define MSG_HEADER_LEN (DW3000_CCC_SIGNATURE_LEN + 3)
#define MSG_LEN(x) (MSG_HEADER_LEN + (x).tlvs_len)
#define MSG_NEXT_TLV(x) (struct ccc_tlv *)((x)->raw.tlvs + (x)->tlvs_len)

#define CCC_HANDOVER_MARGIN_MS 100
#define CCC_TEST_SLOT_DELTA_PC 15 /* percent */

struct ccc_tlv {
	u8 type;
	u8 len;
	u8 tlv[]; /* its addr points to TLV data start */
} __attribute__((packed));

struct ccc_slots {
	u8 nb_slots;
	u32 slots[TLV_MAX_NB_SLOTS][2];
} __attribute__((packed));

static void ccc_prepare_header(struct ccc_msg *msg, u8 seqnum)
{
	memcpy(msg->raw.signature, DW3000_CCC_SIGNATURE_STR,
	       DW3000_CCC_SIGNATURE_LEN);
	msg->raw.ver_id = DW3000_CCC_VER_ID;
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

static void ccc_add_tlv_single_slot(struct ccc_msg *msg, u32 start, u32 end)
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

static void ccc_add_tlv_slots(struct ccc_msg *msg,
			      const struct ccc_slots *slots)
{
	struct ccc_tlv *tlv;

	tlv = MSG_NEXT_TLV(msg);
	msg->raw.nb_tlv++;
	tlv->type = TLV_SLOT_LIST;
	tlv->len = TLV_SLOTS_LEN(slots->nb_slots);
	memcpy(&tlv->tlv, slots, sizeof(*slots));
	msg->tlvs_len += TLV_TYPELEN_LEN + tlv->len;
}

static int ccc_check_msg_format(struct ccc_msg *msg, struct device *dev)
{
	/* check signature */
	if (memcmp(msg->raw.signature, DW3000_CCC_SIGNATURE_STR,
		   DW3000_CCC_SIGNATURE_LEN)) {
		dev_err(dev,
			"CCC: signature not found while reading scratch mem");
		return -1;
	}

	/* check AP_NFCC Interface Version ID */
	if (msg->raw.ver_id != DW3000_CCC_VER_ID) {
		dev_warn(dev,
			 "CCC: Interface version mismatch : %d expecting %d\n",
			 msg->raw.ver_id, DW3000_CCC_VER_ID);
	}

	/* Read number of TLVs */
	if (msg->raw.nb_tlv > DW3000_CCC_MAX_NB_TLV) {
		dev_err(dev, "CCC: read nb_tlv = %d exceeds max = %d\n",
			msg->raw.nb_tlv, DW3000_CCC_MAX_NB_TLV);
		return -1;
	}

	return 0;
}

static int ccc_process_tlvs(struct dw3000 *dw, struct ccc_msg *msg,
			    struct ccc_data *data)
{
	int i;

	/* if no tlv, assume the session ended */
	if (msg->raw.nb_tlv == 0) {
		dev_dbg(dw->dev, "CCC: stopping coexistence\n");
		return dw3000_ccc_disable(dw);
	}

	msg->tlvs_len = 0; /* start parsing at first tlv */

	/* process tlvs */
	for (i = 0; i < msg->raw.nb_tlv; i++) {
		struct ccc_tlv *tlv;

		tlv = MSG_NEXT_TLV(msg);
		msg->tlvs_len += tlv->len;
		switch (tlv->type) {
		case TLV_SLOT_LIST:
			data->slots = (struct ccc_slots *)&tlv->tlv;
			if (data->slots->nb_slots > 0)
				data->nextslot = data->slots->slots[0][0];
			break;
		case TLV_ERROR:
			dev_err(dw->dev, "CCC: nfcc sent an error");
			/* return -1; ???? */
			break;
		default:
			dev_warn(dw->dev,
				 "CCC: ignoring unexpected TLV type\n");
			break;
		}
	}

	return 0;
}

/* Generates a series of valid, interleaved slots.
 * For instance, if given the input
 * [t_start0, t_end0], [t_start1, t_end1], [t_start2, t_end2]
 * we generate the output
 * [t_end0 + DELTA0, t_start1 - DELTA0], [t_end1 + DELTA1, t_start2 - DELTA1]
 * Where DELTAn = (( t_startn+1 - t_endn ) * margin_pc) / 100
 *
 * NB : assumes input slots are valid, i.e in->nb_slots <= TLV_MAX_NB_SLOTS
 *
 * If slot_after is passed true, append a last slot AFTER all CCC slots
 */
static int ccc_generate_test_slots(const struct ccc_slots *in,
				   struct ccc_slots *out, u32 margin_pc,
				   bool slot_after)
{
	int i;
	u32 free_slot_duration;
	u32 delta_i;

	if (in->nb_slots == 0)
		return -1;

	out->nb_slots = in->nb_slots - 1;
	for (i = 0; i < out->nb_slots; i++) {
		free_slot_duration = in->slots[i + 1][1] - in->slots[i][0];
		delta_i = (free_slot_duration * margin_pc) / 100;
		/* gen slot : [t_endi + DELTAi, t_starti+1 - DELTAi] */
		out->slots[i][0] = in->slots[i][1] + delta_i;
		out->slots[i][1] = in->slots[i + 1][0] - delta_i;
	}

	if (slot_after) {
		if (out->nb_slots == 0) {
			/* we need a value for free_slot_duration and delta_i */
			free_slot_duration = in->slots[0][1] - in->slots[0][0];
			delta_i = (free_slot_duration * margin_pc) / 100;
		}
		/* Add a last slot reusing last free_slot_duration and delta_i */
		out->nb_slots++;
		out->slots[i][0] = in->slots[i][1] + delta_i;
		out->slots[i][1] =
			in->slots[i][1] - delta_i + free_slot_duration;
	}
	return 0;
}

static int dw3000_ccc_sleep(struct dw3000 *dw, u32 delay_ms)
{
	return dw3000_go_to_deep_sleep_and_wakeup_after_ms(dw, delay_ms);
}

int dw3000_ccc_process_received_msg(struct dw3000 *dw, struct ccc_msg *in,
				    void *arg)
{
	struct ccc_data data = { 0 };
	struct ccc_msg out = { 0 };
	u32 curtime = 0;
	int rc = -1;

	if (!in)
		return -1;

	rc = ccc_check_msg_format(in, dw->dev);
	if (rc)
		return rc;

	rc = ccc_process_tlvs(dw, in, &data);
	if (rc)
		return rc;

	rc = dw3000_read_sys_time(dw, &curtime);
	if (rc)
		return rc;

	if (data.nextslot > 0)
		data.diff_ms = (data.nextslot - curtime) /
			       (dw->llhw->dtu_freq_hz / 1000);
	if (data.diff_ms > CCC_HANDOVER_MARGIN_MS) {
		/* Wait for next slot, use a margin, as it looks like we do have a
		 * lot of fluctuations on NFCC side, leading to TXLATE when trying small
		 * delay.
		 * The next message is send in dw3000_isr_handle_spi_ready() on the wake up.
		 */
		/* Save the time in DTU of the next CCC slot before deep sleep. */
		dw->deep_sleep_state.ccc_nextslot_dtu = data.nextslot;
		/* Go to deep sleep with a margin */
		rc = dw3000_ccc_sleep(dw,
				      data.diff_ms - CCC_HANDOVER_MARGIN_MS);
	} else {
		/* The delay is too short to sleep before respond to the CCC. */
		dw->ccc.seqnum++;
		ccc_prepare_header(&out, dw->ccc.seqnum);
		rc = dw3000_ccc_write(dw, out.rawbuf, MSG_LEN(out));
	}
	return rc;
}

int dw3000_ccc_testmode_process_received_msg(struct dw3000 *dw,
					     struct ccc_msg *in, void *arg)
{
	struct ccc_test_config *test_conf = (struct ccc_test_config *)arg;
	struct ccc_data *data = &test_conf->data;
	struct ccc_msg out = { 0 };
	struct ccc_slots out_slots = { 0 };
	u32 curtime = 0;
	int rc = -1;

	data->nextslot = 0;
	data->diff_ms = 0;
	data->round_count++;

	if (!in)
		return 1;

	rc = ccc_check_msg_format(in, dw->dev);
	if (rc)
		return rc;

	rc = ccc_process_tlvs(dw, in, data);
	if (rc)
		return rc;

	ccc_prepare_header(&out, dw->ccc.seqnum++);

	rc = dw3000_read_sys_time(dw, &curtime);
	if (rc)
		return rc;

	/* behavior depends on test mode */
	switch (test_conf->mode) {
	case DW3000_CCC_TEST_DIRECT:
		break;
	case DW3000_CCC_TEST_WAIT:
		/* margin_ms is typically 100ms hence msleep is used */
		msleep(test_conf->margin_ms);
		break;
	case DW3000_CCC_TEST_LATE:
		if (data->nextslot == 0)
			break;
		data->diff_ms = (data->nextslot - curtime) /
				(dw->llhw->dtu_freq_hz / 1000);
		if (test_conf->RRcount == 0 ||
		    data->round_count++ % test_conf->RRcount == 0) {
			/* margin_ms is typically 100ms and diff_ms is a
			 * multiple of 96ms, hence msleep is used */
			msleep(data->diff_ms + test_conf->margin_ms);
			break;
		}
		if (data->diff_ms > CCC_HANDOVER_MARGIN_MS) {
			/* diff_ms is a multiple of 96ms, hence msleep
				 * is used */
			msleep(data->diff_ms - CCC_HANDOVER_MARGIN_MS);
		}
		break;
	case DW3000_CCC_TEST_CONFLICT:
		if (ccc_generate_test_slots(data->slots, &out_slots,
					    CCC_TEST_SLOT_DELTA_PC, true))
			break;
		if ((test_conf->RRcount == 0 ||
		     (data->round_count++ % test_conf->RRcount == 0)) &&
		    test_conf->conflit_slot_idx < out_slots.nb_slots) {
			/* We want a conflict this round
			 * and we have enough slot, given conflit_slot_idx
			 * => Just copy the input slot values*/
			out_slots.slots[test_conf->conflit_slot_idx][0] =
				data->slots
					->slots[test_conf->conflit_slot_idx][0];
			out_slots.slots[test_conf->conflit_slot_idx][1] =
				data->slots
					->slots[test_conf->conflit_slot_idx][1];
		}
		ccc_add_tlv_slots(&out, &out_slots);
		break;
	case DW3000_CCC_TEST_SLEEP_OFFSET:
		ccc_add_tlv_u32(&out, TLV_UWBCNT_OFFS, test_conf->offset_ms);
		break;
	default:
		/* should never happend */
		dev_err(dw->dev, "Unknown test mode #%d", test_conf->mode);
		return 1;
	}

	return dw3000_ccc_write(dw, out.rawbuf, MSG_LEN(out));
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
		ccc_add_tlv_single_slot(&msg, start, end);
	}

	return dw3000_ccc_write(dw, msg.rawbuf, MSG_LEN(msg));
}

int dw3000_ccc_start(struct dw3000 *dw, u8 chan, u32 session_time0, u32 start,
		     u32 end)
{
	dw3000_ccc_enable(dw, chan, dw3000_ccc_process_received_msg, NULL);
	dev_dbg(dw->dev, "CCC: starting coexistence");
	return ccc_write_first_msg(dw, session_time0, start, end);
}

int dw3000_ccc_testmode_start(struct dw3000 *dw, struct ccc_test_config *conf)
{
	if (!conf) {
		dev_err(dw->dev,
			"CCC: error: can't start testmode with a NULL conf");
		return -EINVAL;
	}
	dw3000_ccc_enable(dw, conf->channel,
			  dw3000_ccc_testmode_process_received_msg, conf);
	dev_dbg(dw->dev, "CCC: starting TESTMODE coexistance");
	return ccc_write_first_msg(dw, conf->session_time0, conf->start,
				   conf->end);
}

int dw3000_ccc_write_msg_on_wakeup(struct dw3000 *dw)
{
	struct ccc_msg out = { 0 };
	u32 offset;

	/* Due to the CCC code offset calculation (just an unsigned addition),
	 * we should subtract the current nextslot stored in the CCC and add
	 * the default prepoll message delay.
	 */
	offset = 0 - dw->deep_sleep_state.ccc_nextslot_dtu +
		 US_TO_DTU(DW3000_CCC_DEFAULT_CCC_UWB_TIME0_US);
	dw->ccc.seqnum++;
	ccc_prepare_header(&out, dw->ccc.seqnum);
	ccc_add_tlv_u32(&out, TLV_UWBCNT_OFFS, offset);
	return dw3000_ccc_write(dw, out.rawbuf, MSG_LEN(out));
}
