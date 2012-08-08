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

#define ILLEGAL_INSN(cond)                                                   \
        if (cond) {                                                          \
            cpu_abort(env, "Illegal UniCore64 instruction %08x at line %d!", \
                insn, __LINE__);                                             \
        }

#define UNHANDLED_FLOW(cond)                                                 \
        if (cond) {                                                          \
            cpu_abort(env, "Unhandled control flow at line %d in %s!",       \
                __LINE__, __FILE__);                                         \
        }

#define UCOP_REG_D              (((insn) >> 16) & 0x1f)
#define UCOP_REG_S1             (((insn) >> 11) & 0x1f)
#define UCOP_REG_S2             (((insn) >>  6) & 0x1f)
#define UCOP_OPCODE             (((insn) >> 24) & 0x0f)
#define UCOP_SHIFT              (((insn) >> 24) & 0x03)
#define UCOP_IMM11              (((insn) >>  0) & 0x7ff)
#define UCOP_IMM_6              (((insn) >>  0) & 0x3f)
#define UCOP_CPNUM              (((insn) >> 21) & 0xf)
#define UCOP_LDST_BHD           (((insn) >> 22) & 0x7)

#define UCOP_SET(i)             ((insn) & (1 << (i)))

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

static void do_shift(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_op1_64, t_op2_64;
    TCGv_i32 t_op1_32, t_op2_32;

    ILLEGAL_INSN(UCOP_SET(23));
    ILLEGAL_INSN(UCOP_REG_D == 31);

    /* Prepare op1 */
    ILLEGAL_INSN(UCOP_REG_S1 == 31);

    t_op1_64 = tcg_temp_new_i64();
    tcg_gen_mov_i64(t_op1_64, cpu_R[UCOP_REG_S1]);

    /* Prepare op2 */
    t_op2_64 = tcg_temp_new_i64();
    if (UCOP_SET(21)) { /* reg or imm */
        ILLEGAL_INSN(!UCOP_SET(22) && UCOP_SET(5)); /* bits_per_word = 32 */

        tcg_gen_movi_i64(t_op2_64, UCOP_IMM_6);
    } else {
        ILLEGAL_INSN(UCOP_REG_S2 == 31);
        ILLEGAL_INSN(UCOP_IMM_6);

        tcg_gen_mov_i64(t_op2_64, cpu_R[UCOP_REG_S2]);
    }

    if (!UCOP_SET(22)) { /* If word, not Double word */
        t_op1_32 = tcg_temp_new_i32();
        t_op2_32 = tcg_temp_new_i32();

        tcg_gen_trunc_i64_i32(t_op1_32, t_op1_64);
        tcg_gen_trunc_i64_i32(t_op2_32, t_op2_64);
    }

    switch (UCOP_SHIFT) {
    case 0x00: /* insn DLSL LSL */
        if (UCOP_SET(22)) { /* insn DLSL */
            tcg_gen_shl_i64(t_op1_64, t_op1_64, t_op2_64);
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_op1_64);
        } else { /* insn LSL */
            tcg_gen_shl_i32(t_op1_32, t_op1_32, t_op2_32);
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op1_32);
        }
        break;
    case 0x01: /* insn DLSR LSR */
        if (UCOP_SET(22)) { /* insn DLSR */
            tcg_gen_shr_i64(t_op1_64, t_op1_64, t_op2_64);
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_op1_64);
        } else { /* insn LSR */
            tcg_gen_shr_i32(t_op1_32, t_op1_32, t_op2_32);
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op1_32);
        }
        break;
    case 0x02: /* insn DASR ASR */
        if (UCOP_SET(22)) { /* insn DASR */
            tcg_gen_sar_i64(t_op1_64, t_op1_64, t_op2_64);
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_op1_64);
        } else { /* insn ASR */
            tcg_gen_sar_i32(t_op1_32, t_op1_32, t_op2_32);
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op1_32);
        }
        break;
    case 0x03:
        ILLEGAL_INSN(true);
    }

    /* Free temp var */
    if (!UCOP_SET(22)) { /* If word, not Double word */
        tcg_temp_free_i32(t_op1_32);
        tcg_temp_free_i32(t_op2_32);
    }
    tcg_temp_free_i64(t_op1_64);
    tcg_temp_free_i64(t_op2_64);
}

static void do_clzclo(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    ILLEGAL_INSN(true);
}

static void do_condmove(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    ILLEGAL_INSN(true);
}

static void do_datap(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_op1_64, t_op2_64;
    TCGv_i32 t_op1_32, t_op2_32;

    ILLEGAL_INSN(UCOP_SET(23)); /* S bit */
    ILLEGAL_INSN(UCOP_REG_D == 31);

    /* Prepare op1 if two operands */
    if ((UCOP_OPCODE != 0x0d) && (UCOP_OPCODE != 0x0f)) {
        ILLEGAL_INSN(UCOP_REG_S1 == 31);

        t_op1_64 = tcg_temp_new_i64();
        tcg_gen_mov_i64(t_op1_64, cpu_R[UCOP_REG_S1]);

        if (!UCOP_SET(22)) { /* If word, not Double word */
            t_op1_32 = tcg_temp_new_i32();
            tcg_gen_trunc_i64_i32(t_op1_32, t_op1_64);
        }
    }

    /* Prepare op2 */
    t_op2_64 = tcg_temp_new_i64();
    if (UCOP_SET(21)) { /* reg or imm */
        tcg_gen_movi_i64(t_op2_64, UCOP_IMM11);
    } else {
        ILLEGAL_INSN(UCOP_REG_S2 == 31);
        ILLEGAL_INSN(UCOP_IMM_6);

        tcg_gen_mov_i64(t_op2_64, cpu_R[UCOP_REG_S2]);
    }

    if (!UCOP_SET(22)) { /* If word, not Double word */
        t_op2_32 = tcg_temp_new_i32();
        tcg_gen_trunc_i64_i32(t_op2_32, t_op2_64);
    }

    switch (UCOP_OPCODE) {
    case 0x00: /* insn AND DAND */
        if (UCOP_SET(22)) { /* insn DAND */
            tcg_gen_and_i64(t_op1_64, t_op1_64, t_op2_64);
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_op1_64);
        } else { /* insn AND */
            tcg_gen_and_i32(t_op1_32, t_op1_32, t_op2_32);
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op1_32);
            tcg_temp_free_i32(t_op1_32);
            tcg_temp_free_i32(t_op2_32);
        }
        tcg_temp_free_i64(t_op1_64);
        tcg_temp_free_i64(t_op2_64);
        break;
    case 0x04: /* insn ADD DADD */
        if (UCOP_SET(22)) { /* insn DADD */
            tcg_gen_add_i64(t_op1_64, t_op1_64, t_op2_64);
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_op1_64);
        } else { /* insn ADD */
            tcg_gen_add_i32(t_op1_32, t_op1_32, t_op2_32);
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op1_32);
            tcg_temp_free_i32(t_op1_32);
            tcg_temp_free_i32(t_op2_32);
        }
        tcg_temp_free_i64(t_op1_64);
        tcg_temp_free_i64(t_op2_64);
        break;
    case 0x0d: /* insn MOV DMOV */
        ILLEGAL_INSN(UCOP_REG_S1);

        if (UCOP_SET(22)) { /* insn DMOV */
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_op2_64);
        } else { /* insn MOV */
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op2_32);
            tcg_temp_free_i32(t_op2_32);
        }
        tcg_temp_free_i64(t_op2_64);
        break;
    case 0x0f: /* insn NOT DNOT */
        ILLEGAL_INSN(UCOP_REG_S1);

        if (UCOP_SET(22)) { /* insn DNOT */
            tcg_gen_not_i64(cpu_R[UCOP_REG_D], t_op2_64);
        } else { /* insn NOT */
            tcg_gen_not_i32(t_op2_32, t_op2_32);
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op2_32);
            tcg_temp_free_i32(t_op2_32);
        }
        tcg_temp_free_i64(t_op2_64);
        break;
    default:
        ILLEGAL_INSN(true);
    }
}

static void do_srfr(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    ILLEGAL_INSN(true);
}

static void do_muldiv(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    ILLEGAL_INSN(true);
}

static void do_ldst(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_addr, t_op2_64, t_rd_64;

    ILLEGAL_INSN((insn >> 29) != 3); /* ONLY 011 is handled */
    ILLEGAL_INSN(!UCOP_LDST_BHD); /* prefetch or sync? */
    ILLEGAL_INSN(UCOP_REG_D == 31);
    ILLEGAL_INSN(UCOP_REG_S1 == 31);

    t_addr = tcg_temp_new_i64();
    t_op2_64 = tcg_temp_new_i64();
    t_rd_64 = tcg_temp_new_i64();

    /* Prepare base address */
    tcg_gen_mov_i64(t_addr, cpu_R[UCOP_REG_S1]);

    /* Prepare op2 */
    if (UCOP_SET(21)) { /* reg or imm */
        tcg_gen_movi_i64(t_op2_64, UCOP_IMM11);
    } else {
        ILLEGAL_INSN(UCOP_REG_S2 == 31);
        ILLEGAL_INSN(UCOP_IMM_6);

        tcg_gen_mov_i64(t_op2_64, cpu_R[UCOP_REG_S2]);
    }

    if (UCOP_SET(27)) { /* pre */
        if (UCOP_SET(28)) { /* add */
            tcg_gen_add_i64(t_addr, t_addr, t_op2_64);
        } else { /* sub */
            tcg_gen_sub_i64(t_addr, t_addr, t_op2_64);
        }
    }

    if (UCOP_SET(25)) { /* load */
        switch (UCOP_LDST_BHD) {
        case 0:
            ILLEGAL_INSN(true);
            break;
        case 1:
            tcg_gen_qemu_ld64(t_rd_64, t_addr, 1);
            break;
        case 2:
            tcg_gen_qemu_ld16u(t_rd_64, t_addr, 1);
            break;
        case 3:
            tcg_gen_qemu_ld16s(t_rd_64, t_addr, 1);
            break;
        case 4:
            tcg_gen_qemu_ld8u(t_rd_64, t_addr, 1);
            break;
        case 5:
            tcg_gen_qemu_ld8s(t_rd_64, t_addr, 1);
            break;
        case 6:
            tcg_gen_qemu_ld32u(t_rd_64, t_addr, 1);
            break;
        case 7:
            tcg_gen_qemu_ld32s(t_rd_64, t_addr, 1);
            break;
        }
    } else { /* store */
        tcg_gen_mov_i64(t_rd_64, cpu_R[UCOP_REG_D]);

        switch (UCOP_LDST_BHD) {
        case 1:
            tcg_gen_qemu_st64(t_rd_64, t_addr, 1);
            break;
        case 2:
            tcg_gen_qemu_st16(t_rd_64, t_addr, 1);
            break;
        case 4:
            tcg_gen_qemu_st8(t_rd_64, t_addr, 1);
            break;
        case 6:
            tcg_gen_qemu_st32(t_rd_64, t_addr, 1);
            break;
        default:
            ILLEGAL_INSN(true);
            break;
        }
    }

    if (!UCOP_SET(27)) { /* post */
        ILLEGAL_INSN(!UCOP_SET(26)); /* post && !writeback is illegal */

        if (UCOP_SET(28)) { /* add */
            tcg_gen_add_i64(t_addr, t_addr, t_op2_64);
        } else { /* sub */
            tcg_gen_sub_i64(t_addr, t_addr, t_op2_64);
        }
    }

    if (!UCOP_SET(27) || UCOP_SET(26)) { /* post || writeback */
        tcg_gen_mov_i64(cpu_R[UCOP_REG_S1], t_addr);
    }

    if (UCOP_SET(25)) { /* Complete the load, in case rd==rs1 */
        tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_rd_64);
    }

    tcg_temp_free_i64(t_addr);
    tcg_temp_free_i64(t_op2_64);
    tcg_temp_free_i64(t_rd_64);
}

static void do_branch(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    target_ulong t_addr;

    ILLEGAL_INSN(!(UCOP_OPCODE == 0xe));

    if (UCOP_SET(28)) { /* link */
        /* r30 <- next_insn */
        tcg_gen_movi_i64(cpu_R[30], s->dc_pc);
    }

    /* r31 <- current_insn + (signed_offset * 4) */
    t_addr = (s->dc_pc - 4) + ((((int32_t)insn) << 8) >> 6);
    gen_goto_tb(s, 0, t_addr);
}

static void do_coproc(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 tmp;

    switch (UCOP_CPNUM) {
    case 1: /* fake ocd */
        /* ONLY handle movc p1.cd, rs1, #0 */
        ILLEGAL_INSN((insn & 0xfe0007ff) != 0xc0000000);
        ILLEGAL_INSN(UCOP_REG_S1 == 31);

        switch (UCOP_REG_D) { /* REG_D is cd */
        case 0: /* movc p1.c0, rs1, #0 */
            tmp = tcg_temp_new_i64();
            tcg_gen_mov_i64(tmp, cpu_R[UCOP_REG_S1]);
            gen_helper_cp1_putc(tmp);
            tcg_temp_free(tmp);
            break;
        case 1: /* movc p1.c1, rs1, #0 */
            tmp = tcg_temp_new_i64();
            tcg_gen_mov_i64(tmp, cpu_R[UCOP_REG_S1]);
            gen_helper_cp1_putx(tmp);
            tcg_temp_free(tmp);
            break;
        default:
            ILLEGAL_INSN(true);
        }
        break;
    default:
        ILLEGAL_INSN(true);
    }
}

static void do_exception(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    ILLEGAL_INSN(true);
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
        if (UCOP_SET(28)) {
            switch ((insn >> 26) & 0x3) {
            case 0x0:
                do_shift(env, s, insn);
                break;
            case 0x1:
                ILLEGAL_INSN(true);
            case 0x2:
                do_clzclo(env, s, insn);
                break;
            case 0x3:
                do_condmove(env, s, insn);
                break;
            }
        } else {
            do_datap(env, s, insn);
        }
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
        do_exception(env, s, insn);
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
        UNHANDLED_FLOW(unlikely(!QTAILQ_EMPTY(&env->breakpoints)));
        UNHANDLED_FLOW(search_pc);
        UNHANDLED_FLOW(tb->cflags & CF_LAST_IO);

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

    } while (dc->dc_jmp == DISAS_NEXT);

done_disas_loop:
    if (unlikely(env->singlestep_enabled)) {
        UNHANDLED_FLOW(true);
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
            UNHANDLED_FLOW(true);
        }
    }

    gen_icount_end(tb, num_insns);
    *gen_opc_ptr = INDEX_op_end;

    if (search_pc) {
        UNHANDLED_FLOW(true);
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
