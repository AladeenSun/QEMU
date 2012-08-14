/*
 * UniCore64 virtual CPU header
 *
 * Copyright (C) 2012 Guan Xuetao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or (at your option) any
 * later version. See the COPYING file in the top-level directory.
 */
#ifndef QEMU_UNICORE64_CPU_H
#define QEMU_UNICORE64_CPU_H

#define TARGET_LONG_BITS                64
#define TARGET_PAGE_BITS                12

#define TARGET_PHYS_ADDR_SPACE_BITS     64
#define TARGET_VIRT_ADDR_SPACE_BITS     64

#define ELF_MACHINE                     EM_UNICORE64

#define CPUArchState                    struct CPUUniCore64State

#define NB_MMU_MODES                     2

#include "config.h"
#include "qemu-common.h"
#include "cpu-defs.h"
#include "softfloat.h"

typedef struct CPUUniCore64State {
    /* Regs for current mode.  */
    uint64_t regs[32];
    /* Frequently accessed ASR bits are stored separately for efficiently.
       This contains all the other bits.  Use asr_{read,write} to access
       the whole ASR.  */
    uint64_t uncached_asr;
    uint64_t uncached_afr;
    uint64_t bsr;
    uint64_t bfr;

    /* Banked registers. */
    uint64_t banked_r29[3];
    uint64_t banked_r30[3];
    uint64_t banked_bsr[3];
    uint64_t banked_bfr[3];

    /* AFR cache for faster execution */
    uint64_t VF; /* V is the bit 31. All other bits are undefined */
    uint64_t CF; /* 0 or 1 */
    uint64_t ZF; /* Z set if zero.  */
    uint64_t NF; /* N is bit 31. All other bits are undefined.  */

    /* Addr for LL-SC */
    uint64_t exclusive_addr_ll;

    /* System control coprocessor (cp0) */
    struct {
        uint64_t c0_cpuid;
        uint64_t c1_sys; /* System control register.  */
        uint64_t c2_base; /* MMU translation table base.  */
        uint64_t c3_faultstatus; /* Fault status registers.  */
        uint64_t c4_faultaddr; /* Fault address registers.  */
        uint64_t c4_itrapaddr; /* Exception address register.*/
        uint64_t c6_dcache; /* Dcache management register */
        uint64_t c7_icache; /* Icache management register */
        uint64_t c9_excpbase; /* Exception base register. */
        uint64_t c12_sysu[8]; /* SysU registers. */
    } cp0;

    CPU_COMMON

} CPUUniCore64State;

#define ASR_IDX_USER            (0)
#define ASR_IDX_PRIV            (1)
#define ASR_IDX_DEBUG           (2)
#define ASR_MODE_SELECT         (ASR_MODE_USER | ASR_MODE_PRIV | ASR_MODE_DEBUG)
#define ASR_MODE_USER           (1 << ASR_IDX_USER)
#define ASR_MODE_PRIV           (1 << ASR_IDX_PRIV)
#define ASR_MODE_DEBUG          (1 << ASR_IDX_DEBUG)
#define ASR_INTR_SELECT         (0x3f << 5)
#define AFR_V                   (1 << 0)
#define AFR_C                   (1 << 1)
#define AFR_Z                   (1 << 2)
#define AFR_N                   (1 << 3)

#define UC64_EXCP_PRIV          0x08
#define UC64_EXCP_ITRAP         0x0c
#define UC64_EXCP_DTRAP         0x10
#define UC64_INT_TIMER          0x20

/* Return the current ASR value.  */
target_ulong cpu_asr_read(CPUUniCore64State *env1);
/* Set the ASR.  Note that some bits of mask must be all-set or all-clear.  */
void cpu_asr_write(CPUUniCore64State *env, target_ulong val, target_ulong msk);

#define cpu_init                        uc64_cpu_init
#define cpu_exec                        uc64_cpu_exec
#define cpu_signal_handler              uc64_cpu_signal_handler
#define cpu_handle_mmu_fault            uc64_cpu_handle_mmu_fault

CPUUniCore64State *uc64_cpu_init(const char *cpu_model);
int uc64_cpu_exec(CPUUniCore64State *s);
int uc64_cpu_signal_handler(int host_signum, void *pinfo, void *puc);
int uc64_cpu_handle_mmu_fault(CPUUniCore64State *env, target_ulong address,
        int rw, int mmu_idx);

void uc64_translate_init(void);
void do_interrupt(CPUUniCore64State *);
void switch_mode(CPUUniCore64State *, int);

#define CPU_SAVE_VERSION       2

/* MMU modes definitions */
#define MMU_MODE0_SUFFIX       _kernel
#define MMU_MODE1_SUFFIX       _user
#define MMU_USER_IDX           1

static inline int cpu_mmu_index(CPUUniCore64State *env)
{
    return (env->uncached_asr & ASR_MODE_SELECT) == ASR_MODE_USER ? 1 : 0;
}

static inline void cpu_clone_regs(CPUUniCore64State *env, target_ulong newsp)
{
    if (newsp) {
        env->regs[29] = newsp;
    }
    env->regs[0] = 0;
}

static inline void cpu_set_tls(CPUUniCore64State *env, target_ulong newtls)
{
    abort();
}

#include "cpu-all.h"
#include "cpu-qom.h"
#include "exec-all.h"

static inline void cpu_pc_from_tb(CPUUniCore64State *env, TranslationBlock *tb)
{
    env->regs[31] = tb->pc;
}

static inline void cpu_get_tb_cpu_state(CPUUniCore64State *env,
        target_ulong *pc, target_ulong *cs_base, int *flags)
{
    *pc = env->regs[31];
    *cs_base = 0;
    *flags = 0;
    if ((env->uncached_asr & ASR_MODE_SELECT) != ASR_MODE_USER) {
        *flags |= (1 << 6);
    }
}

static inline bool cpu_has_work(CPUUniCore64State *env)
{
    return env->interrupt_request &
        (CPU_INTERRUPT_HARD | CPU_INTERRUPT_EXITTB);
}

#endif /* QEMU_UNICORE64_CPU_H */
