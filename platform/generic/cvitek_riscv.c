/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_error.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>

__asm__(".section .rodata\n"
	".global suspend_sram_entry\n"
	".global suspend_sram_end\n"
	".type suspend_sram_entry, @object\n"
	".type suspend_sram_end, @object\n"
	".align 4\n"
	"suspend_sram_entry:\n"
	".incbin \"" STRINGIFY(PM_SRAM_BIN_PATH) "\"\n"
	".align 4\n"
	"suspend_sram_end:\n"
	".text\n");

#define REG_RTC_CTRL_BASE	0x05025000
#ifdef CONFIG_CV180X
#define RTC_IO_BASE		0x05027000
#define SUSPEND_SRAM_ENTRY 0x3C000000
#define RTC_SRAM_FLAG_ADDR	0x05026ff8
#endif
#ifdef CONFIG_CV181X
#define RTC_IO_BASE		0x05027000
#define SUSPEND_SRAM_ENTRY 0xC030000
#define RTC_SRAM_FLAG_ADDR	0x05026ff8
#endif
#ifdef CONFIG_CV190X
#define RTC_IO_BASE		0x05028000
#define SUSPEND_SRAM_ENTRY 0x3C000000
#define RTC_SRAM_FLAG_ADDR	0x05027ff8
#endif
#define memcpy sbi_memcpy

static void rtc_latch_pinmux_settings(void)
{
	writel(0x2, (void *)(REG_RTC_CTRL_BASE + 0xAC));
	writel(0x0, (void *)(RTC_IO_BASE + 0x84));
}

static void rtc_power_saving_settings_for_suspend(void)
{
	//TODO
}

static int cvitek_sbi_system_suspend_check(u32 sleep_type)
{
	return sleep_type == SBI_SUSP_SLEEP_TYPE_SUSPEND ? 0 : SBI_EINVAL;
}

static int cvitek_sbi_system_suspend(u32 sleep_type,
				unsigned long mmode_resume_addr)
{
	void (*suspend)(void) = (void *)SUSPEND_SRAM_ENTRY;
	u32 clk_reg;

	if (sleep_type != SBI_SUSP_SLEEP_TYPE_SUSPEND)
		return SBI_EINVAL;

	rtc_latch_pinmux_settings();
	rtc_power_saving_settings_for_suspend();

	/* store warmboot entry for resume */
	writel(mmode_resume_addr, (void *)RTC_SRAM_FLAG_ADDR);

	/*
	 * On cv190x, clk_tpu and clk_a53 share REG_CLK_EN_0 bit0 (cv190x has no
	 * A53 core, bit0 actually gates TPU SRAM). clk_a53 is CLK_IS_CRITICAL
	 * but may be gated by syscore suspend path. Re-enable bit0 before
	 * accessing TPU SRAM.
	 */
	clk_reg = readl((void *)0x03002000);
	writel(clk_reg | 0x1, (void *)0x03002000);

	/*
	 * Copy PM SRAM bin (ddr_suspend code) to TPU SRAM and jump to it.
	 * Use Shadow address (0x3C000000) not Origin (0x0C000000) - only
	 * Shadow supports instruction fetch on C906.
	 */
	memcpy((void *)SUSPEND_SRAM_ENTRY, suspend_sram_entry,
	       suspend_sram_end - suspend_sram_entry);

	/*
	 * Flush I-cache & D-cache + clear BTB/BHT before jumping to SRAM.
	 * fence.i + fence rw,rw alone is not enough on cv190x C906 when
	 * code is written via D-path to TPU SRAM. mcor 0x70013 = IC + DC
	 * invalidate + BTB/BHT clear (reference:
	 * opensbi/lib/utils/reset/fdt_reset_thead_asm.S).
	 *
	 * A short delay loop after mcor is needed on cv190x: without
	 * sbi_printf (which provides implicit UART delay), jumping
	 * immediately after mcor causes instruction access fault.
	 * The delay gives cache invalidate hardware time to complete.
	 */
	asm volatile("fence.i" ::: "memory");
	asm volatile("fence rw,rw" ::: "memory");
	asm volatile(
		"li t0, 0x70013\n\t"
		"csrw 0x7c2, t0\n\t"
		"fence rw,rw\n\t"
		::: "t0", "memory"
	);
	asm volatile("fence.i" ::: "memory");
	{
		volatile u32 dummy = 0;
		u32 i;
		for (i = 0; i < 1000; i++)
			dummy += i;
		(void)dummy;
	}

	suspend();
	wfi();

	return SBI_OK;
}

static struct sbi_system_suspend_device cvitek_sbi_suspend_device = {
	.name = "cvi-suspend",
	.system_suspend_check = cvitek_sbi_system_suspend_check,
	.system_suspend = cvitek_sbi_system_suspend,
};

static int cvitek_riscv_early_init(bool cold_boot, const struct fdt_match *match)
{
	sbi_system_suspend_set_device(&cvitek_sbi_suspend_device);

	return 0;
}

static const struct fdt_match cvitek_riscv_match[] = {
	{ .compatible = "cvitek,cv180x" },
	{ .compatible = "cvitek,cv181x" },
	{ .compatible = "cvitek,cv190x" }
};

const struct platform_override cvitek_riscv = {
	.match_table = cvitek_riscv_match,
	.early_init = cvitek_riscv_early_init,
};
