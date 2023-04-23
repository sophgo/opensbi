/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Lin Chunzhi <chunzhi.lin@sophgo.com>
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

#define MANGO_BOARD_TYPE		0x80
#define MANGO_BOARD_TYPE_MASK		1 << 7

#define REG_MCU_BOARD_TYPE		0x00
#define REG_MCU_CMD		0x03

#define CMD_POWEROFF		0x02
#define CMD_RESET		0x03
#define CMD_REBOOT		0x07

static struct {
	struct i2c_adapter *adapter;
	uint32_t reg;
} mango;

static int mango_system_reset_check(u32 type, u32 reason)
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

static inline int mango_sanity_check(struct i2c_adapter *adap, uint32_t reg)
{
	static uint8_t val;
	int ret;

	/* check board type*/
	ret = i2c_adapter_reg_read(adap, reg, REG_MCU_BOARD_TYPE, &val);
	if (ret)
		return ret;

	if ((val & MANGO_BOARD_TYPE_MASK) != MANGO_BOARD_TYPE)
		return SBI_ENODEV;

	return 0;
}

static inline int mango_shutdown(struct i2c_adapter *adap, uint32_t reg)
{
	int ret;
	ret = i2c_adapter_reg_write(adap, reg, REG_MCU_CMD, CMD_POWEROFF);

	if (ret)
		return ret;

	return 0;
}

static inline int mango_reset(struct i2c_adapter *adap, uint32_t reg)
{
	int ret;
	ret = i2c_adapter_reg_write(adap, reg, REG_MCU_CMD, CMD_REBOOT);

	if (ret)
		return ret;

	return 0;
}

static void mango_system_reset(u32 type, u32 reason)
{
	struct i2c_adapter *adap = mango.adapter;
	uint32_t reg = mango.reg;
	int ret;
	if (adap) {
		/* sanity check */
		ret = mango_sanity_check(adap, reg);
		if (ret) {
			sbi_printf("%s: chip is not mango\n", __func__);
			goto skip_reset;
		}

		switch (type) {
		case SBI_SRST_RESET_TYPE_SHUTDOWN:
			mango_shutdown(adap, reg);
			break;
		case SBI_SRST_RESET_TYPE_COLD_REBOOT:
		case SBI_SRST_RESET_TYPE_WARM_REBOOT:
			mango_reset(adap, reg);
			break;
		}
	}

skip_reset:
	sbi_hart_hang();
}

static struct sbi_system_reset_device mango_reset_i2c = {
	.name = "mango-reset",
	.system_reset_check = mango_system_reset_check,
	.system_reset = mango_system_reset
};

static int mango_reset_init(void *fdt, int nodeoff,
			   const struct fdt_match *match)
{
	int rc, i2c_bus;
	struct i2c_adapter *adapter;
	uint64_t addr;

	/* we are mango,mcu node */
	rc = fdt_get_node_addr_size(fdt, nodeoff, 0, &addr, NULL);
	if (rc)
		return rc;

	mango.reg = addr;

	/* find i2c bus parent node */
	i2c_bus = fdt_parent_offset(fdt, nodeoff);
	if (i2c_bus < 0)
		return i2c_bus;

	/* i2c adapter get */
	rc = fdt_i2c_adapter_get(fdt, i2c_bus, &adapter);
	if (rc)
		return rc;

	mango.adapter = adapter;

	sbi_system_reset_add_device(&mango_reset_i2c);
	fdt_del_node(fdt, nodeoff);

	return 0;
}

static const struct fdt_match mango_reset_match[] = {
	{ .compatible = "mango,reset", .data = (void *)true},
	{ },
};

struct fdt_reset fdt_reset_sophgo_mcu = {
	.match_table = mango_reset_match,
	.init = mango_reset_init,
};
