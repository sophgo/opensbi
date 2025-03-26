/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Sophgo Inc.
 *
 */

#ifndef __SBI_CLK_H__
#define __SBI_CLK_H__

#include <sbi/sbi_types.h>

/* clock device */
struct sbi_clk_device {
	/* name of the clock device */
	char name[32];

	/* set clock rate */
	int (*clk_set_rate)(const char *name, uint64_t rate);

	/* returns clock rate */
	uint64_t (*clk_get_rate)(const char *name);

	/* enable clock */
	int (*clk_enable)(const char *name);

	/* disable clock */
	int (*clk_disable)(const char *name);
};

int sbi_clk_set_rate(const char *name, uint64_t rate);
uint64_t sbi_clk_get_rate(const char *name);
int sbi_clk_enable(const char *name);
int sbi_clk_disable(const char *name);

const struct sbi_clk_device *sbi_clk_get_device(const void *fdt);
void sbi_clk_set_device(const struct sbi_clk_device *dev);

#endif
