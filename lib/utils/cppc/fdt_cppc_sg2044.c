/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Sophgo Inc.
 *
 */

#include <libfdt.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_clk.h>
#include <sbi/sbi_cppc.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi_utils/cppc/fdt_cppc.h>

#define CPPC_REGISTER_WIDTH	64
#define CPPC_REGISTER_NOT_IMPLEMENTED	0

static char clock_names[64];
static uint64_t cpu_clk_granularity;

static int sg2044_cppc_read(unsigned long reg, u64 *val)
{
	int rc = SBI_SUCCESS;

	switch (reg) {
	case SBI_CPPC_DESIRED_PERF:
		*val = sbi_clk_get_rate(clock_names) / cpu_clk_granularity;
		break;
	default:
		rc = SBI_ERR_NOT_SUPPORTED;
		break;
	}

	return rc;
}

static int sg2044_cppc_write(unsigned long reg, u64 val)
{
	int rc = SBI_SUCCESS;

	switch (reg) {
	case SBI_CPPC_DESIRED_PERF:
		rc = sbi_clk_set_rate(clock_names, val * cpu_clk_granularity);
		break;
	default:
		rc = SBI_ERR_NOT_SUPPORTED;
		break;
	}

	return rc;
}

static int sg2044_cppc_probe(unsigned long reg)
{
	int rc;

	switch (reg) {
	case SBI_CPPC_DESIRED_PERF:
		rc = CPPC_REGISTER_WIDTH;
		break;
	default:
		rc = CPPC_REGISTER_NOT_IMPLEMENTED;
		break;
	}

	return rc;
}

static struct sbi_cppc_device sbi_sg2044_cppc = {
	.name		= "sg2044-cppc",
	.cppc_read	= sg2044_cppc_read,
	.cppc_write	= sg2044_cppc_write,
	.cppc_probe	= sg2044_cppc_probe,
};

static int sg2044_cppc_cold_init(const void *fdt, int nodeoff,
			       const struct fdt_match *match)
{
	const struct sbi_clk_device *clk;
	const fdt32_t *val;
	int len;

	clk = sbi_clk_get_device(fdt);
	if (!clk)
		return -1;

	val = fdt_getprop(fdt, nodeoff, "step", &len);
	if (!val)
		return SBI_ENODEV;

	cpu_clk_granularity = ((uint64_t)fdt32_to_cpu(val[0]) << 32) | (uint64_t)fdt32_to_cpu(val[1]);

	val = fdt_getprop(fdt, nodeoff, "clock-names", &len);
	if (!val)
		return SBI_ENODEV;

	sbi_strncpy(clock_names, (char *)val, MIN(sizeof(clock_names), len));

	sbi_cppc_set_device(&sbi_sg2044_cppc);

	return 0;
}

static const struct fdt_match sg2044_cppc_match[] = {
	{ .compatible = "sophgo,sg2044-cppc" },
	{},
};

struct fdt_driver fdt_cppc_sg2044 = {
	.match_table = sg2044_cppc_match,
	.init = sg2044_cppc_cold_init,
};
