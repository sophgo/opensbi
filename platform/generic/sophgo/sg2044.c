/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c)
 *
 * Authors:
 *   Inochi Amaoto <inochiama@outlook.com>
 *   YuQing Cai <caiyuqing_hz@163.com>
 *   ZhenYu Zhang <1204122531@qq.com>
 */

#include <platform_override.h>
#include <thead/c9xx_pmu.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/fdt/fdt_helper.h>

static u32 selected_hartid = -1;

static bool sg2044_cold_boot_allowed(u32 hartid,
                                   const struct fdt_match *match)
{
	if (selected_hartid != -1)
		return (selected_hartid == hartid);

	return true;
}

static int sg2044_extensions_init(const struct fdt_match *match,
				  struct sbi_hart_features *hfeatures)
{
	thead_c9xx_register_pmu_device();
	return 0;
}

static const struct fdt_match sophgo_sg2044_match[] = {
	{ .compatible = "sophgo,sg2044" },
	{ },
};

const struct platform_override sophgo_sg2044 = {
	.match_table		= sophgo_sg2044_match,
	.cold_boot_allowed 	= sg2044_cold_boot_allowed,
	.extensions_init	= sg2044_extensions_init,
};
