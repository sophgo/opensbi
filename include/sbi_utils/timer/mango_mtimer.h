/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Sophgo
 *
 */

#ifndef __TIMER_MANGO_MTIMER_H__
#define __TIMER_MANGO_MTIMER_H__

#include <sbi/sbi_types.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define MANGO_MTIMER_ALIGN			0x8
#define MANGO_MTIMER_MAX_HARTS		4095

struct mango_mtimer_data {
	/* Public details */
	unsigned long mtime_freq;
	unsigned long mtimecmp_addr;
	unsigned long mtimecmp_size;
	u32 hart_count;
	bool has_64bit_mmio;
	u64 (*time_rd)(volatile u64 *addr);
	void (*time_wr)(bool timecmp, u64 value, volatile u64 *addr);
	u32 cores_per_cluster;
	u32 cluster_mtimer_offset;
};

int mango_mtimer_cold_init(struct mango_mtimer_data *mt);
int mango_mtimer_warm_init(void);

#endif
