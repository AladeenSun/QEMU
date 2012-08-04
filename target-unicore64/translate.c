/*
 *  UniCore64 translation
 *
 * Copyright (C) 2012 Guan Xuetao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or (at your option) any
 * later version. See the COPYING file in the top-level directory.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "cpu.h"
#include "disas.h"
#include "tcg-op.h"
#include "qemu-log.h"

#include "helper.h"
#define GEN_HELPER 1
#include "helper.h"

static TCGv_ptr cpu_env;
static TCGv_i64 cpu_R[32];

#include "gen-icount.h"

static const char *regnames[] = {
      "r00", "r01", "r02", "r03", "r04", "r05", "r06", "r07",
      "r08", "r09", "r10", "r11", "r12", "r13", "r14", "r15",
      "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
      "r24", "r25", "r26", "r27", "r28", "r29", "r30", "pc" };

/* initialize TCG globals.  */
void uc64_translate_init(void)
{
    int i;

    cpu_env = tcg_global_reg_new_ptr(TCG_AREG0, "env");

    for (i = 0; i < 32; i++) {
        cpu_R[i] = tcg_global_mem_new_i64(TCG_AREG0,
                           offsetof(CPUUniCore64State, regs[i]), regnames[i]);
    }

#define GEN_HELPER 2
#include "helper.h"
}

/* generate intermediate code in gen_opc_buf and gen_opparam_buf for
   basic block 'tb'. If search_pc is TRUE, also generate PC
   information for each intermediate instruction. */
static inline void gen_intermediate_code_internal(CPUUniCore64State *env,
        TranslationBlock *tb, int search_pc)
{
    int num_insns = 0;

    gen_icount_start();
    gen_icount_end(tb, num_insns);
    abort();
}

void gen_intermediate_code(CPUUniCore64State *env, TranslationBlock *tb)
{
    gen_intermediate_code_internal(env, tb, 0);
}

void gen_intermediate_code_pc(CPUUniCore64State *env, TranslationBlock *tb)
{
    gen_intermediate_code_internal(env, tb, 1);
}

void cpu_dump_state(CPUUniCore64State *env, FILE *f,
        fprintf_function cpu_fprintf, int flags)
{
    abort();
}

void restore_state_to_opc(CPUUniCore64State *env, TranslationBlock *tb,
        int pc_pos)
{
    env->regs[31] = gen_opc_pc[pc_pos];
}
