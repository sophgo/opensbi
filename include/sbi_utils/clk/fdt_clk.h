/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Sophgo Inc.
 *
 * Authors:
 *   Haijiao Liu <haijiao.liu@sophgo.com>
 */

#ifndef __FDT_CLK_H__
#define __FDT_CLK_H__

#include <sbi/sbi_types.h>
#include <sbi_utils/fdt/fdt_driver.h>

#ifdef CONFIG_FDT_CLK

void fdt_clk_init(const void *fdt);

#else

static inline void fdt_clk_init(const void *fdt) { }

#endif

#endif
