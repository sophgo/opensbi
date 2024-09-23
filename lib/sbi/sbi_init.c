/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_atomic.h>
#include <sbi/riscv_barrier.h>
#include <sbi/riscv_locks.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_ecall.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_hartmask.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_timer.h>
#include <sbi/sbi_tlb.h>
#include <sbi/sbi_version.h>

#define BANNER                                              \
	"   ____                    _____ ____ _____\n"     \
	"  / __ \\                  / ____|  _ \\_   _|\n"  \
	" | |  | |_ __   ___ _ __ | (___ | |_) || |\n"      \
	" | |  | | '_ \\ / _ \\ '_ \\ \\___ \\|  _ < | |\n" \
	" | |__| | |_) |  __/ | | |____) | |_) || |_\n"     \
	"  \\____/| .__/ \\___|_| |_|_____/|____/_____|\n"  \
	"        | |\n"                                     \
	"        |_|\n\n"


static spinlock_t coldboot_lock = SPIN_LOCK_INITIALIZER;
static struct sbi_hartmask coldboot_wait_hmask = { 0 };

static unsigned long coldboot_done;

static void wait_for_coldboot(struct sbi_scratch *scratch, u32 hartid)
{
	unsigned long saved_mie, cmip;

	/* Save MIE CSR */
	saved_mie = csr_read(CSR_MIE);

	/* Set MSIE bit to receive IPI */
	csr_set(CSR_MIE, MIP_MSIP);

	/* Acquire coldboot lock */
	spin_lock(&coldboot_lock);

	/* Mark current HART as waiting */
	sbi_hartmask_set_hart(hartid, &coldboot_wait_hmask);

	/* Release coldboot lock */
	spin_unlock(&coldboot_lock);

	/* Wait for coldboot to finish using WFI */
	while (!__smp_load_acquire(&coldboot_done)) {
		do {
			wfi();
			cmip = csr_read(CSR_MIP);
		 } while (!(cmip & MIP_MSIP));
	};

	/* Acquire coldboot lock */
	spin_lock(&coldboot_lock);

	/* Unmark current HART as waiting */
	sbi_hartmask_clear_hart(hartid, &coldboot_wait_hmask);

	/* Release coldboot lock */
	spin_unlock(&coldboot_lock);

	/* Restore MIE CSR */
	csr_write(CSR_MIE, saved_mie);

	/*
	 * The wait for coldboot is common for both warm startup and
	 * warm resume path so clearing IPI here would result in losing
	 * an IPI in warm resume path.
	 *
	 * Also, the sbi_platform_ipi_init() called from sbi_ipi_init()
	 * will automatically clear IPI for current HART.
	 */
}

static void wake_coldboot_harts(struct sbi_scratch *scratch, u32 hartid)
{
	/* Mark coldboot done */
	__smp_store_release(&coldboot_done, 1);

	/* Acquire coldboot lock */
	spin_lock(&coldboot_lock);

	/* Send an IPI to all HARTs waiting for coldboot */
	for (int i = 0; i <= sbi_scratch_last_hartid(); i++) {
		if ((i != hartid) &&
		    sbi_hartmask_test_hart(i, &coldboot_wait_hmask))
			sbi_ipi_raw_send(i);
	}

	/* Release coldboot lock */
	spin_unlock(&coldboot_lock);
}

static unsigned long init_count_offset;

#ifdef CONFIG_SKIP_UBOOT
extern void generic_fdt_fixup_chosen(void);
#endif
static void __noreturn init_coldboot(struct sbi_scratch *scratch, u32 hartid)
{
	int rc;
	unsigned long *init_count;
	// const struct sbi_platform *plat = sbi_platform_ptr(scratch);

	/* Note: This has to be first thing in coldboot init sequence */
	rc = sbi_scratch_init(scratch);
	if (rc)
		sbi_hart_hang();

	/* Note: This has to be second thing in coldboot init sequence */
	rc = sbi_domain_init(scratch, hartid);
	if (rc)
		sbi_hart_hang();

	init_count_offset = sbi_scratch_alloc_offset(__SIZEOF_POINTER__,
						     "INIT_COUNT");
	if (!init_count_offset)
		sbi_hart_hang();

	rc = sbi_hsm_init(scratch, hartid, TRUE);
	if (rc)
		sbi_hart_hang();

	// rc = sbi_platform_early_init(plat, TRUE);
	// if (rc)
	// 	sbi_hart_hang();

	rc = sbi_hart_init(scratch, TRUE);
	if (rc)
		sbi_hart_hang();

#if defined(CONFIG_SKIP_UBOOT_DEBUG)
	rc = sbi_console_init(scratch);
	if (rc)
		sbi_hart_hang();
#endif

	// rc = sbi_platform_irqchip_init(plat, TRUE);
	// if (rc) {
	// 	sbi_printf("%s: platform irqchip init failed (error %d)\n",
	// 		   __func__, rc);
	// 	sbi_hart_hang();
	// }

	// rc = sbi_ipi_init(scratch, TRUE);
	// if (rc) {
	// 	sbi_printf("%s: ipi init failed (error %d)\n", __func__, rc);
	// 	sbi_hart_hang();
	// }

	rc = sbi_tlb_init(scratch, TRUE);
	if (rc) {
		sbi_printf("%s: tlb init failed (error %d)\n", __func__, rc);
		sbi_hart_hang();
	}

	rc = sbi_timer_init(scratch, TRUE);
	if (rc) {
		sbi_printf("%s: timer init failed (error %d)\n", __func__, rc);
		sbi_hart_hang();
	}

	rc = sbi_ecall_init();
	if (rc) {
		sbi_printf("%s: ecall init failed (error %d)\n", __func__, rc);
		sbi_hart_hang();
	}

	/*
	 * Note: Finalize domains after HSM initialization so that we
	 * can startup non-root domains.
	 * Note: Finalize domains before HART PMP configuration so
	 * that we use correct domain for configuring PMP.
	 */
	rc = sbi_domain_finalize(scratch, hartid);
	if (rc) {
		sbi_printf("%s: domain finalize failed (error %d)\n",
			   __func__, rc);
		sbi_hart_hang();
	}

	rc = sbi_hart_pmp_configure(scratch);
	if (rc) {
		sbi_printf("%s: PMP configure failed (error %d)\n",
			   __func__, rc);
		sbi_hart_hang();
	}

	/*
	 * Note: Platform final initialization should be last so that
	 * it sees correct domain assignment and PMP configuration.
	 */
	// rc = sbi_platform_final_init(plat, TRUE);
	// if (rc) {
	// 	sbi_printf("%s: platform final init failed (error %d)\n",
	// 		   __func__, rc);
	// 	sbi_hart_hang();
	// }
#ifdef CONFIG_SKIP_UBOOT
	generic_fdt_fixup_chosen();
#endif

	wake_coldboot_harts(scratch, hartid);

	init_count = sbi_scratch_offset_ptr(scratch, init_count_offset);
	(*init_count)++;

	sbi_hsm_prepare_next_jump(scratch, hartid);
	sbi_hart_switch_mode(hartid, scratch->next_arg1, scratch->next_addr,
			     scratch->next_mode, FALSE);
}

static void init_warm_startup(struct sbi_scratch *scratch, u32 hartid)
{
	int rc;
	unsigned long *init_count;
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);

	if (!init_count_offset)
		sbi_hart_hang();

	rc = sbi_hsm_init(scratch, hartid, FALSE);
	if (rc)
		sbi_hart_hang();

	rc = sbi_platform_early_init(plat, FALSE);
	if (rc)
		sbi_hart_hang();

	rc = sbi_hart_init(scratch, FALSE);
	if (rc)
		sbi_hart_hang();

	rc = sbi_platform_irqchip_init(plat, FALSE);
	if (rc)
		sbi_hart_hang();

	rc = sbi_ipi_init(scratch, FALSE);
	if (rc)
		sbi_hart_hang();

	rc = sbi_tlb_init(scratch, FALSE);
	if (rc)
		sbi_hart_hang();

	rc = sbi_timer_init(scratch, FALSE);
	if (rc)
		sbi_hart_hang();

	rc = sbi_hart_pmp_configure(scratch);
	if (rc)
		sbi_hart_hang();

	rc = sbi_platform_final_init(plat, FALSE);
	if (rc)
		sbi_hart_hang();

	init_count = sbi_scratch_offset_ptr(scratch, init_count_offset);
	(*init_count)++;

	sbi_hsm_prepare_next_jump(scratch, hartid);
}

static void init_warm_resume(struct sbi_scratch *scratch)
{
	int rc;

	sbi_hsm_hart_resume_start(scratch);

	rc = sbi_hart_reinit(scratch);
	if (rc)
		sbi_hart_hang();

	rc = sbi_hart_pmp_configure(scratch);
	if (rc)
		sbi_hart_hang();

	sbi_hsm_hart_resume_finish(scratch);
}

static void __noreturn init_warmboot(struct sbi_scratch *scratch, u32 hartid)
{
	int hstate;

	wait_for_coldboot(scratch, hartid);

	hstate = sbi_hsm_hart_get_state(sbi_domain_thishart_ptr(), hartid);
	if (hstate < 0)
		sbi_hart_hang();

	if (hstate == SBI_HSM_STATE_SUSPENDED)
		init_warm_resume(scratch);
	else
		init_warm_startup(scratch, hartid);

	sbi_hart_switch_mode(hartid, scratch->next_arg1,
			     scratch->next_addr,
			     scratch->next_mode, FALSE);
}

static atomic_t coldboot_lottery = ATOMIC_INITIALIZER(0);

/**
 * Initialize OpenSBI library for current HART and jump to next
 * booting stage.
 *
 * The function expects following:
 * 1. The 'mscratch' CSR is pointing to sbi_scratch of current HART
 * 2. Stack pointer (SP) is setup for current HART
 * 3. Interrupts are disabled in MSTATUS CSR
 * 4. All interrupts are disabled in MIE CSR
 *
 * @param scratch pointer to sbi_scratch of current HART
 */
void __noreturn sbi_init(struct sbi_scratch *scratch)
{
	bool next_mode_supported	= FALSE;
	bool coldboot			= FALSE;
	u32 hartid			= current_hartid();
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);

	if ((SBI_HARTMASK_MAX_BITS <= hartid) ||
	    sbi_platform_hart_invalid(plat, hartid))
		sbi_hart_hang();

	switch (scratch->next_mode) {
	case PRV_M:
		next_mode_supported = TRUE;
		break;
	case PRV_S:
		if (misa_extension('S'))
			next_mode_supported = TRUE;
		break;
	case PRV_U:
		if (misa_extension('U'))
			next_mode_supported = TRUE;
		break;
	default:
		sbi_hart_hang();
	}

	/*
	 * Only the HART supporting privilege mode specified in the
	 * scratch->next_mode should be allowed to become the coldboot
	 * HART because the coldboot HART will be directly jumping to
	 * the next booting stage.
	 *
	 * We use a lottery mechanism to select coldboot HART among
	 * HARTs which satisfy above condition.
	 */

	if (next_mode_supported && atomic_xchg(&coldboot_lottery, 1) == 0)
		coldboot = TRUE;

	if (coldboot)
		init_coldboot(scratch, hartid);
	else
		init_warmboot(scratch, hartid);
}

unsigned long sbi_init_count(u32 hartid)
{
	struct sbi_scratch *scratch;
	unsigned long *init_count;

	if (!init_count_offset)
		return 0;

	scratch = sbi_hartid_to_scratch(hartid);
	if (!scratch)
		return 0;

	init_count = sbi_scratch_offset_ptr(scratch, init_count_offset);

	return *init_count;
}

/**
 * Exit OpenSBI library for current HART and stop HART
 *
 * The function expects following:
 * 1. The 'mscratch' CSR is pointing to sbi_scratch of current HART
 * 2. Stack pointer (SP) is setup for current HART
 *
 * @param scratch pointer to sbi_scratch of current HART
 */
void __noreturn sbi_exit(struct sbi_scratch *scratch)
{
	u32 hartid			= current_hartid();
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);

	if (sbi_platform_hart_invalid(plat, hartid))
		sbi_hart_hang();

	sbi_platform_early_exit(plat);

	sbi_timer_exit(scratch);

	sbi_ipi_exit(scratch);

	sbi_platform_irqchip_exit(plat);

	sbi_platform_final_exit(plat);

	sbi_hsm_exit(scratch);
}
