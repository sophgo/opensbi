#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (C) 2025 Sophgo Inc
#
# Authors:
#   Haijiao Liu <haijiao.liu@sophgo.com>
#

libsbiutils-objs-$(CONFIG_FDT_CLK) += clk/fdt_clk.o
libsbiutils-objs-$(CONFIG_FDT_CLK) += clk/fdt_clk_drivers.carray.o

carray-fdt_clk_drivers-$(CONFIG_FDT_CLK_SG2044) += fdt_clk_sg2044
libsbiutils-objs-$(CONFIG_FDT_CLK_SG2044) += clk/sg2044_clk.o
