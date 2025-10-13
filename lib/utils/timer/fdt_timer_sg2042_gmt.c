/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Andes Technology Corporation
 *
 * Authors:
 *   Yu Chien Peter Lin <peterlin@andestech.com>
 */

#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/timer/fdt_timer.h>
#include <sbi_utils/timer/sg2042_gmt.h>
#include <sbi/sbi_error.h>
#include <libfdt.h>

static int fdt_sg2042gmt_cold_timer_init(void *fdt, int nodeoff,
				    const struct fdt_match *match)
{
	int err;
	unsigned long mtimer_base, mtimer_size,
		      mtimecmp_base, mtimecmp_size,
		      declared_freq, actual_freq, timecmp_freq;
	const fdt32_t *val;
	int len;

	err = fdt_get_node_addr_size(fdt, nodeoff, 0, &mtimer_base, &mtimer_size);
	if (err)
		return err;

	err = fdt_get_node_addr_size(fdt, nodeoff, 1, &mtimecmp_base, &mtimecmp_size);
	if (err)
		return err;


	err = fdt_parse_timebase_frequency(fdt, &declared_freq);
	if (err)
		return err;

	val = (fdt32_t *)fdt_getprop(fdt, nodeoff, "clock-frequency", &len);
	if (len < 8 || val == NULL)
		return SBI_EINVAL;

	actual_freq = fdt32_to_cpu(*val);
	timecmp_freq = fdt32_to_cpu(*(val + 1));

	return sg2042gmt_cold_timer_init(mtimer_base,
					 mtimecmp_base, mtimecmp_size,
					 declared_freq, actual_freq, timecmp_freq);
}

static const struct fdt_match timer_sg2042gmt_match[] = {
	{ .compatible = "sophgo,sg2042-global-mtimer" },
	{},
};

struct fdt_timer fdt_timer_sg2042_gmt = {
	.match_table = timer_sg2042gmt_match,
	.cold_init   = fdt_sg2042gmt_cold_timer_init,
	.warm_init   = sg2042gmt_warm_timer_init,
	.exit	     = NULL,
};
