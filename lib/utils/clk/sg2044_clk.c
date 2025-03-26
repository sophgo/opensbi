/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Sophgo Inc.
 *
 */

#include <libfdt.h>
#include <sbi/sbi_timer.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <sbi/riscv_io.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_driver.h>
#include <sbi_utils/clk/sg2044_clk.h>
#include <sbi/sbi_clk.h>

#define	POSTDIV_RESULT_INDEX	2

static uintptr_t top_base_addr;

static int sg2044_pll_mux[][2] = {
	{MPLL0_CLK, 0}, {MPLL1_CLK, 1}, {MPLL2_CLK, 2}, {MPLL3_CLK, 3},
	{MPLL4_CLK, 4}, {MPLL5_CLK, 5}, {FPLL1_CLK, 6}, {DPLL0_CLK, 7},
	{DPLL1_CLK, 8}, {DPLL2_CLK, 9}, {DPLL3_CLK, 10}, {DPLL4_CLK, 11},
	{DPLL5_CLK, 12}, {DPLL6_CLK, 13}, {DPLL7_CLK, 14}
};

static int postdiv1_2[][3] = {
	{2, 4,  8}, {3, 3,  9}, {2, 5, 10}, {2, 6, 12},
	{2, 7, 14}, {3, 5, 15}, {4, 4, 16}, {3, 6, 18},
	{4, 5, 20}, {3, 7, 21}, {4, 6, 24}, {5, 5, 25},
	{4, 7, 28}, {5, 6, 30}, {5, 7, 35}, {6, 6, 36},
	{6, 7, 42}, {7, 7, 49}
};

static inline unsigned long abs_diff(unsigned long a, unsigned long b)
{
	return (a > b) ? (a - b) : (b - a);
}

static struct sg2044_pll_clock sg2044_root_pll_clks[] = {
	{
		.id = MPLL0_CLK,
		.name = "mpll0_clock",
		.default_rate = 2000 * MHZ,
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = MPLL1_CLK,
		.name = "mpll1_clock",
		.default_rate = 2000 * MHZ,
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = MPLL2_CLK,
		.name = "mpll2_clock",
		.default_rate = 1000 * MHZ,
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = MPLL3_CLK,
		.name = "mpll3_clock",
		.default_rate = 2000 * MHZ,
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = MPLL4_CLK,
		.name = "mpll4_clock",
		.default_rate = 1050 * MHZ,
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = MPLL5_CLK,
		.name = "mpll5_clock",
		.default_rate = 900 * MHZ,
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = FPLL0_CLK,
		.name = "fpll0_clock",
	}, {
		.id = FPLL1_CLK,
		.name = "fpll1_clock",
	}, {
		.id = DPLL0_CLK,
		.name = "dpll0_clock",
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = DPLL1_CLK,
		.name = "dpll1_clock",
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = DPLL2_CLK,
		.name = "dpll2_clock",
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = DPLL3_CLK,
		.name = "dpll3_clock",
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = DPLL4_CLK,
		.name = "dpll4_clock",
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = DPLL5_CLK,
		.name = "dpll5_clock",
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = DPLL6_CLK,
		.name = "dpll6_clock",
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}, {
		.id = DPLL7_CLK,
		.name = "dpll7_clock",
		.status_offset = 0x98,
		.enable_offset = 0x9c,
	}
};

/**
 * @name    top_misc_read
 * @brief   Read the value from a register at a specific offset within the TOP MISC base address.
 * @ingroup clk
 * @details This function reads a 32-bit value from a I/O register. The register is
 * located at an offset from the TOP MISC base address.
 *
 * @param [in]  offset The offset from the TOP MISC base address where the register is located.
 * @param [out] value  Pointer to a variable where the read value will be stored.
 * @retval  void
 */
static inline void top_misc_read(uintptr_t offset, uint32_t *value)
{
	*value = readl((const volatile void *)top_base_addr + offset);
}

/**
 * @name    top_misc_write
 * @brief   Write a value to a register at a specific offset within the TOP MISC base address.
 * @ingroup clk
 * @details This function writes a 32-bit value to a I/O register. The register is
 * located at an offset from the TOP MISC base address.
 *
 * @param [in] offset The offset from the TOP MISC base address where the register is located.
 * @param [in] value  Value to be written to the register.
 * @retval void
 */
static inline void top_misc_write(uintptr_t offset, uint32_t value)
{
	writel(value, (volatile void *)(top_base_addr + offset));
}

/**
 * @name    sg2044_pll_write_h
 * @brief   Write a value to a higher part of  the PLL control register for a specific PLL ID.
 * @ingroup clk
 * @details This function calculates the correct offset for the higher part of the PLL control register
 * based on the PLL ID and writes a value to it.
 *
 * @param [in] id    The ID of the PLL whose higher control register is to be written.
 * @param [in] value The value to write to the PLL control register.
 * @retval  void
 */
static void sg2044_pll_write_h(int id, int value)
{
	top_misc_write(PLL_CTRL_OFFSET + (id << 3), value);
}

/**
 * @name    sg2044_pll_read_h
 * @brief   Read the value from a higher part of the PLL control register for a specific PLL ID.
 * @ingroup clk
 * @details This function calculates the correct offset for the higher part of the PLL control register
 * based on the PLL ID and reads the value from it.
 *
 * @param [in]  id     The ID of the PLL whose higher control register is to be read.
 * @param [out] pvalue Pointer to a variable where the read value will be stored.
 * @retval  void
 *
 */
static void sg2044_pll_read_h(int id, uint32_t *pvalue)
{
	top_misc_read(PLL_CTRL_OFFSET + (id << 3), pvalue);
}

/**
 * @name    sg2044_pll_write_l
 * @brief   Write a value to a lower part of the PLL control register for a specific PLL ID.
 * @ingroup clk
 * @details This function calculates the correct offset for the lower part of the PLL control register
 * based on the PLL ID and writes a value to it.
 *
 * @param [in] id    The ID of the PLL whose lower control register part is to be written.
 * @param [in] value The value to write to the lower part of the PLL control register.
 * @retval     void
 */
static void sg2044_pll_write_l(int id, int value)
{
	top_misc_write(PLL_CTRL_OFFSET + (id << 3) - 4, value);
}

/**
 * @name    sg2044_pll_read_l
 * @brief   Read the value from a lower part of the PLL control register for a specific PLL ID.
 * @ingroup clk
 * @details This function calculates the correct offset for the lower part of the PLL control register
 * based on the PLL ID and reads the value from it.
 *
 * @param [in]  id     The ID of the PLL whose lower control register part is to be read.
 * @param [out] pvalue Pointer to a variable where the read value will be stored.
 * @retval      void
 */
static void sg2044_pll_read_l(int id, uint32_t *pvalue)
{
	top_misc_read(PLL_CTRL_OFFSET + (id << 3) - 4, pvalue);
}

/**
 * @name    __pll_get_postdiv_1_2
 * @brief   Calculate the post-divider values (POSTDIV1 and POSTDIV2) for PLL configuration.
 * @ingroup clk
 * @details The function computes POSTDIV1 and POSTDIV2 based on the provided FBDIV, REFDIV, input rate,
 * and parent rate. POSTDIV = (parent_rate/REFDIV) x FBDIV/input_rate, where POSTDIV = POSTDIV1*POSTDIV2
 *
 * @param[in] rate       The desired output frequency rate from the PLL.
 * @param[in] prate      The parent frequency rate to the PLL.
 * @param[in] fbdiv      Feedback divider value used in the PLL.
 * @param[in] refdiv     Reference divider value used in the PLL.
 * @param[out] postdiv1   Pointer to store the calculated POSTDIV1 value.
 * @param[out] postdiv2   Pointer to store the calculated POSTDIV2 value.
 *
 * @return           Returns 0 on success, -1 if the computed post-divider exceeds available range
 */
static int __pll_get_postdiv_1_2(uint64_t rate, uint64_t prate,
				 uint32_t fbdiv, uint32_t refdiv, uint32_t *postdiv1,
				 uint32_t *postdiv2)
{
	int index = 0;
	int ret = 0;
	uint64_t tmp0;

	/* calculate (parent_rate/refdiv)
	 * and result save to tmp0
	 */
	tmp0 = prate;
	do_div(tmp0, refdiv);

	/* calcuate ((parent_rate/REFDIV) x FBDIV)
	 * and result save to prate
	 */
	tmp0 *= fbdiv;

	/* calcuate (((parent_rate/REFDIV) x FBDIV)/input_rate)
	 * and result save to tmp0
	 * here tmp0 is (POSTDIV1*POSTDIV2)
	 */
	do_div(tmp0, rate);

	/* calculate div1 and div2 value */
	if (tmp0 <= 7) {
		/* (div1 * div2) <= 7, no need to use array search */
		*postdiv1 = tmp0;
		*postdiv2 = 1;
	} else {
		/* (div1 * div2) > 7, use array search */
		for (index = 0; index < array_size(postdiv1_2); index++) {
			if (tmp0 > postdiv1_2[index][POSTDIV_RESULT_INDEX]) {
				continue;
			} else {
				/* found it */
				break;
			}
		}
		if (index < array_size(postdiv1_2)) {
			*postdiv1 = postdiv1_2[index][1];
			*postdiv2 = postdiv1_2[index][0];
		} else {
			sbi_printf("%s out of postdiv array range!\n", __func__);
			ret = -1;
		}
	}

	return ret;
}

/**
 * @name  __set_pll_vcosel
 * @brief Set the frequency range selection bits.
 * @ingroup clk
 * @details
 * The function uses the following mapping for frequency range selection:
 * - 2'd2 (bit[17:16] = 0b10): for frequencies from 1.6 GHz to 2.4 GHz.
 * - 2'd3 (bit[17:16] = 0b11): for frequencies from 2.4 GHz to 3.2 GHz.
 *
 * @param[in] sg2044_pll Pointer to the sg2044_pll_clock structure that represents the PLL control.
 * @param[in] foutvco The target frequency in Hz for which the frequency range needs to be set.
 *
 */
static void __set_pll_vcosel(struct sg2044_pll_clock *sg2044_pll, uint64_t foutvco)
{
	int vcosel;
	uint32_t value;

	if (foutvco < (2400 * MHZ))
		vcosel = 0x2;
	else
		vcosel = 0x3;

	sg2044_pll_read_l(sg2044_pll->id, &value);
	value &= ~(0x3 << 16);
	value |= (vcosel << 16);
	sg2044_pll_write_l(sg2044_pll->id, value);
}

/**
 * @brief Maps a PLL ID to its corresponding shift value in the multiplexer array.
 * @ingroup clk
 * @param[in] id The unique identifier for the PLL whose shift value is being queried.
 *
 * @return int Returns the shift value if the ID is found in the array; otherwise, returns -1.
 */
static inline int sg2044_pll_id2shift(uint32_t id)
{
	for (int i = 0; i < 15; i++) {
		if (sg2044_pll_mux[i][0] == id)
			return sg2044_pll_mux[i][1];
	}

	return -1;
}

/**
 * @brief Switches to the fpll or mpll based on the provided enable flag.
 * @ingroup clk
 *
 * @param[in] pll A pointer to the `sg2044_pll_clock` structure containing the PLL configuration,
 *            including its unique identifier.
 * @param[in] en  A character flag ('1' for enable, '0' for disable) indicating whether to
 *	      Switches to the fpll.
 *
 * @return int Returns 0 on successful modification of the multiplexer. Returns -1 if the shift
 *             value for the PLL ID is not found, indicating an error.
 */
static inline int sg2044_pll_switch_mux(struct sg2044_pll_clock *pll, char en)
{
	uint32_t value;
	uint32_t id = pll->id;
	int shift;

	shift = sg2044_pll_id2shift(id);
	if (shift == -1) {
		sbi_printf("%s Unable to find a suitable shift!\n", __func__);
		return -1;
	}

	top_misc_read(PLL_SELECT_OFFSET, &value);
	if (en)
		top_misc_write(PLL_SELECT_OFFSET, value & (~(1<<shift)));
	else
		top_misc_write(PLL_SELECT_OFFSET, value | (1<<shift));

	return 0;
}

/**
 * @brief Enables or disables a specified MPLL based on the enable flag.
 * @ingroup clk
 *
 * @param[in] pll A pointer to the `sg2044_pll_clock` structure that contains the configuration
 *            and status information of the PLL.
 * @param[in] en  A character flag that specifies the desired state of the PLL:
 *            '1' to enable the PLL, '0' to disable it.
 *
 * @return int Always returns 0. This function does not return error codes; it logs warnings
 *             if the PLL does not lock or update within the expected time.
 */
static inline int sg2044_pll_enable(struct sg2044_pll_clock *pll, char en)
{
	uint32_t value;
	uint64_t wait = 0;
	uint32_t id = pll->id;

	if (en) {
		/* wait pll lock */
		top_misc_read(pll->status_offset, &value);
		while (!((value >> (PLL_STAT_LOCK_OFFSET + id)) & 0x1)) {
			top_misc_read(pll->status_offset, &value);
			wait++;
			sbi_timer_udelay(10);
			if (wait > 10000)
				sbi_printf("%s not locked\n", pll->name);
		}
		/* wait pll updating */
		wait = 0;
		top_misc_read(pll->status_offset, &value);
		while (((value >> id) & 0x1)) {
			top_misc_read(pll->status_offset, &value);
			wait++;
			sbi_timer_udelay(10);
			if (wait > 10000)
				sbi_printf("%s still updating\n", pll->name);
		}
		/* enable pll */
		top_misc_read(pll->enable_offset, &value);
		top_misc_write(pll->enable_offset, value | (1 << id));
	} else {
		/* disable pll */
		top_misc_read(pll->enable_offset, &value);
		top_misc_write(pll->enable_offset, value & (~(1 << id)));
	}

	return 0;
}

/**
 * @name  __get_pll_ctl_setting
 * @ingroup clk
 * @brief Configure the PLL control settings to achieve the requested output rate.
 * @details The function iterates through possible values of REFDIV and FBDIV to calculate the output frequency.
 * It ensures that this frequency is within the specified limits. If the frequency is valid, it then calculates
 * the necessary post-divider settings to get as close as possible to the requested output frequency.
 *
 * The PLL output frequency calculation is based on the formula:
 * FOUTPOSTDIV = FREF * FBDIV / REFDIV / (POSTDIV1+1) * (POSTDIV2+1)
 * where:
 *	- FREF: Reference Clock Input (12MHz to 1600MHz). SG2044 uses 25MHz reference clock.
 *	- FOUTPOSTDIV: Output Clock (25MHz to 3200MHz).
 *	- REFDIV: Reference divide value (1 to 63).
 *	- FBDIV: Feedback divide value (8 to 1066).
 *	- POSTDIV1: Post Divide 1 setting (0 to 7).
 *	- POSTDIV2: Post Divide 2 setting (0 to 7).
 *
 * Additionally, other check points inside PLL are listed here:
 * 1.FOUTVCO = FREF * FBDIV / REFDIV （1600MHz to 3200MHz）
 *	- VCOSEL =2'd2, FOUTVCO 1.6G~2.4G;
 *	- VCOSEL= 2'd3, FOUTVCO 2.4G~3.2G
 * 2.POSTDIV1>=POSTDIV2
 *
 * @param[out] best Pointer to a structure where the best PLL settings are stored.
 * @param[in] req_rate The requested output frequency.
 * @param[in] parent_rate The reference input frequency.
 *
 * @return Returns 0 on success, which includes finding an exact match or the closest possible settings.
 * If no valid configuration is found within the given range, the function returns -1.
 *
 */
static int __get_pll_ctl_setting(struct sg2044_pll_ctrl *best,
			uint64_t req_rate, uint64_t parent_rate)
{
	int ret;
	uint32_t fbdiv, refdiv, fref, postdiv1, postdiv2;
	uint64_t tmp = 0, foutvco;

	fref = parent_rate;

	for (refdiv = REFDIV_MIN; refdiv < REFDIV_MAX + 1; refdiv++) {
		for (fbdiv = FBDIV_MIN; fbdiv < FBDIV_MAX + 1; fbdiv++) {
			foutvco = fref * fbdiv / refdiv;
			/* check fpostdiv pfd */
			if (foutvco < PLL_FREQ_MIN || foutvco > PLL_FREQ_MAX
					|| (fref / refdiv) < 10)
				continue;

			ret = __pll_get_postdiv_1_2(req_rate, fref, fbdiv,
					refdiv, &postdiv1, &postdiv2);
			if (ret)
				continue;

			tmp = foutvco / (postdiv1 * postdiv2);
			if (abs_diff(tmp, req_rate) < abs_diff(best->freq, req_rate)) {
				best->freq = tmp;
				best->refdiv = refdiv;
				best->fbdiv = fbdiv;
				best->postdiv1 = postdiv1;
				best->postdiv2 = postdiv2;
				if (tmp == req_rate)
					return 0;
			}
			continue;
		}
	}

	return -1;
}

/**
 * @name sg2044_clk_pll_set_rate
 * @brief Set the frequency rate of a PLL.
 * @ingroup clk
 * @details
 * This function configures the PLL to operate at a specified frequency rate by calculating
 * and setting the appropriate control values in the PLL's register. The process involves
 * switching to FPLL source, disabling the MPLL, computing the new frequency settings,
 * updating the control register, and re-enabling the MPLL.
 *
 * The function performs the following steps:
 * 1. Switch the PLL source to fpll before modifying settings.
 * 2. Disable the MPLL to allow safe modifications to its configuration.
 * 3. Calculate the new PLL settings based on the desired rate and the parent rate.
 * 5. Set the frequency range based on the foutvco .
 * 6. Write the new settings to the PLL control register.
 * 7. Re-enable the PLL.
 * 8. Switch back the PLL source to mpll after modifications.
 *
 * @param[in] sg2044_pll Pointer to the sg2044_pll_clock structure that represents the PLL control.
 * @param[in] rate The desired frequency rate to set for the PLL in Hz.
 * @param[in] parent_rate The frequency rate of the parent clock in Hz.
 *
 * @return int Returns 0 on success, -1 on failure with an error message indicating the failure reason.
 */
static int sg2044_clk_pll_set_rate(struct sg2044_pll_clock *sg2044_pll, uint64_t rate, uint64_t parent_rate)
{
	int ret = 0;
	uint32_t value;
	uint64_t foutvco;
	struct sg2044_pll_ctrl pctrl_table;

	sbi_memset(&pctrl_table, 0, sizeof(struct sg2044_pll_ctrl));

	/* switch to fpll before modify mpll */
	ret = sg2044_pll_switch_mux(sg2044_pll, 1);
	if (ret == -1) {
		sbi_printf("switch to fpll fail!\n");
		goto out;
	}

	if (sg2044_pll_enable(sg2044_pll, 0)) {
		sbi_printf("Can't disable pll(%s), status error\n", sg2044_pll->name);
		ret = -1;
		goto out;
	}

	sg2044_pll_read_h(sg2044_pll->id, &value);
	__get_pll_ctl_setting(&pctrl_table, rate, parent_rate);
	if (!pctrl_table.freq) {
		sbi_printf("%s: Can't find a proper pll setting\n", sg2044_pll->name);
		ret = -1;
		goto out;
	}

	value = TOP_PLL_CTRL(pctrl_table.fbdiv, pctrl_table.postdiv1,
			     pctrl_table.postdiv2, pctrl_table.refdiv);

	foutvco = parent_rate * pctrl_table.fbdiv / pctrl_table.refdiv;
	__set_pll_vcosel(sg2044_pll, foutvco);

	/* write the value to top register */
	sg2044_pll_write_h(sg2044_pll->id, value);
	sg2044_pll_enable(sg2044_pll, 1);
	/* switch back to mpll after modify mpll */
	ret = sg2044_pll_switch_mux(sg2044_pll, 0);
	if (ret == -1) {
		sbi_printf("switch back to mpll fail!\n");
		goto out;
	}
out:
	return ret;
}

static uint64_t sg2044_clk_pll_get_rate(struct sg2044_pll_clock *sg2044_pll, uint64_t parent_rate)
{
	uint32_t value;
	uint64_t fref;
	int refdiv, fbdiv;
	int postdiv1, postdiv2;

	sg2044_pll_read_h(sg2044_pll->id, &value);
	fref = parent_rate;
	fbdiv = (value >> FBDIV_SHIFT) & div_mask(FBDIV_WIDTH);
	refdiv = (value >> REFDIV_SHIFT) & div_mask(REFDIV_WIDTH);
	postdiv1 = (value >> POSTDIV1_SHIFT) & div_mask(POSTDIV1_WIDTH);
	postdiv2 = (value >> POSTDIV2_SHIFT) & div_mask(POSTDIV2_WIDTH);

	return fref * fbdiv / refdiv / (postdiv1 + 1) * (postdiv2 + 1);
}

static struct sg2044_pll_clock *sg2044_get_clk_by_name(const char *name)
{
	int index;

	for (index = 0; index < array_size(sg2044_root_pll_clks); index++) {
		if (!sbi_strcmp(name, sg2044_root_pll_clks[index].name))
		    return &sg2044_root_pll_clks[index];
	}

	return 0;
}

static int sg2044_clk_set_rate(const char *name, uint64_t rate)
{
	int ret;
	struct sg2044_pll_clock *clk;

	clk = sg2044_get_clk_by_name(name);
	if (!clk)
		return -1;

	ret = sg2044_clk_pll_set_rate(clk, rate, 25 * MHZ);
	if (ret != 0)
		sbi_printf("%s set default rat fail!, ret = %d\n", name, ret);

	return ret;
}

static uint64_t sg2044_clk_get_rate(const char *name)
{
	struct sg2044_pll_clock *clk;

	clk = sg2044_get_clk_by_name(name);
	if (!clk)
		return -1;

	return sg2044_clk_pll_get_rate(clk, 25 * MHZ);
}

static int sg2044_clk_enable(const char *name)
{
	struct sg2044_pll_clock *clk;

	clk = sg2044_get_clk_by_name(name);
	if (!clk)
		return -1;

	sg2044_pll_enable(clk, 1);

	return 0;
}

static int sg2044_clk_disable(const char *name)
{
	struct sg2044_pll_clock *clk;

	clk = sg2044_get_clk_by_name(name);
	if (!clk)
		return -1;

	sg2044_pll_enable(clk, 0);

	return 0;
}

static struct sbi_clk_device sbi_sg2044_clk = {
	.name		= "sg2044-clk",
	.clk_set_rate	= sg2044_clk_set_rate,
	.clk_get_rate	= sg2044_clk_get_rate,
	.clk_enable	= sg2044_clk_enable,
	.clk_disable	= sg2044_clk_disable,
};

static int sg2044_clk_init(const void *fdt, int nodeoff,
			       const struct fdt_match *match)
{
	int top_offset, len;
	const fdt32_t *phandle;
	const fdt32_t *reg;

	if (top_base_addr)
		return 0;

	phandle = fdt_getprop(fdt, nodeoff, "subctrl-syscon", &len);
	if (!phandle)
		return SBI_ENODEV;

	top_offset = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(*phandle));
	if (!top_offset)
		return top_offset;

	reg = fdt_getprop(fdt, top_offset, "reg", &len);
	if (!reg || len <= 0)
		return SBI_EINVAL;

	top_base_addr = ((uint64_t)fdt32_to_cpu(reg[0]) << 32) | (uint64_t)fdt32_to_cpu(reg[1]);

	sbi_clk_set_device(&sbi_sg2044_clk);

	return 0;
}

static const struct fdt_match sg2044_clk_match[] = {
	{ .compatible = "sg2044, pll-clock" },
	{},
};

struct fdt_driver fdt_clk_sg2044 = {
	.match_table = sg2044_clk_match,
	.init = sg2044_clk_init,
};
