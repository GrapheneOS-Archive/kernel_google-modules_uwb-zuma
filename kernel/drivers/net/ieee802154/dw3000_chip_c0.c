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
#include "dw3000_core_reg.h"

static int dw3000_c0_softreset(struct dw3000 *dw)
{
	/* Reset HIF, TX, RX and PMSC */
	return dw3000_reg_write8(dw, DW3000_SOFT_RST_ID, 0, DW3000_RESET_ALL);
}

static int dw3000_c0_init(struct dw3000 *dw)
{
	/* TODO */
	return 0;
}

static int dw3000_c0_coex_init(struct dw3000 *dw)
{
	/* TODO */
	return 0;
}

static int dw3000_c0_coex_gpio(struct dw3000 *dw, bool state, int delay_us)
{
	/* TODO */
	return 0;
}

const struct dw3000_chip_ops dw3000_chip_c0_ops = {
	.softreset = dw3000_c0_softreset,
	.init = dw3000_c0_init,
	.coex_init = dw3000_c0_coex_init,
	.coex_gpio = dw3000_c0_coex_gpio,
};
