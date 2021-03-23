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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>

#include "dw3000.h"
#include "dw3000_core.h"
#include "dw3000_stm.h"
#include "dw3000_mcps.h"

static int dw3000_thread_cpu;
module_param_named(cpu, dw3000_thread_cpu, uint, 0444);
MODULE_PARM_DESC(cpu, "CPU on which the DW state machine's thread will run");

static int dw3000_wifi_coex_gpio = -1;
module_param_named(wificoex_gpio, dw3000_wifi_coex_gpio, int, 0444);
MODULE_PARM_DESC(wificoex_gpio,
		 "WiFi coexistence GPIO number, -1 for disabled (default)");

static int dw3000_lna_pa_mode = 0;
module_param_named(lna_pa_mode, dw3000_lna_pa_mode, int, 0444);
MODULE_PARM_DESC(
	lna_pa_mode,
	"Configure LNA/PA mode. May conflict with WiFi coexistence GPIO number, 0 for disabled (default)");

static int dw3000_spi_probe(struct spi_device *spi)
{
	struct dw3000 *dw;
	int rc;

	/* Allocate MCPS 802.15.4 device */
	dw = dw3000_mcps_alloc(&spi->dev);
	if (!dw) {
		rc = -ENOMEM;
		goto err_alloc_hw;
	}
	dw->llhw->hw->parent = &spi->dev;
	spi_set_drvdata(spi, dw);
	dw->spi = spi;
	dw->coex_gpio = (s8)dw3000_wifi_coex_gpio;
	dw->lna_pa_mode = (s8)dw3000_lna_pa_mode;

	dev_info(dw->dev, "Loading driver...");
	dw3000_sysfs_init(dw);

	/* Setup SPI parameters */
	dev_info(dw->dev, "setup mode: %d, %u bits/w, %u Hz max\n",
		 (int)(spi->mode & (SPI_CPOL | SPI_CPHA)), spi->bits_per_word,
		 spi->max_speed_hz);
	dev_info(dw->dev, "can_dma: %d\n", spi->master->can_dma != NULL);

	spi->bits_per_word = 8;
#if (KERNEL_VERSION(5, 3, 0) <= LINUX_VERSION_CODE)
	spi->rt = 1;
#endif
	rc = spi_setup(spi);
	if (rc != 0)
		goto err_spi_setup;

	/* Request and setup the reset GPIO pin */
	rc = dw3000_setup_reset_gpio(dw);
	if (rc != 0)
		goto err_setup_gpios;

	/* Allocate pre-computed SPI messages for fast access some registers */
	rc = dw3000_transfers_init(dw);
	if (rc != 0)
		goto err_transfers_init;

	/* Initialise state descriptor */
	rc = dw3000_state_init(dw, dw3000_thread_cpu);
	if (rc != 0) {
		dev_err(dw->dev, "state machine initialisation failed: %d\n",
			rc);
		goto err_state_init;
	}

	/* Turn on power (with RST GPIO) */
	rc = dw3000_hardreset(dw);
	if (rc != 0) {
		dev_err(dw->dev, "device power on failed: %d\n", rc);
		goto err_power;
	}

	/* Request and setup the irq GPIO pin */
	rc = dw3000_setup_irq(dw);
	if (rc != 0)
		goto err_setup_gpios;

	/* Soft reset */
	rc = dw3000_softreset(dw);
	if (rc != 0) {
		dev_err(dw->dev, "device reset failed: %d\n", rc);
		goto err_reset;
	}

	/* Register MCPS 802.15.4 device */
	rc = dw3000_mcps_register(dw);
	if (rc != 0) {
		dev_err(&spi->dev, "could not register: %d\n", rc);
		goto err_register_hw;
	}

	/* Start state machine & initialise device */
	rc = dw3000_state_start(dw);
	if (rc != 0)
		goto err_state_start;

	/* All is ok */
	return 0;

err_state_start:
	dw3000_mcps_unregister(dw);
	dw3000_state_stop(dw);
err_register_hw:
err_reset:
err_power:
err_state_init:
err_transfers_init:
err_setup_gpios:
err_spi_setup:
	dw3000_sysfs_remove(dw);
	dw3000_mcps_free(dw);
err_alloc_hw:
	return rc;
}

static int dw3000_spi_remove(struct spi_device *spi)
{
	struct dw3000 *dw = spi_get_drvdata(spi);

	dw3000_sysfs_remove(dw);

	dev_dbg(dw->dev, "unloading...");

	/* Unregister subsystems */
	dw3000_mcps_unregister(dw);

	/* Stop state machine */
	dw3000_state_stop(dw);

	/* Free pre-computed SPI messages */
	dw3000_transfers_free(dw);

	/* Release the mcps 802.15.4 device */
	dw3000_mcps_free(dw);

	return 0;
}

enum { DW3000,
};

static const struct of_device_id dw3000_of_ids[] = {
	{ .compatible = "decawave,dw3000", .data = (void *)DW3000 },
	{},
};
MODULE_DEVICE_TABLE(of, dw3000_of_ids);

static const struct spi_device_id dw3000_spi_ids[] = {
	{ "dw3000", DW3000 },
	{},
};
MODULE_DEVICE_TABLE(spi, dw3000_spi_ids);

static struct spi_driver dw3000_driver = {
	.driver = {
		.name = "dw3000",
		.of_match_table = of_match_ptr(dw3000_of_ids),
	},
	.id_table = dw3000_spi_ids,
	.probe = dw3000_spi_probe,
	.remove = dw3000_spi_remove,
};
module_spi_driver(dw3000_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Thomas Venriès <tvenries@sevenhugs.com>");
MODULE_DESCRIPTION("DecaWave DW3000 IEEE 802.15.4 driver");
