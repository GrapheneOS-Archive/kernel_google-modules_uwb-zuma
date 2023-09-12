/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __QM35_SSCD__
#define __QM35_SSCD__

#include <linux/platform_data/sscoredump.h>
#include "qm35.h"
#include "hsspi_coredump.h"

#define QM35_COREDUMP_SEGMENTS 1

struct sscd_info {
	char *name;
	struct sscd_segment segs[QM35_COREDUMP_SEGMENTS];
	u16 seg_count;
};

struct sscd_desc {
	struct sscd_info sscd_info;
	struct sscd_platform_data sscd_pdata;
	struct platform_device sscd_dev;
};

int report_coredump(struct qm35_ctx *qm35_ctx);
int register_coredump(struct spi_device *spi, struct qm35_ctx *qm35_ctx);
void unregister_coredump(struct sscd_desc *sscd);

#endif /* __QM35_SSCD__ */
