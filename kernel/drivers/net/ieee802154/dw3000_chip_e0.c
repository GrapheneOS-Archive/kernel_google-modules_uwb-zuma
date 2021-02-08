/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2021 Qorvo US, Inc.
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
#include "dw3000_core_reg.h"

int dw3000_d0_softreset(struct dw3000 *dw);
int dw3000_d0_init(struct dw3000 *dw);
int dw3000_d0_coex_init(struct dw3000 *dw);
int dw3000_d0_coex_gpio(struct dw3000 *dw, bool state, int delay_us);

/* Lookup table default values for channel 5 */
static const u32 dw3000_e0_configmrxlut_ch5[DW3000_CONFIGMRXLUT_MAX] = {
	0x380FD, 0x3887d, 0x38c7d, 0x38DFD, 0x39D7d, 0x39Dfd, 0x39ffd
};

/* Lookup table default values for channel 9 */
static const u32 dw3000_e0_configmrxlut_ch9[DW3000_CONFIGMRXLUT_MAX] = {
	0x5407d, 0x5487d, 0x54c7d, 0x54d7d, 0x54dfd, 0x55bfd, 0x55dfd
};

const u32 *dw3000_e0_get_config_mrxlut_chan(struct dw3000 *dw, u8 channel)
{
	if (channel == 5)
		return dw3000_e0_configmrxlut_ch5;
	else
		return dw3000_e0_configmrxlut_ch9;
}

/**
 * dw3000_prog_ldo_and_bias_tune() - Programs the device's LDO and BIAS tuning
 * @dw: The DW device.
 *
 * Return: zero on success, else a negative error code.
 */
int dw3000_e0_prog_ldo_and_bias_tune(struct dw3000 *dw)
{
	const u16 bias_mask = DW3000_BIAS_CTRL_DIG_BIAS_DAC_ULV_BIT_MASK;
	struct dw3000_local_data *local = &dw->data;
	struct dw3000_otp_data *otp = &dw->otp_data;
	u16 bias_tune = (otp->bias_tune >> 16) & bias_mask;
	if (otp->ldo_tune_lo && otp->ldo_tune_hi && bias_tune) {
		dw3000_reg_or16(dw, DW3000_NVM_CFG_ID, 0, DW3000_LDO_BIAS_KICK);
		/* Save the kicks for the on-wake configuration */
		local->sleep_mode |= DW3000_LOADLDO | DW3000_LOADBIAS;
	}
	/* Use DGC_CFG from OTP */
	local->dgc_otp_set = otp->dgc_addr == DW3000_DGC_CFG0 ?
				     DW3000_DGC_LOAD_FROM_OTP :
				     DW3000_DGC_LOAD_FROM_SW;
	return 0;
}

/**
 * dw3000_timers_reset() - reset the timers block. It will reset both timers. It can be used to stop a timer running
 * in repeat mode.
 * @dw: the DW device
 *
 * Return: zero on success, else a negative error code.
 */
int dw3000_timers_reset(struct dw3000 *dw)
{
	return dw3000_reg_and16(dw, DW3000_SOFT_RST_ID, 0,
				(u16)(~DW3000_SOFT_RST_TIM_RST_N_BIT_MASK));
}

/**
 * dw3000_timers_read_and_clear_events() - read the timers' event counts.
 * When reading from this register the values will be reset/cleared,
 * thus the host needs to read both timers' event counts the events relating to TIMER0 are in bits [7:0] and events
 * relating to TIMER1 in bits [15:8].
 * @dw: the DW device
 *
 * Return: event counts from both timers: TIMER0 events in bits [7:0], TIMER1 events in bits [15:8]
 */
u16 dw3000_timers_read_and_clear_events(struct dw3000 *dw)
{
	u16 status;
	dw3000_reg_read16(dw, DW3000_TIMER_STATUS_ID, 0, &status);
	return status;
}

/**
 * dw3000_configure_timer() - configures selected timer (TIMER0 or TIMER1) as per configuration structure
 * @dw: the DW device
 * @tim_cfg: pointer to timer configuration structure
 *
 * Return: zero on success, else a negative error code.
 */
int dw3000_configure_timer(struct dw3000 *dw,
			   struct dw3000_timer_cfg_t *tim_cfg)
{
	/* For TIMER1 we write the configuration at offset 2 */
	return dw3000_reg_write16(
		dw, DW3000_TIMER_CTRL_ID,
		(tim_cfg->timer == DW3000_TIMER1) ? 2 : 0,
		((u32)tim_cfg->timer_div
		 << DW3000_TIMER_CTRL_TIMER_0_DIV_BIT_OFFSET) |
			((u32)tim_cfg->timer_mode
			 << DW3000_TIMER_CTRL_TIMER_0_MODE_BIT_OFFSET) |
			((u32)tim_cfg->timer_gpio_stop
			 << DW3000_TIMER_CTRL_TIMER_0_GPIO_BIT_OFFSET) |
			((u32)tim_cfg->timer_coexout
			 << DW3000_TIMER_CTRL_TIMER_0_COEXOUT_BIT_OFFSET));
}

/**
 * dw3000_set_timer_expiration() - sets timer expiration period, it is a 22-bit number
 * @dw: the DW device
 * @timer_name: specify which timer period to set: TIMER0 or TIMER1
 * @exp: expiry count - e.g. if units are XTAL/64 (1.66 us) then setting 1024 ~= 1.7 ms period
 *
 * Return: zero on success, else a negative error code.
 */
int dw3000_set_timer_expiration(struct dw3000 *dw,
				enum dw3000_timers_e timer_name, u32 exp)
{
	if (timer_name == DW3000_TIMER0) {
		return dw3000_reg_write32(
			dw, DW3000_TIMER0_CNT_SET_ID, 0,
			exp & DW3000_TIMER0_CNT_SET_TIMER_0_SET_BIT_MASK);
	} else {
		return dw3000_reg_write32(
			dw, DW3000_TIMER1_CNT_SET_ID, 0,
			exp & DW3000_TIMER1_CNT_SET_TIMER_1_SET_BIT_MASK);
	}
}

/**
 * dw3000_timer_enable() - enables the timer. In order to enable, the timer enable bit [0] for TIMER0 or [1] for TIMER1
 * needs to transition from 0->1.
 * @dw: the DW device
 * @timer_name: specifies which timer to enable
 *
 * Return: zero on success, else a negative error code.
 */
int dw3000_timer_enable(struct dw3000 *dw, enum dw3000_timers_e timer_name)
{
	u8 val = 1 << ((uint8_t)timer_name);
	int rc;
	/* Enable LDO to run the timer - needed if not in IDLE state */
	rc = dw3000_reg_or8(dw, DW3000_LDO_CTRL_ID, 0,
			    DW3000_LDO_CTRL_LDO_VDDPLL_EN_BIT_MASK);
	if (rc)
		return rc;
	/* Set to '0' */
	rc = dw3000_reg_and8(dw, DW3000_TIMER_CTRL_ID, 0, ~val);
	if (rc)
		return rc;
	/* Set to '1' */
	return dw3000_reg_or8(dw, DW3000_TIMER_CTRL_ID, 0, val);
}

const struct dw3000_chip_ops dw3000_chip_e0_ops = {
	.softreset = dw3000_d0_softreset,
	.init = dw3000_d0_init,
	/* TODO: will need update with w3000_e0_coex_init and dw3000_e0_coex_gpio
	 * using E0 specific hardware timers */
	.coex_init = dw3000_d0_coex_init,
	.coex_gpio = dw3000_d0_coex_gpio,
	.prog_ldo_and_bias_tune = dw3000_e0_prog_ldo_and_bias_tune,
	.get_config_mrxlut_chan = dw3000_e0_get_config_mrxlut_chan,
};
