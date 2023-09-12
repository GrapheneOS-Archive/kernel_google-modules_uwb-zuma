/* SPDX-License-Identifier: GPL-2.0-only */

#include "qm35-sscd.h"

#define NAME "uwb"

void release_coredump(struct device *dev)
{
}

int report_coredump(struct qm35_ctx *qm35_ctx)
{
	struct coredump_layer *layer = &qm35_ctx->coredump_layer;
	struct sscd_platform_data *sscd_pdata = &qm35_ctx->sscd->sscd_pdata;
	struct sscd_info *sscd_info = &qm35_ctx->sscd->sscd_info;

	if (!layer || !sscd_pdata || !sscd_info || !sscd_pdata->sscd_report)
		return 1;

	sscd_info->seg_count = 0;
	sscd_info->name = NAME;
	sscd_info->segs[0].addr = layer->coredump_data;
	sscd_info->segs[0].size = layer->coredump_size;
	sscd_info->seg_count = 1;

	return sscd_pdata->sscd_report(&qm35_ctx->sscd->sscd_dev,
					sscd_info->segs, sscd_info->seg_count, 0,
					"qm35 coredump");
}

int register_coredump(struct spi_device *spi, struct qm35_ctx *qm35_ctx)
{
	struct sscd_desc *sscd;
	sscd = devm_kzalloc(&spi->dev, sizeof(*sscd), GFP_KERNEL);
	if (!sscd)
		return -ENOMEM;

	sscd->sscd_dev.name = NAME;
	sscd->sscd_dev.driver_override = SSCD_NAME;
	sscd->sscd_dev.id = -1;
	sscd->sscd_dev.dev.platform_data = &sscd->sscd_pdata;
	sscd->sscd_dev.dev.release = release_coredump;

	qm35_ctx->sscd = sscd;
	return platform_device_register(&qm35_ctx->sscd->sscd_dev);
}

void unregister_coredump(struct sscd_desc *sscd)
{
	platform_device_unregister(&sscd->sscd_dev);
}
