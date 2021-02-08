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

// clang-format off
#define CHAN_PRF_PARAMS (4 * DW3000_CALIBRATION_PRF_MAX)
#define ANT_CHAN_PARAMS (CHAN_PRF_PARAMS * DW3000_CALIBRATION_CHANNEL_MAX)
#define ANT_OTHER_PARAMS (3) /* port, selector_gpio... */
#define ANTPAIR_CHAN_PARAMS (2 * DW3000_CALIBRATION_CHANNEL_MAX)
#define OTHER_PARAMS (2) /* xtal_trim & temperature_reference */

#define MAX_CALIB_KEYS ((ANTMAX * (ANT_CHAN_PARAMS + ANT_OTHER_PARAMS)) + \
			(ANTPAIR_MAX * ANTPAIR_CHAN_PARAMS) +		\
			(DW3000_CALIBRATION_CHANNEL_MAX) +		\
			OTHER_PARAMS)

#define CAL_OFFSET(m) offsetof(struct dw3000, calib_data.m)
#define CAL_SIZE(m) sizeof_field(struct dw3000, calib_data.m)
#define CAL_INFO(m) { .offset = CAL_OFFSET(m), .length = CAL_SIZE(m) }

#define OTP_OFFSET(m) offsetof(struct dw3000, otp_data.m)
#define OTP_SIZE(m) sizeof_field(struct dw3000, otp_data.m)
#define OTP_INFO(m) { .offset = OTP_OFFSET(m), .length = OTP_SIZE(m) }

#define PRF_CAL_INFO(b,x)			\
	CAL_INFO(b.prf[x].ant_delay),		\
	CAL_INFO(b.prf[x].tx_power),		\
	CAL_INFO(b.prf[x].pg_count),		\
	CAL_INFO(b.prf[x].pg_delay)

#define ANTENNA_CAL_INFO(x)			\
	PRF_CAL_INFO(ant[x].ch[0], 0),		\
	PRF_CAL_INFO(ant[x].ch[0], 1),		\
	PRF_CAL_INFO(ant[x].ch[1], 0),		\
	PRF_CAL_INFO(ant[x].ch[1], 1),		\
	CAL_INFO(ant[x].port),			\
	CAL_INFO(ant[x].selector_gpio),		\
	CAL_INFO(ant[x].selector_gpio_value)

#define ANTPAIR_CAL_INFO(x,y)					\
	CAL_INFO(antpair[ANTPAIR_IDX(x, y)].ch[0].pdoa_offset),	\
	CAL_INFO(antpair[ANTPAIR_IDX(x, y)].ch[0].pdoa_lut),	\
	CAL_INFO(antpair[ANTPAIR_IDX(x, y)].ch[1].pdoa_offset),	\
	CAL_INFO(antpair[ANTPAIR_IDX(x, y)].ch[1].pdoa_lut)

static const struct {
	unsigned int offset;
	unsigned int length;
} dw3000_calib_keys_info[MAX_CALIB_KEYS] = {
	// ant0.*
	ANTENNA_CAL_INFO(0),
	// ant1.*
	ANTENNA_CAL_INFO(1),
	// ant0.*
	ANTENNA_CAL_INFO(2),
	// ant1.*
	ANTENNA_CAL_INFO(3),
	// antX.antW.*
	ANTPAIR_CAL_INFO(0,1),
	ANTPAIR_CAL_INFO(0,2),
	ANTPAIR_CAL_INFO(0,3),
	ANTPAIR_CAL_INFO(1,2),
	ANTPAIR_CAL_INFO(1,3),
	ANTPAIR_CAL_INFO(2,3),
	// chY.*
	CAL_INFO(ch[0].pll_locking_code),
	CAL_INFO(ch[1].pll_locking_code),
	// other with defaults from OTP
	OTP_INFO(xtal_trim),
	OTP_INFO(tempP)
};

#define PRF_CAL_LABEL(a,c,p)				\
	"ant" #a ".ch" #c ".prf" #p ".ant_delay",	\
	"ant" #a ".ch" #c ".prf" #p ".tx_power",	\
	"ant" #a ".ch" #c ".prf" #p ".pg_count",	\
	"ant" #a ".ch" #c ".prf" #p ".pg_delay"

#define ANTENNA_CAL_LABEL(x)			\
	PRF_CAL_LABEL(x, 5, 16),		\
	PRF_CAL_LABEL(x, 5, 64),		\
	PRF_CAL_LABEL(x, 9, 16),		\
	PRF_CAL_LABEL(x, 9, 64),		\
	"ant" #x ".port",			\
	"ant" #x ".selector_gpio",		\
	"ant" #x ".selector_gpio_value"

#define PDOA_CAL_LABEL(a, b, c)				\
	"ant" #a ".ant" #b ".ch" #c ".pdoa_offset",	\
	"ant" #a ".ant" #b ".ch" #c ".pdoa_lut"

#define ANTPAIR_CAL_LABEL(x,y)			\
	PDOA_CAL_LABEL(x, y, 5),		\
	PDOA_CAL_LABEL(x, y, 9)

/**
 * dw3000_calib_keys - calibration parameters keys table
 */
static const char *const dw3000_calib_keys[MAX_CALIB_KEYS + 1] = {
	/* antX */
	ANTENNA_CAL_LABEL(0),
	ANTENNA_CAL_LABEL(1),
	ANTENNA_CAL_LABEL(2),
	ANTENNA_CAL_LABEL(3),
	// antX.antY.*
	ANTPAIR_CAL_LABEL(0,1),
	ANTPAIR_CAL_LABEL(0,2),
	ANTPAIR_CAL_LABEL(0,3),
	ANTPAIR_CAL_LABEL(1,2),
	ANTPAIR_CAL_LABEL(1,3),
	ANTPAIR_CAL_LABEL(2,3),
	// chY.*
	"ch5.pll_locking_code",
	"ch9.pll_locking_code",
	// other
	"xtal_trim",
	"temperature_reference",
	// NULL terminated array for caller of dw3000_calib_list_keys().
	NULL
};
// clang-format on

/**
 * dw3000_calib_parse_key - parse key and find corresponding param
 * @dw: the DW device
 * @key: pointer to NUL terminated string to retrieve param address and len
 * @param: pointer where to store the corresponding parameter address
 *
 * This function lookup the NULL terminated table @dw3000_calib_keys and
 * if specified key is found, store the corresponding address in @param and
 *
 * Return: length of corresponding parameter if found, else a -ENOENT error.
 */
int dw3000_calib_parse_key(struct dw3000 *dw, const char *key, void **param)
{
	int i;

	for (i = 0; dw3000_calib_keys[i]; i++) {
		const char *k = dw3000_calib_keys[i];

		if (strcmp(k, key) == 0) {
			/* Key found, calculate parameter address */
			*param = (void *)dw + dw3000_calib_keys_info[i].offset;
			return dw3000_calib_keys_info[i].length;
		}
	}
	return -ENOENT;
}

/**
 * dw3000_calib_list_keys - return the @dw3000_calib_keys known key table
 * @dw: the DW device
 *
 * Return: pointer to known keys table.
 */
const char *const *dw3000_calib_list_keys(struct dw3000 *dw)
{
	return dw3000_calib_keys;
}

/**
 * dw3000_calib_update_config - update running configuration
 * @dw: the DW device
 *
 * This function update the required fields in struct dw3000_txconfig according
 * the channel and PRF and the corresponding calibration values.
 *
 * Also update RX/TX RMARKER offset according calibrated antenna delay.
 *
 * Other calibration parameters aren't used yet.
 *
 * Return: zero on success, else a negative error code.
 */
int dw3000_calib_update_config(struct dw3000 *dw)
{
	struct mcps802154_llhw *llhw = dw->llhw;
	struct dw3000_config *config = &dw->config;
	struct dw3000_txconfig *txconfig = &dw->txconfig;
	int ant_rf1 = config->ant[0];
	int ant_rf2 = config->ant[1];
	int antpair = ant_rf2 > ant_rf1 ? ANTPAIR_IDX(ant_rf1, ant_rf2) :
					  ANTPAIR_IDX(ant_rf2, ant_rf1);
	struct dw3000_antenna_calib *ant_calib = &dw->calib_data.ant[ant_rf1];
	struct dw3000_antenna_pair_calib *antpair_calib =
		&dw->calib_data.antpair[antpair];
	int chanidx = config->chan == 9;
	int prfidx = config->txCode > 9;
	/* Update TX configuration */
	txconfig->power = ant_calib->ch[chanidx].prf[prfidx].tx_power ?
				  ant_calib->ch[chanidx].prf[prfidx].tx_power :
				  0xfefefefe;
	txconfig->PGdly = ant_calib->ch[chanidx].prf[prfidx].pg_delay ?
				  ant_calib->ch[chanidx].prf[prfidx].pg_delay :
				  0x34;
	txconfig->PGcount =
		ant_calib->ch[chanidx].prf[prfidx].pg_count ?
			ant_calib->ch[chanidx].prf[prfidx].pg_count :
			0;
	/* Update RMARKER offsets */
	llhw->tx_rmarker_offset_rctu =
		ant_calib->ch[chanidx].prf[prfidx].ant_delay;
	llhw->rx_rmarker_offset_rctu =
		ant_calib->ch[chanidx].prf[prfidx].ant_delay;
	/* Update PDOA offset */
	config->pdoaOffset = antpair_calib->ch[chanidx].pdoa_offset;
	return 0;
}
