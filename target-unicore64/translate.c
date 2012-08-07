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

#define UNHANDLED_FLOW  cpu_abort(env,                                  \
                        "Unhandled control flow at line %d in %s!",     \
                        __LINE__, __FILE__)

/* internal defines */
typedef struct DisasContext {
    int dc_jmp;
    int dc_singlestep;
    target_ulong dc_pc;
    struct TranslationBlock *dc_tb;
} DisasContext;

static inline void gen_goto_tb(DisasContext *s, int n, target_ulong dest)
{
    TranslationBlock *tb;

    tb = s->dc_tb;
    if ((tb->pc & TARGET_PAGE_MASK) == (dest & TARGET_PAGE_MASK)) {
        tcg_gen_goto_tb(n);
        tcg_gen_movi_i64(cpu_R[31], dest);
        tcg_gen_exit_tb((tcg_target_long)tb + n);
    } else {
        tcg_gen_movi_i64(cpu_R[31], dest);
        tcg_gen_exit_tb(0);
    }
}

static void do_datap(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
}

static void do_srfr(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
}

static void do_muldiv(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
}

static void do_ldst(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
}

static void do_branch(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
}

static void do_coproc(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
}

static void disas_uc64_insn(CPUUniCore64State *env, DisasContext *s)
{
    unsigned int insn;

    insn = ldl_code(s->dc_pc);
    s->dc_pc += 4;

    /* UniCore64 instructions class:
     *   AAAx xxxx xxxx xxxx xxxx xxxx xxxx xxxx
     */
    switch (insn >> 29) {
    case 0x0:
        do_datap(env, s, insn);
        break;
    case 0x1:
        do_srfr(env, s, insn);
        break;
    case 0x2:
        do_muldiv(env, s, insn);
        break;
    case 0x3:
        /* Fall through */
    case 0x4:
        do_ldst(env, s, insn);
        break;
    case 0x5:
        do_branch(env, s, insn);
        break;
    case 0x6:
        do_coproc(env, s, insn);
        break;
    case 0x7:
        do_branch(env, s, insn);
        /* All conditions are handled, so default is not necessary */
    }
}

/* generate intermediate code in gen_opc_buf and gen_opparam_buf for
   basic block 'tb'. If search_pc is TRUE, also generate PC
   information for each intermediate instruction. */
static inline void gen_intermediate_code_internal(CPUUniCore64State *env,
        TranslationBlock *tb, int search_pc)
{
    DisasContext dc1, *dc = &dc1;
    target_ulong pc_start;
    int num_insns;
    int max_insns;

    pc_start = tb->pc;

    dc->dc_tb = tb;
    dc->dc_pc = pc_start;
    dc->dc_jmp = DISAS_NEXT;
    dc->dc_singlestep = env->singlestep_enabled;

    num_insns = 0;
    max_insns = tb->cflags & CF_COUNT_MASK;
    if (max_insns == 0) {
        max_insns = CF_COUNT_MASK;
    }

    gen_icount_start();
    do {
        if (unlikely(!QTAILQ_EMPTY(&env->breakpoints))) {
            UNHANDLED_FLOW;
        }

        if (search_pc) {
            UNHANDLED_FLOW;
        }

        if (tb->cflags & CF_LAST_IO) {
            UNHANDLED_FLOW;
        }

        disas_uc64_insn(env, dc);

        /* Translation stops when a conditional branch is encountered.
         * Otherwise the subsequent code could get translated several times.
         * Also stop translation when a page boundary is reached.  This
         * ensures prefetch aborts occur at the right place.  */
        num_insns++;
        if (num_insns >= max_insns) {
            goto done_disas_loop;
        }

        if (singlestep || env->singlestep_enabled) {
            goto done_disas_loop;
        }

        if (gen_opc_ptr >= gen_opc_buf + OPC_MAX_SIZE) {
            goto done_disas_loop;
        }

        if (dc->dc_pc >= (pc_start & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE) {
            goto done_disas_loop;
        }

    } while (dc->dc_jmp != DISAS_NEXT);

done_disas_loop:
    if (unlikely(env->singlestep_enabled)) {
        UNHANDLED_FLOW;
    } else {
        /* While branches must always occur at the end of an IT block,
         * there are a few other things that can cause us to terminate
         * the TB in the middel of an IT block:
         *  - Exception generating instructions (bkpt, swi, undefined).
         *  - Page boundaries.
         *  - Hardware watchpoints.
         * Hardware breakpoints have already been handled and skip this code.
         */
        switch (dc->dc_jmp) {
        case DISAS_NEXT:
            gen_goto_tb(dc, 1, dc->dc_pc);
            break;
        default:
            UNHANDLED_FLOW;
        }
    }

    gen_icount_end(tb, num_insns);
    *gen_opc_ptr = INDEX_op_end;

    if (search_pc) {
        UNHANDLED_FLOW;
    } else {
        tb->size = dc->dc_pc - pc_start;
        tb->icount = num_insns;
    }
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
    int i;

    for (i = 0; i < 32; i++) {
        cpu_fprintf(f, "R%02d=%16" PRIx64, i, env->regs[i]);
        if ((i % 4) == 3) {
            cpu_fprintf(f, "\n");
        } else {
            cpu_fprintf(f, " ");
        }
    }
}

void restore_state_to_opc(CPUUniCore64State *env, TranslationBlock *tb,
        int pc_pos)
{
    env->regs[31] = gen_opc_pc[pc_pos];
}
