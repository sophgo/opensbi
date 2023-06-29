/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Sophgo
 *
 * Authors:
 *   Chunzhi Lin <chunzhi.lin@sophgo.com>
 */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_timer.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/reset/fdt_reset.h>

#define TOP_BASE		0x7030010000
#define SOPHGO_TOP_CTRL_REG_OFFSET		0x008

#define REG_BIT(offset)		(1U << (offset))
#define BIT_MASK_TOP_CTRL_SW_ROOT_RESET_EN		REG_BIT(2)

#define SOPHGO_WDT_CR_REG_OFFSET		0x00
#define SOPHGO_WDT_TORR_REG_OFFSET		0x04
#define SOPHGO_WDT_CRR_REG_OFFSET		0x0C

static volatile char *sophgo_wdt_base;
static volatile char *sophgo_top_base;

static void sophgo_wdt_system_reset(u32 type, u32 reason)
{
	u32 val;
	
	val = readl(sophgo_top_base + SOPHGO_TOP_CTRL_REG_OFFSET);
	writel((val | BIT_MASK_TOP_CTRL_SW_ROOT_RESET_EN),
		 sophgo_top_base + SOPHGO_TOP_CTRL_REG_OFFSET);
	sbi_timer_udelay(1);

	/*next reset time = 2^(16 + 0x0b) / 100M = 1.34s */
	writel(0x0b, sophgo_wdt_base + SOPHGO_WDT_TORR_REG_OFFSET);
	sbi_timer_udelay(1);

	/* a safety feature for counter restart register */
	writel(0x76, sophgo_wdt_base + SOPHGO_WDT_CRR_REG_OFFSET);
	sbi_timer_udelay(1);

	/* reset pluse length: 32 pclk cycles; enable wdt */
	writel(0x11, sophgo_wdt_base + SOPHGO_WDT_CR_REG_OFFSET);

	return;
}

static int sophgo_wdt_system_reset_check(u32 type, u32 reason)
{
	switch (type) {
	case SBI_SRST_RESET_TYPE_COLD_REBOOT:
	case SBI_SRST_RESET_TYPE_WARM_REBOOT:
		return 1;
	}

	return 0;
}

static int sophgo_wdt_system_get_top_base(void *fdt,
		 int nodeoff, unsigned long *addr)
{
	const fdt32_t *val;
	int len, noff;

	val = fdt_getprop(fdt, nodeoff, "subctrl-syscon", &len);
	if (val || len >= sizeof(fdt32_t)) {
		noff = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(*val));
		if (noff < 0)
			return noff;
	}

	return fdt_get_node_addr_size(fdt, noff, 0, addr, NULL);
}

static struct sbi_system_reset_device mango_reset_wdt = {
	.name = "mango-wdt",
	.system_reset_check = sophgo_wdt_system_reset_check,
	.system_reset = sophgo_wdt_system_reset,
};

static int mango_wdt_reset_init(void *fdt, int nodeoff,
			   const struct fdt_match *match)
{
	unsigned long wdt_addr, top_addr;
	int rc;

	rc = fdt_get_node_addr_size(fdt, nodeoff, 0, &wdt_addr, NULL);
	if (rc < 0 || !wdt_addr) {
		return SBI_ENODEV;
	}

	sophgo_wdt_base = (volatile char *)(unsigned long)wdt_addr;

	rc = sophgo_wdt_system_get_top_base(fdt, nodeoff, &top_addr);
	if (rc < 0 || !top_addr)
		return SBI_ENODEV;

	sophgo_top_base = (volatile char *)(unsigned long)top_addr;

	sbi_system_reset_add_device(&mango_reset_wdt);

	return 0;
}

static const struct fdt_match mango_wdt_reset_match[] = {
	{ .compatible = "mango,wdt-reset", .data = (void *)true},
	{ },
};

struct fdt_reset fdt_reset_sophgo_wdt = {
	.match_table = mango_wdt_reset_match,
	.init = mango_wdt_reset_init,
};
