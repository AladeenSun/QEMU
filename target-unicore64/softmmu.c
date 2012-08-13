/*
 * Softmmu related functions
 *
 * Copyright (C) 2012 Guan Xuetao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or any later version.
 * See the COPYING file in the top-level directory.
 */
#include "cpu.h"
#include "dyngen-exec.h"
#include "helper.h"

#ifdef CONFIG_USER_ONLY
#error This file only exist under softmmu circumstance
#endif

#define MMUSUFFIX _mmu

#define SHIFT 0
#include "softmmu_template.h"

#define SHIFT 1
#include "softmmu_template.h"

#define SHIFT 2
#include "softmmu_template.h"

#define SHIFT 3
#include "softmmu_template.h"

#undef DEBUG_UC64

#ifdef DEBUG_UC64
#define DPRINTF(fmt, ...) printf("%s: " fmt , __func__, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

void tlb_fill(CPUUniCore64State *env, target_ulong addr, int is_write,
        int mmu_idx, uintptr_t retaddr)
{
    cpu_abort(env, "%s not supported yet\n", __func__);
}

void switch_mode(CPUUniCore64State *env, int mode)
{
    cpu_abort(env, "%s not supported yet\n", __func__);
}

void do_interrupt(CPUUniCore64State *env)
{
    uint64_t addr;
    int new_mode;

    switch (env->exception_index) {
    case UC64_EXCP_PRIV:
        new_mode = ASR_MODE_PRIV;
        addr = UC64_EXCP_PRIV;
        break;
    case UC64_EXCP_ITRAP:
        DPRINTF("itrap happend at %x\n", env->regs[31]);
        new_mode = ASR_MODE_PRIV;
        addr = UC64_EXCP_ITRAP;
        break;
    case UC64_EXCP_DTRAP:
        DPRINTF("dtrap happend at %x\n", env->regs[31]);
        new_mode = ASR_MODE_PRIV;
        addr = UC64_EXCP_DTRAP;
        break;
    case UC64_EXCP_INTR:
    default:
        cpu_abort(env, "Unhandled exception 0x%x\n", env->exception_index);
        return;
    }
    /* Get exception virtual base address , only least 39 bits available */
    addr += (env->cp0.c9_excpaddr & 0x7fffffffffULL);

    env->uncached_asr = (env->uncached_asr & ~ASR_MODE_SELECT) | new_mode;
    env->uncached_asr |= ASR_INTR_SELECT;
    /* the PC already points to the proper instruction. */
    env->regs[30] = env->regs[31];
    env->regs[31] = addr;
    env->interrupt_request |= CPU_INTERRUPT_EXITTB;
}

int uc64_cpu_handle_mmu_fault(CPUUniCore64State *env, target_ulong address,
                              int access_type, int mmu_idx)
{
    cpu_abort(env, "%s not supported yet\n", __func__);
    return 1;
}

target_phys_addr_t cpu_get_phys_page_debug(CPUUniCore64State *env,
        target_ulong addr)
{
    cpu_abort(env, "%s not supported yet\n", __func__);
    return addr;
}
