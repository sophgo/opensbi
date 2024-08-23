/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Yang Dong <dong.yang@sophgo.com>
 */

#include <libfdt.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/reset/fdt_reset.h>
#include <sbi_utils/i2c/fdt_i2c.h>
#include <sbi/sbi_timer.h>

#define SOPHGO_BOARD_TYPE		0x80

#define REG_MCU_BOARD_TYPE		0x00
#define REG_MCU_CMD		0x03

#define CMD_POWEROFF		0x02
#define CMD_RESET		0x03
#define CMD_REBOOT		0x07

static struct {
	struct i2c_adapter *adapter;
	uint32_t reg;
} sg2044;

static int sg2044_system_reset_check(u32 type, u32 reason)
{
	switch (type) {
	case SBI_SRST_RESET_TYPE_SHUTDOWN:
		return 1;
	case SBI_SRST_RESET_TYPE_COLD_REBOOT:
	case SBI_SRST_RESET_TYPE_WARM_REBOOT:
		return 255;
	}

	return 0;
}

static inline int sg2044_sanity_check(struct i2c_adapter *adap, uint32_t reg)
{
	static uint8_t val;
	int ret;

	/* check board type*/
	ret = i2c_adapter_reg_read(adap, reg, REG_MCU_BOARD_TYPE, &val);
	if (ret)
		return ret;

	if (val != SOPHGO_BOARD_TYPE)
		return SBI_ENODEV;

	return 0;
}

static inline int sg2044_shutdown(struct i2c_adapter *adap, uint32_t reg)
{
	int ret;

	ret = i2c_adapter_reg_write(adap, reg, REG_MCU_CMD, CMD_POWEROFF);

	if (ret)
		return ret;

	return 0;
}

static inline int sg2044_reset(struct i2c_adapter *adap, uint32_t reg)
{
	int ret;

	ret = i2c_adapter_reg_write(adap, reg, REG_MCU_CMD, CMD_REBOOT);

	if (ret)
		return ret;

	return 0;
}

static void sg2044_system_reset(u32 type, u32 reason)
{
	struct i2c_adapter *adap = sg2044.adapter;
	uint32_t reg = sg2044.reg;

	if (adap) {
		switch (type) {
		case SBI_SRST_RESET_TYPE_SHUTDOWN:
			sg2044_shutdown(adap, reg);
			break;
		case SBI_SRST_RESET_TYPE_COLD_REBOOT:
		case SBI_SRST_RESET_TYPE_WARM_REBOOT:
			sg2044_reset(adap, reg);
			break;
		}
	}

	sbi_hart_hang();
}

static struct sbi_system_reset_device sg2044_reset_i2c = {
	.name = "sg2044-reset",
	.system_reset_check = sg2044_system_reset_check,
	.system_reset = sg2044_system_reset
};

static int sg2044_reset_init(void *fdt, int nodeoff,
			   const struct fdt_match *match)
{
	int rc, i2c_bus;
	struct i2c_adapter *adapter;
	uint64_t addr;

	/* we are sg2044,mcu node */
	rc = fdt_get_node_addr_size(fdt, nodeoff, 0, &addr, NULL);
	if (rc)
		return rc;

	sg2044.reg = addr;

	/* find i2c bus parent node */
	i2c_bus = fdt_parent_offset(fdt, nodeoff);
	if (i2c_bus < 0)
		return i2c_bus;

	/* i2c adapter get */
	rc = fdt_i2c_adapter_get(fdt, i2c_bus, &adapter);
	if (rc)
		return rc;

	sg2044.adapter = adapter;

	sbi_system_reset_add_device(&sg2044_reset_i2c);

	return 0;
}

static const struct fdt_match sg2044_reset_match[] = {
	{ .compatible = "sg2044,reset", .data = (void *)true},
	{ },
};

struct fdt_reset fdt_reset_sophgo_mcu = {
	.match_table = sg2044_reset_match,
	.init = sg2044_reset_init,
};
