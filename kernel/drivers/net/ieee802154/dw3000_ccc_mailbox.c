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
#include "dw3000_ccc.h"

int dw3000_scratch_ram_read_data(struct dw3000 *dw, u8 *buffer, u16 len,
				 u16 offset)
{
	if (unlikely(!dw->nfcc_mode))
		return -EOPNOTSUPP;
	if (unlikely((len + offset) > DW3000_SCRATCH_RAM_LEN)) {
		dev_err(dw->dev, "Scratch ram bad address\n");
		return -EINVAL;
	}
	return dw3000_xfer(dw, DW3000_SCRATCH_RAM_ID, offset, len, buffer,
			   DW3000_SPI_RD_BIT);
}

static int dw3000_scratch_ram_write_data(struct dw3000 *dw, u8 *buffer, u16 len,
					 u16 offset)
{
	if (unlikely(!dw->nfcc_mode))
		return -EOPNOTSUPP;
	if (unlikely((len + offset) > DW3000_SCRATCH_RAM_LEN)) {
		dev_err(dw->dev, "Scratch ram bad address\n");
		return -EINVAL;
	}
	return dw3000_xfer(dw, DW3000_SCRATCH_RAM_ID, offset, len, buffer,
			   DW3000_SPI_WR_BIT);
}

static int dw3000_is_spi1_reserved(struct dw3000 *dw, bool *val)
{
	u8 reg;
	int rc;

	if (unlikely(!dw->nfcc_mode))
		return -EOPNOTSUPP;
	/* Check if SPI1 is reserved by reading SPI_SEM register */
	rc = dw3000_reg_read8(dw, DW3000_SPI_SEM_ID, 0, &reg);
	if (rc)
		return rc;
	if (reg & DW3000_SPI_SEM_SPI1_RG_BIT_MASK)
		*val = true;
	else
		*val = false;
	return 0;
}

static int dw3000_spi1_release(struct dw3000 *dw)
{
	int rc;
	bool is_spi1_enabled;

	if (unlikely(!dw->nfcc_mode))
		return -EOPNOTSUPP;
	rc = dw3000_write_fastcmd(dw, DW3000_CMD_SEMA_REL);
	if (rc)
		return rc;
	rc = dw3000_is_spi1_reserved(dw, &is_spi1_enabled);
	if (rc)
		return rc;
	if (is_spi1_enabled)
		rc = -EBUSY;
	return rc;
}

static int dw3000_spi1_reserve(struct dw3000 *dw)
{
	int rc;
	bool is_spi1_reserved;

	if (unlikely(!dw->nfcc_mode))
		return -EOPNOTSUPP;
	rc = dw3000_write_fastcmd(dw, DW3000_CMD_SEMA_REQ);
	if (rc)
		return rc;
	/* Check if the SPI1 is really reserved.
	 * Indeed, if SPI2 is already reserved, SPI1 could not be reserved.
	 */
	rc = dw3000_is_spi1_reserved(dw, &is_spi1_reserved);
	if (rc)
		return rc;
	if (!is_spi1_reserved)
		rc = -EBUSY;
	return rc;
}

static int dw3000_clear_SPI1MAVAIL_interrupt(struct dw3000 *dw)
{
	if (unlikely(__dw3000_chip_version == 0))
		return -EOPNOTSUPP;
	return dw3000_clear_dss_status(dw, DW3000_DSS_STAT_SPI1_AVAIL_BIT_MASK);
}

static int dw3000_clear_SPI2MAVAIL_interrupt(struct dw3000 *dw)
{
	if (unlikely(__dw3000_chip_version == 0))
		return -EOPNOTSUPP;
	return dw3000_clear_dss_status(dw, DW3000_DSS_STAT_SPI2_AVAIL_BIT_MASK);
}

static int dw3000_is_SPIxMAVAIL_interrupts_enabled(struct dw3000 *dw, bool *val)
{
	int rc;
	u8 reg_sem;
	u32 reg_sys;

	if (unlikely(__dw3000_chip_version == 0))
		return -EOPNOTSUPP;
	rc = dw3000_reg_read8(dw, DW3000_SPI_SEM_ID, 1, &reg_sem);
	if (rc)
		return rc;
	rc = dw3000_reg_read32(dw, DW3000_SYS_CFG_ID, 0, &reg_sys);
	if (rc)
		return rc;
	if ((reg_sem & (DW3000_SPI_SEM_SPI1MAVAIL_BIT_MASK >> 8)) &&
	    (reg_sem & (DW3000_SPI_SEM_SPI2MAVAIL_BIT_MASK >> 8)) &&
	    (reg_sys & DW3000_SYS_CFG_DS_IE2_BIT_MASK))
		*val = true;
	else
		*val = false;
	return 0;
}

static int dw3000_SPIxMAVAIL_interrupts_enable(struct dw3000 *dw)
{
	int rc;
	u8 reg;

	if (unlikely(__dw3000_chip_version == 0))
		return -EOPNOTSUPP;
	rc = dw3000_clear_SPI1MAVAIL_interrupt(dw);
	if (rc)
		return rc;
	rc = dw3000_clear_SPI2MAVAIL_interrupt(dw);
	if (rc)
		return rc;
	/* Disable SPIRDY in SYS_MASK. If it is enabled, the IRQ2 will not work.
	 * It is an undocumented feature.
	 */
	rc = dw3000_reg_modify32(
		dw, DW3000_SYS_ENABLE_LO_ID, 0,
		(u32)~DW3000_SYS_ENABLE_LO_SPIRDY_ENABLE_BIT_MASK, 0);
	if (rc)
		return rc;
	/* Enable the dual SPI interrupt for SPI */
	rc = dw3000_reg_modify32(dw, DW3000_SYS_CFG_ID, 0, U32_MAX,
				 (u32)DW3000_SYS_CFG_DS_IE2_BIT_MASK);
	if (rc)
		return rc;
	/* The masked write transactions do not work on the SPI_SEM register.
	 * So, a read, modify, write sequence is mandatory on this register.
	 *
	 * The 16 bits SPI_SEM register can be accessed as two 8 bits registers.
	 * So, only read the upper 8 bits for performance.
	 */
	rc = dw3000_reg_read8(dw, DW3000_SPI_SEM_ID, 1, &reg);
	if (rc)
		return rc;
	/* Set SPI1MAVAIL and SPI2MAVAIL masks */
	reg = reg | (DW3000_SPI_SEM_SPI1MAVAIL_BIT_MASK >> 8) |
	      (DW3000_SPI_SEM_SPI2MAVAIL_BIT_MASK >> 8);
	return dw3000_reg_write8(dw, DW3000_SPI_SEM_ID, 1, reg);
}

static int dw3000_SPIxMAVAIL_interrupts_disable(struct dw3000 *dw)
{
	int rc;
	u8 reg;

	if (unlikely(!dw->nfcc_mode))
		return -EOPNOTSUPP;
	/* Please read the comment in dw3000_SPIxMAVAIL_interrupts_enable() for SPI_SEM access */
	rc = dw3000_reg_read8(dw, DW3000_SPI_SEM_ID, 1, &reg);
	if (rc)
		return rc;
	/* Reset SPI1MAVAIL and SPI2MAVAIL masks */
	reg = reg & ~(DW3000_SPI_SEM_SPI1MAVAIL_BIT_MASK >> 8) &
	      ~(DW3000_SPI_SEM_SPI2MAVAIL_BIT_MASK >> 8);
	rc = dw3000_reg_write8(dw, DW3000_SPI_SEM_ID, 1, reg);
	if (rc)
		return rc;
	/* Disable the dual SPI interrupt for SPI */
	return dw3000_reg_modify32(dw, DW3000_SYS_CFG_ID, 0,
				   (u32)~DW3000_SYS_CFG_DS_IE2_BIT_MASK, 0);
}

int dw3000_ccc_write(struct dw3000 *dw, u8 *buffer, u16 len)
{
	int rc;

	if (unlikely(!dw->nfcc_mode))
		return -EOPNOTSUPP;
	if (unlikely((len) > DW3000_CCC_SCRATCH_AP_SIZE)) {
		dev_err(dw->dev, "Writing to NFCC should not exceed %u bytes\n",
			DW3000_CCC_SCRATCH_AP_SIZE);
		return -EINVAL;
	}
	rc = dw3000_spi1_reserve(dw);
	if (rc)
		return rc;
	rc = dw3000_scratch_ram_write_data(dw, buffer, len,
					   DW3000_CCC_SCRATCH_AP_OFFSET);
	if (rc)
		return rc;
	/* Trigger IRQ2 to inform NFCC */
	return dw3000_spi1_release(dw);
}

int dw3000_ccc_read(struct dw3000 *dw, u8 *buffer, u16 len)
{
	if (unlikely(!dw->nfcc_mode))
		return -EOPNOTSUPP;
	if (unlikely((len) > DW3000_CCC_SCRATCH_NFCC_SIZE)) {
		dev_err(dw->dev,
			"Reading from NFCC should not exceed %u bytes\n",
			DW3000_CCC_SCRATCH_NFCC_SIZE);
		return -EINVAL;
	}
	return dw3000_scratch_ram_read_data(dw, buffer, len,
					    DW3000_CCC_SCRATCH_NFCC_OFFSET);
}

int dw3000_isr_handle_spi1_avail(struct dw3000 *dw)
{
	int rc;
	u8 buffer[DW3000_CCC_MSGSIZE];

	if (unlikely(!dw->nfcc_mode))
		return -EOPNOTSUPP;

	rc = dw3000_ccc_read(dw, buffer, DW3000_CCC_SCRATCH_NFCC_SIZE);
	/* clear interrupt even on error to avoid isr in loop */
	rc &= dw3000_clear_SPI1MAVAIL_interrupt(dw);
	if (rc)
		return rc;

	return dw3000_ccc_process_received_msg(dw, buffer);
}

int dw3000_ccc_enable(struct dw3000 *dw)
{
	int rc;

	/* NFCC need a D0 chip or above. C0 does not have 2 SPI interfaces */
	if (__dw3000_chip_version == 0) {
		dev_err(dw->dev, "NFCC mode is not supported on C0 chip.\n");
		return -EOPNOTSUPP;
	}

	/* disable rx during ccc */
	rc = dw3000_rx_disable(dw);
	if (rc) {
		dev_err(dw->dev, "rx disable failed: %d\n", rc);
	}

	rc = dw3000_SPIxMAVAIL_interrupts_enable(dw);
	if (rc) {
		dev_err(dw->dev, "SPIxMAVAIL interrupts enable failed: %d\n",
			rc);
	}
	dw->nfcc_mode = true;
	return rc;
}

int dw3000_ccc_disable(struct dw3000 *dw)
{
	int rc;
	bool val = false;

	rc = dw3000_is_SPIxMAVAIL_interrupts_enabled(dw, &val);
	if (rc) {
		dev_err(dw->dev,
			"SPIxMAVAIL interrupts read enable status failed: %d\n",
			rc);
		return rc;
	}
	if (val) {
		rc = dw3000_SPIxMAVAIL_interrupts_disable(dw);
		if (rc) {
			dev_err(dw->dev,
				"SPIxMAVAIL interrupts disable failed: %d\n",
				rc);
			return rc;
		}
	}

	/* TODO: inform HAL that CCC session is complete */
	dw->nfcc_mode = false;
	return rc;
}
