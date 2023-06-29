/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Sophgo
 *
 * Authors:
 *   Chunzhi Lin <chunzhi.lin@sophgo.com>
 */

#include <libfdt.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_timer.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/gpio/fdt_gpio.h>
#include <sbi_utils/reset/fdt_reset.h>

struct cpld_reset {
	struct gpio_pin pin;
	u32 active_delay;
	u32 inactive_delay;
};

static struct cpld_reset poweroff = {
	.active_delay = 300,
	.inactive_delay = 300
};

static struct cpld_reset reboot = {
	.active_delay = 300,
	.inactive_delay = 300
};

static struct cpld_reset *cpld_reset_get(bool is_poweroff, u32 type)
{
	struct cpld_reset *reset = NULL;

	switch (type) {
	case SBI_SRST_RESET_TYPE_SHUTDOWN:
		if (is_poweroff)
			reset = &poweroff;
		break;
	case SBI_SRST_RESET_TYPE_COLD_REBOOT:
	case SBI_SRST_RESET_TYPE_WARM_REBOOT:
		if (!is_poweroff)
			reset = &reboot;
		break;
	}

	if (reset && !reset->pin.chip)
		reset = NULL;

	return reset;
}

static void cpld_reset_exec(struct cpld_reset *reset)
{
	if (reset) {
		/* drive it active, also inactive->active edge */
		gpio_direction_output(&reset->pin, 1);
		sbi_timer_mdelay(reset->active_delay);

		/* drive inactive, also active->inactive edge */
		gpio_set(&reset->pin, 0);
		sbi_timer_mdelay(reset->inactive_delay);
	}
	/* hang !!! */
	sbi_hart_hang();
}

static int cpld_system_poweroff_check(u32 type, u32 reason)
{
	if (cpld_reset_get(true, type))
		return 128;

	return 0;
}

static void cpld_system_poweroff(u32 type, u32 reason)
{
	cpld_reset_exec(cpld_reset_get(true, type));
}

static struct sbi_system_reset_device mango_reset_gpio_poweroff = {
	.name = "mango-cpld",
	.system_reset_check = cpld_system_poweroff_check,
	.system_reset = cpld_system_poweroff
};

static int cpld_system_reboot_check(u32 type, u32 reason)
{
	if (cpld_reset_get(false, type))
		return 128;

	return 0;
}

static void cpld_system_reboot(u32 type, u32 reason)
{
	cpld_reset_exec(cpld_reset_get(false, type));
}

static struct sbi_system_reset_device mango_reset_gpio_reboot = {
	.name = "mango-cpld",
	.system_reset_check = cpld_system_reboot_check,
	.system_reset = cpld_system_reboot
};

static int mango_cpld_reset_init(void *fdt, int nodeoff,
			   const struct fdt_match *match)
{
	int rc, len;
	const fdt32_t *val;
	bool is_poweroff = (ulong)match->data;
	struct cpld_reset *reset = (is_poweroff) ? &poweroff : &reboot;
	const char *dir_prop = "output";

	rc = fdt_gpio_pin_get(fdt, nodeoff, 0, &reset->pin);
	if (rc)
		goto out;

	if (fdt_getprop(fdt, nodeoff, dir_prop, &len)) {
		rc = gpio_direction_output(&reset->pin, 0);
		if (rc)
			goto out;
	}

	val = fdt_getprop(fdt, nodeoff, "active-delay-ms", &len);
	if (len > 0)
		reset->active_delay = fdt32_to_cpu(*val);

	val = fdt_getprop(fdt, nodeoff, "inactive-delay-ms", &len);
	if (len > 0)
		reset->inactive_delay = fdt32_to_cpu(*val);

	if (is_poweroff)
		sbi_system_reset_add_device(&mango_reset_gpio_poweroff);
	else
		sbi_system_reset_add_device(&mango_reset_gpio_reboot);

out:
	fdt_del_node(fdt, nodeoff);

	return rc;
}

static const struct fdt_match mango_cpld_poweroff_match[] = {
	{ .compatible = "mango,cpld-poweroff", .data = (void *)true},
	{ },
};

struct fdt_reset fdt_reset_sophgo_cpld_poweroff = {
	.match_table = mango_cpld_poweroff_match,
	.init = mango_cpld_reset_init,
};

static const struct fdt_match mango_cpld_reboot_match[] = {
	{ .compatible = "mango,cpld-reboot", .data = (void *)false},
	{ },
};

struct fdt_reset fdt_reset_sophgo_cpld_reboot = {
	.match_table = mango_cpld_reboot_match,
	.init = mango_cpld_reset_init,
};