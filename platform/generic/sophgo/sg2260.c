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
#include <thead_c9xx.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define SOPHGO_sg2260_TIMER_BASE		0x70ac000000UL
#define SOPHGO_sg2260_TIMER_OFFSET	0x10000UL

extern struct sbi_platform platform;

int sg2260_early_init(bool cold_boot, const struct fdt_match *match)
{
	const struct sbi_platform * plat = sbi_platform_thishart_ptr();

	/*
	 * Sophgo sg2260 board use seperate 16/32 timers while initiating,
	 * merge them as a single domain to avoid wasting.
	 */
	if (cold_boot)
		return sbi_domain_root_add_memrange(SOPHGO_sg2260_TIMER_BASE,
						    SOPHGO_sg2260_TIMER_OFFSET *
						    sbi_platform_hart_count(plat),
						    MTIMER_REGION_ALIGN,
						    (SBI_DOMAIN_MEMREGION_MMIO |
						     SBI_DOMAIN_MEMREGION_M_READABLE |
						     SBI_DOMAIN_MEMREGION_M_WRITABLE));
	return 0;
}

static int sg2260_extensions_init(const struct fdt_match *match,
				     struct sbi_hart_features *hfeatures)
{
	// sbi_pmu_set_device(&thead_c9xx_pmu_device);

	/* auto-detection doesn't work on t-head c9xx cores */
	hfeatures->mhpm_count = 29;
	hfeatures->mhpm_bits = 64;

	return 0;
}
static void sg2260_fw_init(void *fdt, const struct fdt_match *match)
{
	platform.hart_stack_size = 16384;
}

static const struct fdt_match sophgo_sg2260_match[] = {
	{ .compatible = "sophgo, sg2260_pld" },
	{ },
};

const struct platform_override sophgo_sg2260 = {
	.match_table		= sophgo_sg2260_match,
	.fw_init			= sg2260_fw_init,
	// .early_init		= sg2260_early_init,
	.extensions_init	= sg2260_extensions_init,
};
