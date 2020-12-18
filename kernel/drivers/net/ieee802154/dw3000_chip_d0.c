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

static int dw3000_d0_softreset(struct dw3000 *dw)
{
	/* D0 require a FAST command to start soft-reset */
	return dw3000_write_fastcmd(dw, DW3000_CMD_SEMA_RESET);
}

static int dw3000_d0_init(struct dw3000 *dw)
{
	/* TODO */
	return 0;
}

static int dw3000_d0_coex_init(struct dw3000 *dw)
{
	u32 modemask;
	u16 dirmask;
	int rc;

	if (dw->coex_gpio < 0)
		return 0;
	/* Ensure selected GPIO is well configured */
	modemask = DW3000_GPIO_MODE_MSGP0_MODE_BIT_MASK
		   << (DW3000_GPIO_MODE_MSGP0_MODE_BIT_LEN * dw->coex_gpio);
	rc = dw3000_set_gpio_mode(dw, modemask, 0);
	if (rc)
		return rc;
	dirmask = DW3000_GPIO_DIR_GDP0_BIT_MASK
		  << (DW3000_GPIO_DIR_GDP0_BIT_LEN * dw->coex_gpio);
	rc = dw3000_set_gpio_dir(dw, dirmask, 0);
	if (rc)
		return rc;
	/* Test GPIO */
	dw->chip_ops->coex_gpio(dw, true, 0);
	udelay(10);
	dw->chip_ops->coex_gpio(dw, false, 0);
	return 0;
}

static int dw3000_d0_coex_gpio(struct dw3000 *dw, bool state, int delay_us)
{
	int offset;
	/* /!\ could be called first with (true, 1000), then before end of 1000
	   microseconds could be called with (false, 0), should handle this case
	   with stopping the timer if any */
	if (delay_us) {
		/* Wait to ensure GPIO is toggle on time */
		if (delay_us > 10)
			usleep_range(delay_us - 10, delay_us);
		else
			udelay(delay_us);
	}
	offset = DW3000_GPIO_OUT_GOP0_BIT_LEN * dw->coex_gpio;
	dw3000_set_gpio_out(dw, !state << offset, state << offset);
	return 0;
}

const struct dw3000_chip_ops dw3000_chip_d0_ops = {
	.softreset = dw3000_d0_softreset,
	.init = dw3000_d0_init,
	.coex_init = dw3000_d0_coex_init,
	.coex_gpio = dw3000_d0_coex_gpio,
};
