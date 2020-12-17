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
 */
struct dw3000_chip_ops {
	int (*softreset)(struct dw3000 *dw);
	int (*init)(struct dw3000 *dw);
	int (*coex_init)(struct dw3000 *dw);
	int (*coex_gpio)(struct dw3000 *dw, bool state, int delay_us);
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

/* Declaration of version specific chip operations */
extern const struct dw3000_chip_ops dw3000_chip_c0_ops;
extern const struct dw3000_chip_ops dw3000_chip_d0_ops;

#endif /* __DW3000_CHIP_H */
