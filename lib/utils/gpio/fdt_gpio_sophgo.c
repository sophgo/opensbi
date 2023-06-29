/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Sophgo
 *
 * Author:
 *   Chunzhi Lin <chunzhi.lin@sophgo.com>
 */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/gpio/fdt_gpio.h>

#define SOPHGO_GPIO_CHIP_MAX	3

#define SOPHGO_GPIO_PINS_MIN	0
#define SOPHGO_GPIO_PINS_MAX	31
#define SOPHGO_GPIO_PINS_DEF	16

#define SOPHGO_GPIO_SWPORTA_DR_OFFSET	0x00
#define SOPHGO_GPIO_SWPORTA_DDR_OFFSET	0x04
#define SOPHGO_GPIO_SWPORTA_CTL_OFFSET	0x08

#define SOPHGO_GPIO_BIT(offset)	(1U << (offset))
#define SOPHGO_GPIO_STARTUP_FLAG	SOPHGO_GPIO_BIT(16)

struct sophgo_gpio_chip {
	unsigned long addr;
	struct gpio_chip chip;
};

static unsigned int sophgo_gpio_chip_count;
static struct sophgo_gpio_chip sophgo_gpio_chip_array[SOPHGO_GPIO_CHIP_MAX];

static int sophgo_gpio_direction_output(struct gpio_pin *gp, int value)
{
	unsigned int v;
	struct sophgo_gpio_chip *chip =
		container_of(gp->chip, struct sophgo_gpio_chip, chip);

	v = readl((volatile void *)(chip->addr + SOPHGO_GPIO_SWPORTA_DDR_OFFSET));
	v |= SOPHGO_GPIO_BIT(gp->offset);
	writel(v, (volatile void *)(chip->addr + SOPHGO_GPIO_SWPORTA_DDR_OFFSET));

	v = readl((volatile void *)(chip->addr + SOPHGO_GPIO_SWPORTA_DR_OFFSET));
	if (!value)
		v &= ~SOPHGO_GPIO_BIT(gp->offset);
	else
		v |= SOPHGO_GPIO_BIT(gp->offset);
	writel(v, (volatile void *)(chip->addr + SOPHGO_GPIO_SWPORTA_DR_OFFSET));

	return 0;
}

static void sophgo_gpio_set(struct gpio_pin *gp, int value)
{
	unsigned int v;
	struct sophgo_gpio_chip *chip =
		container_of(gp->chip, struct sophgo_gpio_chip, chip);

	v = readl((volatile void *)(chip->addr + SOPHGO_GPIO_SWPORTA_DR_OFFSET));

	if (!value)
		v &= ~SOPHGO_GPIO_BIT(gp->offset);
	else
		v |= SOPHGO_GPIO_BIT(gp->offset);

	writel(v, (volatile void *)(chip->addr + SOPHGO_GPIO_SWPORTA_DR_OFFSET));

	return;
}

static int sophgo_gpio_addr_get(void *fdt, int nodeoff, unsigned long *addr)
{
	int parent, len;
	unsigned long addr_high, addr_low;
	const fdt32_t *prop_addr;

	parent = fdt_parent_offset(fdt, nodeoff);
	if (parent < 0)
		return parent;

	prop_addr = fdt_getprop(fdt, parent, "reg", &len);
	if (!prop_addr)
		return SBI_ENODEV;

	addr_high = fdt32_to_cpu(*prop_addr++);
	addr_low = fdt32_to_cpu(*prop_addr);

	*addr = addr_high << 32 | addr_low;

	return 0;
}

static void sophgo_system_normal_startup_flag(unsigned long addr)
{
	unsigned int v;

	v = readl((volatile void *)(addr + SOPHGO_GPIO_SWPORTA_DDR_OFFSET));
	v |= SOPHGO_GPIO_STARTUP_FLAG;
	writel(v, (volatile void *)(addr + SOPHGO_GPIO_SWPORTA_DDR_OFFSET));

	v = readl((volatile void *)(addr + SOPHGO_GPIO_SWPORTA_DR_OFFSET));
	v |= SOPHGO_GPIO_STARTUP_FLAG;
	writel(v, (volatile void *)(addr + SOPHGO_GPIO_SWPORTA_DR_OFFSET));
}

extern struct fdt_gpio fdt_gpio_sophgo;

static int sophgo_gpio_init(void *fdt, int nodeoff, u32 phandle,
				 const struct fdt_match *match)
{
	int rc;
	struct sophgo_gpio_chip *chip;
	unsigned long addr;

	if (SOPHGO_GPIO_CHIP_MAX <= sophgo_gpio_chip_count) {
		rc = SBI_ENOSPC;
		return rc;
	}

	chip = &sophgo_gpio_chip_array[sophgo_gpio_chip_count];

	rc = sophgo_gpio_addr_get(fdt, nodeoff, &addr);
	if (rc)
		return rc;

	chip->addr = addr;
	chip->chip.driver = &fdt_gpio_sophgo;
	chip->chip.id = phandle;
	chip->chip.ngpio = SOPHGO_GPIO_PINS_MAX;
	chip->chip.direction_output = sophgo_gpio_direction_output;
	chip->chip.set = sophgo_gpio_set;
	rc = gpio_chip_add(&chip->chip);

	if (rc)
		return rc;

	sophgo_gpio_chip_count++;
	sophgo_system_normal_startup_flag(addr);

	return 0;
}

static const struct fdt_match sophgo_gpio_match[] = {
	{ .compatible = "sophgo,gpio0" },
	{ },
};

struct fdt_gpio fdt_gpio_sophgo = {
	.match_table = sophgo_gpio_match,
	.xlate = fdt_gpio_simple_xlate,
	.init = sophgo_gpio_init,
};
