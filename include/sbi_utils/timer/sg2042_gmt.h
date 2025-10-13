/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 SOPHGO Corporation
 *
 * Authors:
 *   Chao Wei <chao.wei@sophgo.com>
 */

#ifndef __SG2042_GMT_H__
#define __SG2042_GMT_H__

int sg2042gmt_cold_timer_init(unsigned long mtimer_base,
			      unsigned long mtimecmp_base,
			      unsigned long mtimecmp_size,
			      unsigned long delcared_freq,
			      unsigned long actual_freq,
			      unsigned long timecmp_freq);
int sg2042gmt_warm_timer_init(void);

#endif /* __SG2042_GMT_H__ */
