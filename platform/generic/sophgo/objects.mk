#
# SPDX-License-Identifier: BSD-2-Clause
#

carray-platform_override_modules-$(CONFIG_PLATFORM_SOPHGO_MANGO) += sophgo_mango
platform-objs-$(CONFIG_PLATFORM_SOPHGO_MANGO) += sophgo/mango.o
