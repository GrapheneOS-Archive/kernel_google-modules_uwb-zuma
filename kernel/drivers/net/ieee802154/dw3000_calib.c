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
/**
 * dw3000_calib_keys - calibration parameters keys table
 */
const char *const dw3000_calib_keys[] = {
	// ant0.ch*
	"ant0.ch5.prf16.antenna_delay",
	"ant0.ch5.prf16.tx_power",
	"ant0.ch5.prf16.pg_count",
	"ant0.ch5.prf16.pg_delay",
	"ant0.ch5.prf64.antenna_delay",
	"ant0.ch5.prf64.tx_power",
	"ant0.ch5.prf64.pg_count",
	"ant0.ch5.prf64.pg_delay",
	"ant0.ch9.prf16.antenna_delay",
	"ant0.ch9.prf16.tx_power",
	"ant0.ch9.prf16.pg_count",
	"ant0.ch9.prf16.pg_delay",
	"ant0.ch9.prf64.antenna_delay",
	"ant0.ch9.prf64.tx_power",
	"ant0.ch9.prf64.pg_count",
	"ant0.ch9.prf64.pg_delay",
	// ant0.*
	"ant0.port",
	"ant0.selector_gpio",
	"ant0.selector_gpio_value",
	// ant1.ch*
	"ant1.ch5.prf16.antenna_delay",
	"ant1.ch5.prf16.tx_power",
	"ant1.ch5.prf16.pg_count",
	"ant1.ch5.prf16.pg_delay",
	"ant1.ch5.prf64.antenna_delay",
	"ant1.ch5.prf64.tx_power",
	"ant1.ch5.prf64.pg_count",
	"ant1.ch5.prf64.pg_delay",
	"ant1.ch9.prf16.antenna_delay",
	"ant1.ch9.prf16.tx_power",
	"ant1.ch9.prf16.pg_count",
	"ant1.ch9.prf16.pg_delay",
	"ant1.ch9.prf64.antenna_delay",
	"ant1.ch9.prf64.tx_power",
	"ant1.ch9.prf64.pg_count",
	"ant1.ch9.prf64.pg_delay",
	// ant1.*
	"ant1.port",
	"ant1.selector_gpio",
	"ant1.selector_gpio_value",
	// ant0.ant1.*
	"ant0.ant1.ch5.pdoa_offset",
	"ant0.ant1.ch5.pdoa_lut",
	"ant0.ant1.ch9.pdoa_offset",
	"ant0.ant1.ch9.pdoa_lut",
	// chY.*
	"ch5.pll_locking_code",
	"ch9.pll_locking_code",
	// other
	"xtal_trim",
	"temperature_reference",
	// NULL terminated array for caller of dw3000_calib_list_keys().
	NULL
};

#define CAL_OFFSET(m) offsetof(struct dw3000, calib_data.m)
#define CAL_SIZE(m) sizeof_field(struct dw3000, calib_data.m)
#define CAL_INFO(m) { .offset = CAL_OFFSET(m), .length = CAL_SIZE(m) }

#define OTP_OFFSET(m) offsetof(struct dw3000, otp_data.m)
#define OTP_SIZE(m) sizeof_field(struct dw3000, otp_data.m)
#define OTP_INFO(m) { .offset = OTP_OFFSET(m), .length = OTP_SIZE(m) }

struct {
	unsigned int offset;
	unsigned int length;
} dw3000_calib_keys_info[] = {
	// ant0.*
	CAL_INFO(ant[0].ch[0].prf[0].ant_delay),
	CAL_INFO(ant[0].ch[0].prf[0].tx_power),
	CAL_INFO(ant[0].ch[0].prf[0].pg_count),
	CAL_INFO(ant[0].ch[0].prf[0].pg_delay),
	CAL_INFO(ant[0].ch[0].prf[1].ant_delay),
	CAL_INFO(ant[0].ch[0].prf[1].tx_power),
	CAL_INFO(ant[0].ch[0].prf[1].pg_count),
	CAL_INFO(ant[0].ch[0].prf[1].pg_delay),
	CAL_INFO(ant[0].ch[1].prf[0].ant_delay),
	CAL_INFO(ant[0].ch[1].prf[0].tx_power),
	CAL_INFO(ant[0].ch[1].prf[0].pg_count),
	CAL_INFO(ant[0].ch[1].prf[0].pg_delay),
	CAL_INFO(ant[0].ch[1].prf[1].ant_delay),
	CAL_INFO(ant[0].ch[1].prf[1].tx_power),
	CAL_INFO(ant[0].ch[1].prf[1].pg_count),
	CAL_INFO(ant[0].ch[1].prf[1].pg_delay),
	CAL_INFO(ant[0].port),
	CAL_INFO(ant[0].selector_gpio),
	CAL_INFO(ant[0].selector_gpio_value),
	// ant1.*
	CAL_INFO(ant[1].ch[0].prf[0].ant_delay),
	CAL_INFO(ant[1].ch[0].prf[0].tx_power),
	CAL_INFO(ant[1].ch[0].prf[0].pg_count),
	CAL_INFO(ant[1].ch[0].prf[0].pg_delay),
	CAL_INFO(ant[1].ch[0].prf[1].ant_delay),
	CAL_INFO(ant[1].ch[0].prf[1].tx_power),
	CAL_INFO(ant[1].ch[0].prf[1].pg_count),
	CAL_INFO(ant[1].ch[0].prf[1].pg_delay),
	CAL_INFO(ant[1].ch[1].prf[0].ant_delay),
	CAL_INFO(ant[1].ch[1].prf[0].tx_power),
	CAL_INFO(ant[1].ch[1].prf[0].pg_count),
	CAL_INFO(ant[1].ch[1].prf[0].pg_delay),
	CAL_INFO(ant[1].ch[1].prf[1].ant_delay),
	CAL_INFO(ant[1].ch[1].prf[1].tx_power),
	CAL_INFO(ant[1].ch[1].prf[1].pg_count),
	CAL_INFO(ant[1].ch[1].prf[1].pg_delay),
	CAL_INFO(ant[1].port),
	CAL_INFO(ant[1].selector_gpio),
	CAL_INFO(ant[1].selector_gpio_value),
	// antX.antW.*
	CAL_INFO(antpair[ANTPAIR_IDX(0, 1)].ch[0].pdoa_offset),
	CAL_INFO(antpair[ANTPAIR_IDX(0, 1)].ch[0].pdoa_lut),
	CAL_INFO(antpair[ANTPAIR_IDX(0, 1)].ch[1].pdoa_offset),
	CAL_INFO(antpair[ANTPAIR_IDX(0, 1)].ch[1].pdoa_lut),
	// chY.*
	CAL_INFO(ch[0].pll_locking_code),
	CAL_INFO(ch[1].pll_locking_code),
	// other with defaults from OTP
	OTP_INFO(xtal_trim),
	OTP_INFO(tempP),
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
	struct dw3000_antenna_calib *ant_calib = &dw->calib_data.ant[0];
	struct dw3000_antenna_pair_calib *antpair_calib =
		&dw->calib_data.antpair[0];
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
	llhw->rx_rmarker_offset_rctu =
		ant_calib->ch[chanidx].prf[prfidx].ant_delay;
	llhw->tx_rmarker_offset_rctu =
		ant_calib->ch[chanidx].prf[prfidx].ant_delay;
	/* Update PDOA offset */
	config->pdoaOffset = antpair_calib->ch[chanidx].pdoa_offset;
	/* TODO: add support for port/gpios, etc... */
	return 0;
}
