/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Sophgo Inc.
 *
 */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_cppc.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_scratch.h>
#include <sbi_utils/cppc/fdt_cppc.h>
#include <sbi_utils/clk/sg2044_clk.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_clk.h>

#define CPU_CLK_GRANULARITY	25000000UL
#define CPPC_REGISTER_WIDTH	64
#define CPPC_REGISTER_NOT_IMPLEMENTED	0

enum {
	HIGHEST_PERFORMANCE = 0,
	NOMINAL_PERFORMANCE,
	LOWEST_NONLINEAR_PERFORMANCE,
	LOWEST_PERFORMANCE,
	GUARANTEED_PERFORMANCE_REGISTER,
	DESIRED_PERFORMANCE_REGISTER,
	MINIMUM_PERFORMANCE_REGISTER,
	MAXIMUM_PERFORMANCE_REGISTER,
	PERFORMANCE_REDUCTION_TOLERANCE_REGISTER,
	TIME_WINDOW_REGISTER,
	COUNTER_WRAPAROUND_TIME,
	REFERENCE_PERFORMANCE_COUNTER_REGISTER,
	DELIVERED_PERFORMANCE_COUNTER_REGISTER,
	PERFORMANCE_LIMITED_REGISTER,
	CPPC_ENABLE_REGISTER,
	AUTONOMOUS_SELECTION_ENABLE,
	AUTONOMOUS_ACTIVITY_WINDOW_REGISTER,
	ENERGY_PERFORMANCE_PREFERENCE_REGISTER,
	REFERENCE_PERFORMANCE,
	LOWEST_FREQUENCY,
	NOMINAL_FREQUENCY,
};

static int sg2044_cppc_read(unsigned long reg, u64 *val)
{
	int rc = SBI_SUCCESS;

	switch (reg) {
	case DESIRED_PERFORMANCE_REGISTER:
		*val = sbi_clk_get_rate("mpll1_clock") / CPU_CLK_GRANULARITY;
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
	case DESIRED_PERFORMANCE_REGISTER:
		rc = sbi_clk_set_rate("mpll1_clock", val * CPU_CLK_GRANULARITY);
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
	case DESIRED_PERFORMANCE_REGISTER:
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

	clk = sbi_clk_get_device(fdt);
	if (!clk)
		return -1;

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
