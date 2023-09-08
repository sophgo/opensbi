/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *   Inochi Amaoto <inochiama@outlook.com>
 *
 */

#include <sbi/riscv_elf.h>

	.section .entry, "ax", %progbits
	.align 3
	.globl thead_patch_sfence
thead_patch_sfence:
	.option push
	.option norvc
	.option norelax

	sfence.vma zero, t0

	.option pop
.equ _thead_patch_sfence_size, . - thead_patch_sfence

	.section .rodata
	.align 3
	.globl thead_patch_sfence_size
thead_patch_sfence_size:
	RISCV_INT	_thead_patch_sfence_size
