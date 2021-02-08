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
#ifndef __DW3000_CHIP_H
#define __DW3000_CHIP_H

/* Forward declaration */
struct dw3000;

/**
 * struct dw3000_chip_ops - version dependent chip operations
 * @softreset: soft-reset
 * @init: initialisation
 * @coex_init: initialise WiFi coexistence GPIO
 * @coex_gpio: change state of WiFi coexistence GPIO
 * @prog_ldo_and_bias_tune: programs the device's LDO and BIAS tuning
 * @get_config_mrxlut_chan: Lookup table default values for channel 5 or 9
 * @pre_read_sys_time: Workaround before the SYS_TIME register reads
 */
struct dw3000_chip_ops {
	int (*softreset)(struct dw3000 *dw);
	int (*init)(struct dw3000 *dw);
	int (*coex_init)(struct dw3000 *dw);
	int (*coex_gpio)(struct dw3000 *dw, bool state, int delay_us);
	int (*prog_ldo_and_bias_tune)(struct dw3000 *dw);
	const u32 *(*get_config_mrxlut_chan)(struct dw3000 *dw, u8 channel);
	int (*pre_read_sys_time)(struct dw3000 *dw);
};

/**
 * dw3000_chip_version - supported chip version definition
 * @id: device model ID
 * @version: device version, saved to __dw3000_chip_version
 * @ops: associated version specific operations
 */
struct dw3000_chip_version {
	unsigned id;
	int ver;
	const struct dw3000_chip_ops *ops;
};

/* DW3000 device model IDs (with or non PDOA) */
#define DW3000_C0_DEV_ID 0xdeca0302
#define DW3000_C0_PDOA_DEV_ID 0xdeca0312
#define DW3000_D0_DEV_ID 0xdeca0303
#define DW3000_D0_PDOA_DEV_ID 0xdeca0313
#define DW3000_E0_PDOA_DEV_ID 0xdeca0314

/* Declaration of version specific chip operations */
extern const struct dw3000_chip_ops dw3000_chip_c0_ops;
extern const struct dw3000_chip_ops dw3000_chip_d0_ops;
extern const struct dw3000_chip_ops dw3000_chip_e0_ops;

/* TODO: Hardarwe Timer enum/struct and functions are E0 specific
 * and will need to be moved as static in chip_e0.c as far as
 * dw3000_e0_coex_init and dw3000_e0_coex_gpio will use them.
 */
enum dw3000_timers_e { DW3000_TIMER0 = 0, DW3000_TIMER1 };

enum dw3000_timer_mode_e { DWT_TIM_SINGLE = 0, DWT_TIM_REPEAT };

enum dw3000_timer_period_e {
	/* 38.4 MHz */
	DWT_XTAL = 0,
	/* 19.2 MHz */
	DWT_XTAL_DIV2 = 1,
	/* 9.6 MHz */
	DWT_XTAL_DIV4 = 2,
	/* 4.8 MHz */
	DWT_XTAL_DIV8 = 3,
	/* 2.4 MHz */
	DWT_XTAL_DIV16 = 4,
	/* 1.2 MHz */
	DWT_XTAL_DIV32 = 5,
	/* 0.6 MHz */
	DWT_XTAL_DIV64 = 6,
	/* 0.3 MHz */
	DWT_XTAL_DIV128 = 7
};

struct dw3000_timer_cfg_t {
	/* Select the timer to use */
	enum dw3000_timers_e timer;
	/* Select the timer frequency (divider) */
	enum dw3000_timer_period_e timer_div;
	/* Select the timer mode */
	enum dw3000_timer_mode_e timer_mode;
	/* Set to '1' to halt GPIO on interrupt */
	u8 timer_gpio_stop;
	/* Configure GPIO for WiFi co-ex */
	u8 timer_coexout;
};

/* Hardware timer functions */
int dw3000_timers_reset(struct dw3000 *dw);
u16 dw3000_timers_read_and_clear_events(struct dw3000 *dw);
int dw3000_configure_timer(struct dw3000 *dw,
			   struct dw3000_timer_cfg_t *tim_cfg);
int dw3000_set_timer_expiration(struct dw3000 *dw,
				enum dw3000_timers_e timer_name, u32 exp);
int dw3000_timer_enable(struct dw3000 *dw, enum dw3000_timers_e timer_name);

#endif /* __DW3000_CHIP_H */
