/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Sophgo
 *
 */

#include <libfdt.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/timer/fdt_timer.h>
#include <sbi_utils/timer/mango_mtimer.h>

#define MTIMER_MAX_NR			16

static unsigned long mtimer_count = 0;
static struct mango_mtimer_data mtimer[MTIMER_MAX_NR];

static void __attribute__((unused))
mango_show_mtimer_data(struct mango_mtimer_data *mt)
{
	sbi_printf("frequency: %ld\n", mt->mtime_freq);
	sbi_printf("base address: 0x%016lx\n", mt->mtimecmp_addr);
	sbi_printf("address size: 0x%016lx\n", mt->mtimecmp_size);
	sbi_printf("hart count: %u\n", mt->hart_count);
	sbi_printf("has 64bit mmio: %s\n", mt->has_64bit_mmio ? "yes" : "no");
	sbi_printf("cores per cluster: %d\n", mt->cores_per_cluster);
	sbi_printf("cluster mtimer offset: 0x%x\n", mt->cluster_mtimer_offset);
}

static int mango_mtimer_get_extension(void *fdt, int nodeoff,
				  const char *name, u32* val)
{
	const fdt32_t *tmp;
	int len;

	if (!fdt || !name || !val)
		return SBI_EINVAL;

	tmp = fdt_getprop(fdt, nodeoff, name, &len);
	if (len > 0 && tmp)
		*val = fdt32_to_cpu(*tmp);
	else
		return SBI_ENOENT;

	return 0;
}

static int timer_mtimer_cold_init(void *fdt, int nodeoff,
				  const struct fdt_match *match)
{
	int rc;
	unsigned long addr, size;
	struct mango_mtimer_data *mt;
	u32 first_hartid = 0;

	if (MTIMER_MAX_NR <= mtimer_count)
		return SBI_ENOSPC;
	mt = &mtimer[mtimer_count];

	rc = fdt_parse_aclint_node(fdt, nodeoff, true,
				   &addr, &size, NULL, NULL,
				   &first_hartid, &mt->hart_count);
	if (rc)
		return rc;

	if (first_hartid)
		return SBI_EINVAL;

	mt->has_64bit_mmio = true;

	rc = fdt_parse_timebase_frequency(fdt, &mt->mtime_freq);
	if (rc)
		return rc;

	/* Set ACLINT MTIMER addresses */
	mt->mtimecmp_addr = addr;
	mt->mtimecmp_size = size;
	/* Parse additional ACLINT MTIMER properties */
	if (fdt_getprop(fdt, nodeoff, "mtimer,no-64bit-mmio", &rc))
		mt->has_64bit_mmio = false;

	rc = mango_mtimer_get_extension(fdt, nodeoff, "cores-per-cluster",
				  &mt->cores_per_cluster);
	if (rc)
		return rc;

	rc = mango_mtimer_get_extension(fdt, nodeoff, "cluster-mtimer-offset",
				  &mt->cluster_mtimer_offset);
	if (rc)
		return rc;

	mango_show_mtimer_data(mt);
	/* Initialize the MTIMER device */
	rc = mango_mtimer_cold_init(mt);
	if (rc)
		return rc;

	mtimer_count++;
	return 0;
}

static const struct fdt_match timer_mtimer_match[] = {
	{ .compatible = "riscv,mango-mtimer" },
	{ },
};

struct fdt_timer fdt_timer_mango_mtimer = {
	.match_table = timer_mtimer_match,
	.cold_init = timer_mtimer_cold_init,
	.warm_init = mango_mtimer_warm_init,
	.exit = NULL,
};
