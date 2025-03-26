/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Sophgo Inc.
 *
 * Authors:
 *   Haijiao Liu <haijiao.liu@sophgo.com>
 */

#include <sbi_utils/clk/fdt_clk.h>

/* List of FDT clock drivers generated at compile time */
extern const struct fdt_driver *const fdt_clk_drivers[];

void fdt_clk_init(const void *fdt)
{
	fdt_driver_init_all(fdt, fdt_clk_drivers);
}
