/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Sophgo Inc.
 *
 */

#include <sbi/sbi_error.h>
#include <sbi/sbi_clk.h>
#include <sbi_utils/clk/fdt_clk.h>

static const struct sbi_clk_device *clk_dev = NULL;

const struct sbi_clk_device *sbi_clk_get_device(const void *fdt)
{
	if (!clk_dev)
		fdt_clk_init(fdt);

	return clk_dev ? clk_dev : NULL;
}

void sbi_clk_set_device(const struct sbi_clk_device *dev)
{
	if (!dev || clk_dev)
		return;

	clk_dev = dev;
}

int sbi_clk_set_rate(const char *name, uint64_t rate)
{
	if (!clk_dev || !clk_dev->clk_set_rate)
		return SBI_EFAIL;

	clk_dev->clk_set_rate(name, rate);

	return 0;
}

uint64_t sbi_clk_get_rate(const char *name)
{
	if (!clk_dev || !clk_dev->clk_get_rate)
		return SBI_EFAIL;

	return clk_dev->clk_get_rate(name);
}

int sbi_clk_enable(const char *name)
{
	if (!clk_dev || !clk_dev->clk_enable)
		return SBI_EFAIL;

	return clk_dev->clk_enable(name);
}

int sbi_clk_disable(const char *name)
{
	if (!clk_dev || !clk_dev->clk_disable)
		return SBI_EFAIL;

	return clk_dev->clk_disable(name);
}
