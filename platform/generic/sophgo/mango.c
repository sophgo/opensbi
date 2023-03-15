/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Sophgo
 *
 */

#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>

static const struct fdt_match sophgo_mango_match[] = {
	{ .compatible = "sophgo,mango" },
	{ },
};

const struct platform_override sophgo_mango = {
	.match_table		= sophgo_mango_match,
};