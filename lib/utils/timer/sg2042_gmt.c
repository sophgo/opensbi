/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Andes Technology Corporation
 *
 * Authors:
 *   Yu Chien Peter Lin <peterlin@andestech.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_timer.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/timer/sg2042_gmt.h>

struct sg2042gmt_data {
	unsigned long mtimer_base;
	unsigned long mtimecmp_base;
	unsigned long mtimecmp_size;
	unsigned long declared_freq;
	unsigned long actual_freq;
	unsigned long timecmp_freq;
} sg2042gmt;

#define SG2042GMT_TIMECMP_CLUSTER_SIZE		0x10000
#define SG2042GMT_TIMECMP_CLUSTER_CORE_COUNT	4

static void *timecmp_addr(unsigned int hart_id)
{
	unsigned int cluster_id = hart_id / SG2042GMT_TIMECMP_CLUSTER_CORE_COUNT;
	unsigned int cluster_offset = hart_id % SG2042GMT_TIMECMP_CLUSTER_CORE_COUNT;

	return (void *)(sg2042gmt.mtimecmp_base + 0x4000 +
			cluster_id * SG2042GMT_TIMECMP_CLUSTER_SIZE +
			cluster_offset * 8);
}

static u64 sg2042gmt_timer_value(void)
{
	u64 global_timer = readq_relaxed((void *)sg2042gmt.mtimer_base);

	/* convert global timer to declared frequency */
	return global_timer / (sg2042gmt.actual_freq / sg2042gmt.declared_freq);
}

static void sg2042gmt_timer_event_stop(void)
{
	unsigned int hart_id = current_hartid();

	writel_relaxed(-1U, timecmp_addr(hart_id));
	writel_relaxed(-1U, timecmp_addr(hart_id) + 4);
}

static void sg2042gmt_timer_event_start(u64 next_event)
{
	unsigned int hart_id = current_hartid();

	u64 global_timer = readq_relaxed((void *)sg2042gmt.mtimer_base);
	u64 clint_timer = csr_read(CSR_TIME);
	u64 delta_global_timer, delta_clint_timer;

	delta_global_timer =
		(next_event * (sg2042gmt.actual_freq / sg2042gmt.declared_freq)) - global_timer;

	delta_clint_timer = delta_global_timer * (sg2042gmt.timecmp_freq / sg2042gmt.actual_freq);

	next_event = clint_timer + delta_clint_timer;

	writel_relaxed(next_event >> 32, timecmp_addr(hart_id) + 4);
	writel_relaxed(next_event & 0xffffffff, timecmp_addr(hart_id));
}

static struct sbi_timer_device sg2042gmt_timer = {
	.name		   = "sg2042gmt",
	.timer_freq	   = 50000000,
	.timer_value	   = sg2042gmt_timer_value,
	.timer_event_start = sg2042gmt_timer_event_start,
	.timer_event_stop  = sg2042gmt_timer_event_stop
};

int sg2042gmt_cold_timer_init(unsigned long mtimer_base,
			      unsigned long mtimecmp_base,
			      unsigned long mtimecmp_size,
			      unsigned long declared_freq,
			      unsigned long actual_freq,
			      unsigned long timecmp_freq)
{
	int err;

	/* Add SG2042 GMT timer and time compare region to the root domain */
#if 0
	err = sbi_domain_root_add_memrange(
		mtimer_base, 4096, 4096,
		SBI_DOMAIN_MEMREGION_MMIO | SBI_DOMAIN_MEMREGION_READABLE);
	if (err) {
		sbi_printf("add timer to root domain failed\n");
		return err;
	}
#endif

	err = sbi_domain_root_add_memrange(
		mtimecmp_base, mtimecmp_size, 4096,
		SBI_DOMAIN_MEMREGION_MMIO |
		SBI_DOMAIN_MEMREGION_READABLE |
		SBI_DOMAIN_MEMREGION_WRITEABLE);
	if (err) {
		return err;
	}


	sg2042gmt_timer.timer_freq = declared_freq;

	sg2042gmt.mtimer_base = mtimer_base;
	sg2042gmt.mtimecmp_base = mtimecmp_base;
	sg2042gmt.mtimecmp_size = mtimecmp_size;
	sg2042gmt.declared_freq = declared_freq;
	sg2042gmt.actual_freq = actual_freq;
	sg2042gmt.timecmp_freq = timecmp_freq;

	sbi_timer_set_device(&sg2042gmt_timer);

	return 0;
}

int sg2042gmt_warm_timer_init(void)
{
	if (!sg2042gmt.mtimer_base)
		return SBI_ENODEV;

	sg2042gmt_timer_event_stop();

	return 0;
}
