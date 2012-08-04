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
    cpu_abort(env, "%s not supported yet\n", __func__);
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
