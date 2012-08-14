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

static TCGv_i64 ex_addr_LL; /* exclusive address for LL */
static bool LLbit;

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

    ex_addr_LL = tcg_global_mem_new_i64(TCG_AREG0,
           offsetof(CPUUniCore64State, exclusive_addr_ll), "ex_addr_ll");
    LLbit = false;

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
#define UCOP_IMM_9              (((insn) >>  2) & 0x1ff)
#define UCOP_CPNUM              (((insn) >> 21) & 0xf)
#define UCOP_LDST_BHD           (((insn) >> 22) & 0x7)
#define UCOP_CMOV_COND          (((insn) >> 12) & 0xf)

#define UCOP_SET(i)             ((insn) & (1 << (i)))

/* internal defines */
typedef struct DisasContext {
    int dc_jmp;
    int dc_singlestep;
    target_ulong dc_pc;
    struct TranslationBlock *dc_tb;
    int dc_condinsn; /* If the insn has conditional test */
    int dc_condlabel; /* Label for next instruction */
} DisasContext;

#define gen_load_cpu_field(t_op_64, name)               \
    tcg_gen_ld_i64(t_op_64, cpu_env, offsetof(CPUUniCore64State, name))

#define gen_store_cpu_field(t_op_64, name)               \
    tcg_gen_st_i64(t_op_64, cpu_env, offsetof(CPUUniCore64State, name))

/* Set flags from result.  */
static inline void gen_flags_logic(TCGv_i64 var_rd)
{
    TCGv_i64 t_flag = tcg_temp_new_i64();

    tcg_gen_st_i64(var_rd, cpu_env, offsetof(CPUUniCore64State, NF));
    tcg_gen_st_i64(var_rd, cpu_env, offsetof(CPUUniCore64State, ZF));

    tcg_gen_movi_i64(t_flag, 0);
    tcg_gen_st_i64(t_flag, cpu_env, offsetof(CPUUniCore64State, CF));
    tcg_gen_st_i64(t_flag, cpu_env, offsetof(CPUUniCore64State, VF));

    tcg_temp_free_i64(t_flag);
}

static void gen_test_cond(int cond, int label)
{
    TCGv_i64 t_f1_64, t_f2_64;
    int t_label;

    t_f1_64 = tcg_temp_new_i64();

    switch (cond) {
    case 0x0: /* eq: Z */
        gen_load_cpu_field(t_f1_64, ZF);
        tcg_gen_brcondi_i64(TCG_COND_EQ, t_f1_64, 0, label);
        break;
    case 0x1: /* ne: !Z */
        gen_load_cpu_field(t_f1_64, ZF);
        tcg_gen_brcondi_i64(TCG_COND_NE, t_f1_64, 0, label);
        break;
    case 0x2: /* cs: C */
        gen_load_cpu_field(t_f1_64, CF);
        tcg_gen_brcondi_i64(TCG_COND_NE, t_f1_64, 0, label);
        break;
    case 0x3: /* cc: !C */
        gen_load_cpu_field(t_f1_64, CF);
        tcg_gen_brcondi_i64(TCG_COND_EQ, t_f1_64, 0, label);
        break;
    case 0x4: /* mi: N */
        gen_load_cpu_field(t_f1_64, NF);
        tcg_gen_brcondi_i64(TCG_COND_LT, t_f1_64, 0, label);
        break;
    case 0x5: /* pl: !N */
        gen_load_cpu_field(t_f1_64, NF);
        tcg_gen_brcondi_i64(TCG_COND_GE, t_f1_64, 0, label);
        break;
    case 0x6: /* vs: V */
        gen_load_cpu_field(t_f1_64, VF);
        tcg_gen_brcondi_i64(TCG_COND_LT, t_f1_64, 0, label);
        break;
    case 0x7: /* vc: !V */
        gen_load_cpu_field(t_f1_64, VF);
        tcg_gen_brcondi_i64(TCG_COND_GE, t_f1_64, 0, label);
        break;
    case 0x8: /* hi: C && !Z */
        t_f2_64 = tcg_temp_new_i64();
        t_label = gen_new_label();
        gen_load_cpu_field(t_f1_64, CF);
        tcg_gen_brcondi_i64(TCG_COND_EQ, t_f1_64, 0, t_label);
        gen_load_cpu_field(t_f2_64, ZF);
        tcg_gen_brcondi_i64(TCG_COND_NE, t_f2_64, 0, label);
        gen_set_label(t_label);
        tcg_temp_free_i64(t_f2_64);
        break;
    case 0x9: /* ls: !C || Z */
        t_f2_64 = tcg_temp_new_i64();
        gen_load_cpu_field(t_f1_64, CF);
        tcg_gen_brcondi_i64(TCG_COND_EQ, t_f1_64, 0, label);
        gen_load_cpu_field(t_f2_64, ZF);
        tcg_gen_brcondi_i64(TCG_COND_EQ, t_f2_64, 0, label);
        tcg_temp_free_i64(t_f2_64);
        break;
    case 0xa: /* ge: N == V -> N ^ V == 0 */
        t_f2_64 = tcg_temp_new_i64();
        gen_load_cpu_field(t_f1_64, VF);
        gen_load_cpu_field(t_f2_64, NF);
        tcg_gen_xor_i64(t_f1_64, t_f1_64, t_f2_64);
        tcg_gen_brcondi_i64(TCG_COND_GE, t_f1_64, 0, label);
        tcg_temp_free_i64(t_f2_64);
        break;
    case 0xb: /* lt: N != V -> N ^ V != 0 */
        t_f2_64 = tcg_temp_new_i64();
        gen_load_cpu_field(t_f1_64, VF);
        gen_load_cpu_field(t_f2_64, NF);
        tcg_gen_xor_i64(t_f1_64, t_f1_64, t_f2_64);
        tcg_gen_brcondi_i64(TCG_COND_LT, t_f1_64, 0, label);
        tcg_temp_free_i64(t_f2_64);
        break;
    case 0xc: /* gt: !Z && N == V */
        t_label = gen_new_label();
        t_f2_64 = tcg_temp_new_i64();
        gen_load_cpu_field(t_f1_64, ZF);
        tcg_gen_brcondi_i64(TCG_COND_EQ, t_f1_64, 0, t_label);
        gen_load_cpu_field(t_f1_64, VF);
        gen_load_cpu_field(t_f2_64, NF);
        tcg_gen_xor_i64(t_f1_64, t_f1_64, t_f2_64);
        tcg_gen_brcondi_i64(TCG_COND_GE, t_f1_64, 0, label);
        gen_set_label(t_label);
        tcg_temp_free_i64(t_f2_64);
        break;
    case 0xd: /* le: Z || N != V */
        t_f2_64 = tcg_temp_new_i64();
        gen_load_cpu_field(t_f1_64, ZF);
        tcg_gen_brcondi_i64(TCG_COND_EQ, t_f1_64, 0, label);
        gen_load_cpu_field(t_f1_64, VF);
        gen_load_cpu_field(t_f2_64, NF);
        tcg_gen_xor_i64(t_f1_64, t_f1_64, t_f2_64);
        tcg_gen_brcondi_i64(TCG_COND_LT, t_f1_64, 0, label);
        tcg_temp_free_i64(t_f2_64);
        break;
    default:
        fprintf(stderr, "Bad condition code 0x%x\n", cond);
        abort();
    }

    tcg_temp_free_i64(t_f1_64);
}

/* dest = T0 + T1 + CF. */
static void gen_add_carry_i32(TCGv_i32 dest, TCGv_i32 t0, TCGv_i32 t1)
{
    TCGv_i32 tmp_32;
    TCGv_i64 tmp_64;
    tmp_32 = tcg_temp_new_i32();
    tmp_64 = tcg_temp_new_i64();

    tcg_gen_add_i32(dest, t0, t1);
    gen_load_cpu_field(tmp_64, CF);
    tcg_gen_trunc_i64_i32(tmp_32, tmp_64);
    tcg_gen_add_i32(dest, dest, tmp_32);

    tcg_temp_free_i32(tmp_32);
    tcg_temp_free_i64(tmp_64);
}

/* dest = T0 - T1 + CF - 1.  */
static void gen_sub_carry_i32(TCGv_i32 dest, TCGv_i32 t0, TCGv_i32 t1)
{
    TCGv_i32 tmp_32;
    TCGv_i64 tmp_64;
    tmp_32 = tcg_temp_new_i32();
    tmp_64 = tcg_temp_new_i64();

    tcg_gen_sub_i32(dest, t0, t1);
    gen_load_cpu_field(tmp_64, CF);
    tcg_gen_trunc_i64_i32(tmp_32, tmp_64);
    tcg_gen_add_i32(dest, dest, tmp_32);
    tcg_gen_subi_i32(dest, dest, 1);

    tcg_temp_free_i32(tmp_32);
    tcg_temp_free_i64(tmp_64);
}

/* dest = T0 + T1 + CF. */
static void gen_add_carry_i64(TCGv_i64 dest, TCGv_i64 t0, TCGv_i64 t1)
{
    TCGv_i64 tmp_64;
    tmp_64 = tcg_temp_new_i64();

    tcg_gen_add_i64(dest, t0, t1);
    gen_load_cpu_field(tmp_64, CF);
    tcg_gen_add_i64(dest, dest, tmp_64);

    tcg_temp_free_i64(tmp_64);
}

/* dest = T0 - T1 + CF - 1.  */
static void gen_sub_carry_i64(TCGv_i64 dest, TCGv_i64 t0, TCGv_i64 t1)
{
    TCGv_i64 tmp_64;
    tmp_64 = tcg_temp_new_i64();

    tcg_gen_sub_i64(dest, t0, t1);
    gen_load_cpu_field(tmp_64, CF);
    tcg_gen_add_i64(dest, dest, tmp_64);
    tcg_gen_subi_i64(dest, dest, 1);

    tcg_temp_free_i64(tmp_64);
}

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
    TCGv_i64 t_op1_64;
    TCGv_i32 t_op1_32;

    ILLEGAL_INSN(UCOP_SET(25));
    ILLEGAL_INSN(UCOP_SET(24));
    ILLEGAL_INSN(UCOP_SET(21));
    ILLEGAL_INSN(UCOP_IMM11);

    t_op1_64 = tcg_temp_new_i64();
    tcg_gen_mov_i64(t_op1_64, cpu_R[UCOP_REG_S1]);

    if (UCOP_SET(22)) { /* 64 bit */
        if (UCOP_SET(23)) { /* DCNTLZ */
            gen_helper_clz_i64(t_op1_64, t_op1_64);
        } else { /* DCNTLO */
            gen_helper_clo_i64(t_op1_64, t_op1_64);
        }
    } else {
        t_op1_32 = tcg_temp_new_i32();
        tcg_gen_trunc_i64_i32(t_op1_32, t_op1_64);
        if (UCOP_SET(23)) { /* CNTLZ */
            gen_helper_clz_i32(t_op1_32, t_op1_32);
            tcg_gen_extu_i32_i64(t_op1_64, t_op1_32);
        } else { /* CNTLO */
            gen_helper_clo_i32(t_op1_32, t_op1_32);
            tcg_gen_extu_i32_i64(t_op1_64, t_op1_32);
        }
    }

    tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_op1_64);

    if (!UCOP_SET(22)) {
        tcg_temp_free_i32(t_op1_32);
    }

    tcg_temp_free_i64(t_op1_64);
}

static void do_condmove(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_op2_64;
    TCGv_i32 t_op2_32;

    ILLEGAL_INSN(UCOP_REG_D == 31);
    ILLEGAL_INSN(!UCOP_SET(24));
    ILLEGAL_INSN(UCOP_SET(11));
    ILLEGAL_INSN((UCOP_CMOV_COND == 0xe) || (UCOP_CMOV_COND == 0xf));

    /* Two branches */
    s->dc_condlabel = gen_new_label(); /* label for next instruction */
    gen_test_cond(UCOP_CMOV_COND ^ 1, s->dc_condlabel);
    s->dc_condinsn = true;

    /* Prepare op2 */
    t_op2_64 = tcg_temp_new_i64();
    if (UCOP_SET(21)) { /* reg or imm */
        tcg_gen_movi_i64(t_op2_64, UCOP_IMM11);
    } else {
        ILLEGAL_INSN(UCOP_IMM_6);
        if (UCOP_REG_S2 == 31) {
            tcg_gen_movi_i64(t_op2_64, s->dc_pc);
        } else {
            tcg_gen_mov_i64(t_op2_64, cpu_R[UCOP_REG_S2]);
        }
    }

    if (!UCOP_SET(22)) { /* If word, not Double word */
        t_op2_32 = tcg_temp_new_i32();
        tcg_gen_trunc_i64_i32(t_op2_32, t_op2_64);
    }

    if (UCOP_SET(25)) { /* DCNOT CNOT */
        if (UCOP_SET(22)) { /* insn DCNOT */
            tcg_gen_not_i64(cpu_R[UCOP_REG_D], t_op2_64);
        } else { /* insn CNOT */
            tcg_gen_not_i32(t_op2_32, t_op2_32);
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op2_32);
        }
    } else { /* DCMOV CMOV */
        if (UCOP_SET(22)) { /* insn DCMOV */
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_op2_64);
        } else { /* insn CMOV */
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op2_32);
        }
    }

    if (UCOP_SET(23)) { /* S bit */
        if (!UCOP_SET(22)) { /* word */
            tcg_gen_ext_i32_i64(t_op2_64, t_op2_32); /* Signed extend */
        }
        gen_flags_logic(t_op2_64);
    }

    /* Free temp variables */
    tcg_temp_free_i64(t_op2_64);
    if (!UCOP_SET(22)) {
        tcg_temp_free_i32(t_op2_32);
    }
}

static void do_datap(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_op1_64, t_op2_64;
    TCGv_i32 t_op1_32, t_op2_32;
    int logic_flag = false;

    ILLEGAL_INSN(UCOP_REG_D == 31);

    /* Prepare op1 if two operands */
    if ((UCOP_OPCODE != 0x0d) && (UCOP_OPCODE != 0x0f)) {
        t_op1_64 = tcg_temp_new_i64();
        if (UCOP_REG_S1 == 31) {
            tcg_gen_movi_i64(t_op1_64, s->dc_pc);
        } else {
            tcg_gen_mov_i64(t_op1_64, cpu_R[UCOP_REG_S1]);
        }

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
        ILLEGAL_INSN(UCOP_IMM_6);
        if (UCOP_REG_S2 == 31) {
            tcg_gen_movi_i64(t_op2_64, s->dc_pc);
        } else {
            tcg_gen_mov_i64(t_op2_64, cpu_R[UCOP_REG_S2]);
        }
    }

    if (!UCOP_SET(22)) { /* If word, not Double word */
        t_op2_32 = tcg_temp_new_i32();
        tcg_gen_trunc_i64_i32(t_op2_32, t_op2_64);
    }

    switch (UCOP_OPCODE) {
    case 0x00: /* insn and dand */
        if (UCOP_SET(22)) { /* insn dand */
            tcg_gen_and_i64(t_op2_64, t_op1_64, t_op2_64);
        } else { /* insn and */
            tcg_gen_and_i32(t_op2_32, t_op1_32, t_op2_32);
        }
        logic_flag = true;
        break;
    case 0x01: /* insn xor dxor */
        if (UCOP_SET(22)) { /* insn dxor */
            tcg_gen_xor_i64(t_op2_64, t_op1_64, t_op2_64);
        } else { /* insn xor */
            tcg_gen_xor_i32(t_op2_32, t_op1_32, t_op2_32);
        }
        logic_flag = true;
        break;
    case 0x02: /* insn sub dsub */
        if (!UCOP_SET(23)) { /* S bit */
            if (UCOP_SET(22)) { /* insn DSUB */
                tcg_gen_sub_i64(t_op2_64, t_op1_64, t_op2_64);
            } else { /* insn SUB */
                tcg_gen_sub_i32(t_op2_32, t_op1_32, t_op2_32);
            }
        } else {
            if (UCOP_SET(22)) { /* insn DSUB */
                gen_helper_sub_cc_i64(t_op2_64, t_op1_64, t_op2_64);
            } else { /* insn SUB */
                gen_helper_sub_cc_i32(t_op2_32, t_op1_32, t_op2_32);
            }
        }
        break;
    case 0x03: /* insn rsub drsub */
        if (!UCOP_SET(23)) { /* S bit */
            if (UCOP_SET(22)) { /* insn DRSUB */
                tcg_gen_sub_i64(t_op2_64, t_op2_64, t_op1_64);
            } else { /* insn RSUB */
                tcg_gen_sub_i32(t_op2_32, t_op2_32, t_op1_32);
            }
        } else {
            if (UCOP_SET(22)) { /* insn DRSUB */
                gen_helper_sub_cc_i64(t_op2_64, t_op2_64, t_op1_64);
            } else { /* insn RSUB */
                gen_helper_sub_cc_i32(t_op2_32, t_op2_32, t_op1_32);
            }
        }
        break;
    case 0x04: /* insn ADD DADD */
        if (!UCOP_SET(23)) { /* S bit */
            if (UCOP_SET(22)) { /* insn DADD */
                tcg_gen_add_i64(t_op2_64, t_op1_64, t_op2_64);
            } else { /* insn ADD */
                tcg_gen_add_i32(t_op2_32, t_op1_32, t_op2_32);
            }
        } else {
            if (UCOP_SET(22)) { /* insn DADD */
                gen_helper_add_cc_i64(t_op2_64, t_op1_64, t_op2_64);
            } else { /* insn ADD */
                gen_helper_add_cc_i32(t_op2_32, t_op1_32, t_op2_32);
            }
        }
        break;
    case 0x05: /* insn addc daddc */
        if (!UCOP_SET(23)) { /* S bit */
            if (UCOP_SET(22)) { /* insn DADDC */
                gen_add_carry_i64(t_op2_64, t_op1_64, t_op2_64);
            } else { /* insn ADDC */
                gen_add_carry_i32(t_op2_32, t_op1_32, t_op2_32);
            }
        } else {
            if (UCOP_SET(22)) { /* insn DADDC */
                gen_helper_adc_cc_i64(t_op2_64, t_op1_64, t_op2_64);
            } else { /* insn ADDC */
                gen_helper_adc_cc_i32(t_op2_32, t_op1_32, t_op2_32);
            }
        }
        break;
    case 0x06: /* insn subc dsubc */
        if (!UCOP_SET(23)) { /* S bit */
            if (UCOP_SET(22)) { /* insn DSUBC */
                gen_sub_carry_i64(t_op2_64, t_op1_64, t_op2_64);
            } else { /* insn SUBC */
                gen_sub_carry_i32(t_op2_32, t_op1_32, t_op2_32);
            }
        } else {
            if (UCOP_SET(22)) { /* insn DSUBC */
                gen_helper_sbc_cc_i64(t_op2_64, t_op1_64, t_op2_64);
            } else { /* insn SUBC */
                gen_helper_sbc_cc_i32(t_op2_32, t_op1_32, t_op2_32);
            }
        }
        break;
    case 0x07: /* insn rsubc drsubc */
        if (!UCOP_SET(23)) { /* S bit */
            if (UCOP_SET(22)) { /* insn DRSUBC */
                gen_sub_carry_i64(t_op2_64, t_op2_64, t_op1_64);
            } else { /* insn RSUBC */
                gen_sub_carry_i32(t_op2_32, t_op2_32, t_op1_32);
            }
        } else {
            if (UCOP_SET(22)) { /* insn DRSUBC */
                gen_helper_sbc_cc_i64(t_op2_64, t_op2_64, t_op1_64);
            } else { /* insn RSUBC */
                gen_helper_sbc_cc_i32(t_op2_32, t_op2_32, t_op1_32);
            }
        }
        break;
    case 0x08: /* insn CMPAND DCMPAND */
        ILLEGAL_INSN(!UCOP_SET(23)); /* S bit */
        ILLEGAL_INSN(UCOP_REG_D);

        if (UCOP_SET(22)) { /* insn DCMPAND */
            tcg_gen_and_i64(t_op2_64, t_op1_64, t_op2_64);
        } else { /* insn CMPAND */
            tcg_gen_and_i32(t_op2_32, t_op1_32, t_op2_32);
        }
        logic_flag = true;
        break;
    case 0x09: /* insn CMPXOR DCMPXOR */
        ILLEGAL_INSN(!UCOP_SET(23)); /* S bit */
        ILLEGAL_INSN(UCOP_REG_D);

        if (UCOP_SET(22)) { /* insn DCMPXOR */
            tcg_gen_xor_i64(t_op2_64, t_op1_64, t_op2_64);
        } else { /* insn XOR */
            tcg_gen_xor_i32(t_op2_32, t_op1_32, t_op2_32);
        }
        logic_flag = true;
        break;
    case 0x0a: /* insn CMPSUB.A DCMPSUB.A */
        ILLEGAL_INSN(!UCOP_SET(23)); /* S bit */
        ILLEGAL_INSN(UCOP_REG_D);

        if (UCOP_SET(22)) { /* insn DCMPSUB.A */
            gen_helper_sub_cc_i64(t_op2_64, t_op1_64, t_op2_64);
        } else { /* insn CMPSUB.A */
            gen_helper_sub_cc_i32(t_op2_32, t_op1_32, t_op2_32);
        }
        break;
    case 0x0b: /* insn cmpadd dcmpadd */
        ILLEGAL_INSN(!UCOP_SET(23)); /* S bit */
        ILLEGAL_INSN(UCOP_REG_D);

        if (UCOP_SET(22)) { /* insn DCMPADD */
            gen_helper_add_cc_i64(t_op2_64, t_op1_64, t_op2_64);
        } else { /* insn CMPADD */
            gen_helper_add_cc_i32(t_op2_32, t_op1_32, t_op2_32);
        }
        break;
    case 0x0c: /* insn or dor */
        if (UCOP_SET(22)) { /* insn dor */
            tcg_gen_or_i64(t_op2_64, t_op1_64, t_op2_64);
        } else { /* insn or */
            tcg_gen_or_i32(t_op2_32, t_op1_32, t_op2_32);
        }
        logic_flag = true;
        break;
    case 0x0d: /* insn MOV DMOV */
        ILLEGAL_INSN(UCOP_REG_S1);
        ILLEGAL_INSN(UCOP_SET(23)); /* S bit, NO mov.a/dmov.a in UniCore64 */
        /* Just write the result */
        break;
    case 0x0e: /* insn andn dandn */
        if (UCOP_SET(22)) { /* insn dandn */
            tcg_gen_andc_i64(t_op2_64, t_op1_64, t_op2_64);
        } else { /* insn andn */
            tcg_gen_andc_i32(t_op2_32, t_op1_32, t_op2_32);
        }
        logic_flag = true;
        break;
    case 0x0f: /* insn NOT DNOT */
        ILLEGAL_INSN(UCOP_REG_S1);
        ILLEGAL_INSN(UCOP_SET(23)); /* S bit, NO not.a/dnot.a in UniCore64 */

        if (UCOP_SET(22)) { /* insn DNOT */
            tcg_gen_not_i64(t_op2_64, t_op2_64);
        } else { /* insn NOT */
            tcg_gen_not_i32(t_op2_32, t_op2_32);
        }
        break;
    default:
        ILLEGAL_INSN(true);
    }

    /* Write result */
    if ((UCOP_OPCODE & 0xc) != 0x8) {
        if (UCOP_SET(22)) { /* Double word */
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_op2_64);
        } else { /* Word */
            tcg_gen_extu_i32_i64(cpu_R[UCOP_REG_D], t_op2_32);
        }
    }

    /* Write flags logic */
    if (UCOP_SET(23) && logic_flag) { /* S bit */
        if (!UCOP_SET(22)) { /* word */
            tcg_gen_ext_i32_i64(t_op2_64, t_op2_32); /* Signed extend */
        }
        gen_flags_logic(t_op2_64);
    }

    /* Free temp variables */
    if ((UCOP_OPCODE != 0x0d) && (UCOP_OPCODE != 0x0f)) { /* if two ops */
        tcg_temp_free_i64(t_op1_64);
        if (!UCOP_SET(22)) {
            tcg_temp_free_i32(t_op1_32);
        }
    }
    tcg_temp_free_i64(t_op2_64);
    if (!UCOP_SET(22)) {
        tcg_temp_free_i32(t_op2_32);
    }
}

static void do_srfr(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_flag_64;

    t_flag_64 = tcg_temp_new_i64();

    if ((insn & 0xfbfffff0) == 0x38200000) { /* insn mov afr/bfr, imm */
        tcg_gen_movi_i64(t_flag_64, (uint64_t)insn & 0xf);
        if (UCOP_SET(26)) { /* C bit: insn mov afr, imm*/
            gen_helper_afr_write(t_flag_64);
        } else {/* insn mov bfr, imm */
            gen_store_cpu_field(t_flag_64, bfr);
        }

        tcg_temp_free_i64(t_flag_64);
        return;
    }
    if ((insn & 0xf3ff07ff) == 0x30000000) { /* mov reg to STATUS/FLAG */
        ILLEGAL_INSN(UCOP_REG_S1 == 31);

        tcg_gen_mov_i64(t_flag_64, cpu_R[UCOP_REG_S1]);

        switch ((insn >> 26) & 0x3) {
        case 0: /* !F && !C : bsr */
            gen_store_cpu_field(t_flag_64, bsr);
            break;
        case 1: /* !F &&  C : asr */
            gen_helper_asr_write(t_flag_64);
            break;
        case 2: /*  F && !C : bfr */
            gen_store_cpu_field(t_flag_64, bfr);
            break;
        case 3: /*  F &&  C : afr */
            gen_helper_afr_write(t_flag_64);
        }

        tcg_temp_free_i64(t_flag_64);
        return;
    }
    if ((insn & 0xf3e0ffff) == 0x20000000) { /* mov STATUS/FLAG to reg */
        ILLEGAL_INSN(UCOP_REG_D == 31);

        switch ((insn >> 26) & 0x3) {
        case 0: /* !F && !C : bsr */
            gen_load_cpu_field(t_flag_64, bsr);
            break;
        case 1: /* !F &&  C : asr */
            gen_helper_asr_read(t_flag_64);
            break;
        case 2: /*  F && !C : bfr */
            gen_load_cpu_field(t_flag_64, bfr);
            break;
        case 3: /*  F &&  C : afr */
            gen_helper_afr_read(t_flag_64);
        }

        tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_flag_64);
        tcg_temp_free_i64(t_flag_64);
        return;
    }

    /* otherwise */
    ILLEGAL_INSN(true);
}

static void do_muldiv(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_op1_64, t_op2_64, t_rd_64;
    TCGv_i32 t_op1_32, t_op2_32, t_rd_32;

    ILLEGAL_INSN(UCOP_SET(26));
    ILLEGAL_INSN(UCOP_SET(25));
    ILLEGAL_INSN(UCOP_SET(24));
    ILLEGAL_INSN(UCOP_SET(21));
    ILLEGAL_INSN(UCOP_REG_D == 31);
    ILLEGAL_INSN(UCOP_REG_S1 == 31);
    ILLEGAL_INSN(UCOP_REG_S2 == 31);
    ILLEGAL_INSN(UCOP_IMM_6);

    t_op1_64 = tcg_temp_new_i64();
    t_op2_64 = tcg_temp_new_i64();
    t_rd_64 = tcg_temp_new_i64();

    tcg_gen_mov_i64(t_op1_64, cpu_R[UCOP_REG_S1]);
    tcg_gen_mov_i64(t_op2_64, cpu_R[UCOP_REG_S2]);

    if (!UCOP_SET(22)) {
        t_op1_32 = tcg_temp_new_i32();
        t_op2_32 = tcg_temp_new_i32();
        t_rd_32 = tcg_temp_new_i32();
        tcg_gen_trunc_i64_i32(t_op1_32, t_op1_64);
        tcg_gen_trunc_i64_i32(t_op2_32, t_op2_64);
    }

    if (UCOP_SET(28)) { /* DDIV DIV */
        if (UCOP_SET(22)) { /* 64 bit div */
            if (UCOP_SET(27)) {
                tcg_gen_divu_i64(t_rd_64, t_op1_64, t_op2_64);
            } else {
                tcg_gen_div_i64(t_rd_64, t_op1_64, t_op2_64);
            }
        } else { /* 32 bit div */
            if (UCOP_SET(27)) {
                tcg_gen_divu_i32(t_rd_32, t_op1_32, t_op2_32);
                tcg_gen_extu_i32_i64(t_rd_64, t_rd_32);
            } else {
                tcg_gen_div_i32(t_rd_32, t_op1_32, t_op2_32);
                tcg_gen_extu_i32_i64(t_rd_64, t_rd_32);
            }
        }
    } else { /* DMUL MUL */
        if (UCOP_SET(22)) { /* 64 bit mul */
            if (UCOP_SET(27)) { /* DMLA */
                tcg_gen_mov_i64(t_rd_64, cpu_R[UCOP_REG_D]);
                tcg_gen_mul_i64(t_op1_64, t_op1_64, t_op2_64);
                tcg_gen_add_i64(t_rd_64, t_rd_64, t_op1_64);
            } else { /* DMUL */
                tcg_gen_mul_i64(t_rd_64, t_op1_64, t_op2_64);
            }
        } else { /* 32 bit mul */
            if (UCOP_SET(27)) { /* MLA */
                tcg_gen_mov_i64(t_rd_64, cpu_R[UCOP_REG_D]);
                tcg_gen_trunc_i64_i32(t_rd_32, t_rd_64);
                tcg_gen_mul_i32(t_op1_32, t_op1_32, t_op2_32);
                tcg_gen_add_i32(t_rd_32, t_rd_32, t_op1_32);
                tcg_gen_extu_i32_i64(t_rd_64, t_rd_32);
            } else { /* MUL */
                tcg_gen_mul_i32(t_rd_32, t_op1_32, t_op2_32);
                tcg_gen_extu_i32_i64(t_rd_64, t_rd_32);
            }
        }
    }

    /* Write the result */
    tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_rd_64);

    if (UCOP_SET(23)) { /* S bit */
        if (!UCOP_SET(22)) { /* word */
            tcg_gen_ext_i32_i64(t_rd_64, t_rd_32); /* Signed extend */
        }
        gen_flags_logic(t_rd_64);
    }

    /* Free temp vars */
    if (!UCOP_SET(22)) {
        tcg_temp_free_i32(t_op1_32);
        tcg_temp_free_i32(t_op2_32);
        tcg_temp_free_i32(t_rd_32);
    }
    tcg_temp_free_i64(t_op1_64);
    tcg_temp_free_i64(t_op2_64);
    tcg_temp_free_i64(t_rd_64);
}

static void do_ldst(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_addr, t_op2_64, t_rd_64;

    ILLEGAL_INSN(!UCOP_LDST_BHD); /* prefetch or sync? */

    t_addr = tcg_temp_new_i64();
    t_op2_64 = tcg_temp_new_i64();
    t_rd_64 = tcg_temp_new_i64();

    /* Prepare base address */
    if (UCOP_REG_S1 == 31) {
        tcg_gen_movi_i64(t_addr, s->dc_pc);
    } else {
        tcg_gen_mov_i64(t_addr, cpu_R[UCOP_REG_S1]);
    }

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
        if (UCOP_REG_D == 31) {
            tcg_gen_movi_i64(t_rd_64, s->dc_pc);
        } else {
            tcg_gen_mov_i64(t_rd_64, cpu_R[UCOP_REG_D]);
        }

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
        ILLEGAL_INSN(UCOP_REG_S1 == 31);

        tcg_gen_mov_i64(cpu_R[UCOP_REG_S1], t_addr);
    }

    if (UCOP_SET(25)) { /* Complete the load, in case rd==rs1 */
        ILLEGAL_INSN(UCOP_REG_D == 31);
        tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_rd_64);
    }

    tcg_temp_free_i64(t_addr);
    tcg_temp_free_i64(t_op2_64);
    tcg_temp_free_i64(t_rd_64);
}

static void do_swap(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_addr, t_op2_64;

    ILLEGAL_INSN(insn & 0x1e000000);
    ILLEGAL_INSN(UCOP_REG_D == 31);
    ILLEGAL_INSN(UCOP_REG_S2 == 31);
    ILLEGAL_INSN(UCOP_IMM_6);

    t_addr = tcg_temp_new_i64();
    t_op2_64 = tcg_temp_new_i64();

    /* Prepare address */
    if (UCOP_REG_S1 == 31) {
        tcg_gen_movi_i64(t_addr, s->dc_pc);
    } else {
        tcg_gen_mov_i64(t_addr, cpu_R[UCOP_REG_S1]);
    }

    /* Prepare op2 */
    tcg_gen_mov_i64(t_op2_64, cpu_R[UCOP_REG_S2]);

    if (UCOP_SET(22)) { /* Double word */
        ILLEGAL_INSN(UCOP_SET(21));

        tcg_gen_qemu_ld64(cpu_R[UCOP_REG_D], t_addr, 1);
        tcg_gen_qemu_st64(t_op2_64, t_addr, 1);
    } else {
        if (UCOP_SET(21)) { /* Byte */
            tcg_gen_qemu_ld8u(cpu_R[UCOP_REG_D], t_addr, 1);
            tcg_gen_qemu_st8(t_op2_64, t_addr, 1);
        } else { /* Word */
            tcg_gen_qemu_ld32u(cpu_R[UCOP_REG_D], t_addr, 1);
            tcg_gen_qemu_st32(t_op2_64, t_addr, 1);
        }
    }

    tcg_temp_free_i64(t_addr);
    tcg_temp_free_i64(t_op2_64);
}

static void do_llsc(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_addr, t_op2_64, t_rd_64;
    int fail_label;
    int done_label;

    ILLEGAL_INSN(UCOP_REG_D == 31);

    t_addr = tcg_temp_new_i64();
    t_op2_64 = tcg_temp_new_i64();
    t_rd_64 = tcg_temp_new_i64();

    /* Prepare base address */
    if (UCOP_REG_S1 == 31) {
        tcg_gen_movi_i64(t_addr, s->dc_pc);
    } else {
        tcg_gen_mov_i64(t_addr, cpu_R[UCOP_REG_S1]);
    }

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

    if (UCOP_SET(25)) { /* ll */
        if (UCOP_SET(22)) { /* double word */
            tcg_gen_qemu_ld64(t_rd_64, t_addr, 1);
        } else { /* word */
            tcg_gen_qemu_ld32u(t_rd_64, t_addr, 1);
        }

        tcg_gen_mov_i64(ex_addr_LL, t_addr);
        LLbit = true;
    } else { /* SC */
        if (LLbit) { /* SC && LLbit is true */
            fail_label = gen_new_label();
            done_label = gen_new_label();

            tcg_gen_brcond_i64(TCG_COND_NE, t_addr, ex_addr_LL, fail_label);
            /* Above tcg will destroy t_addr ??? */

            tcg_gen_mov_i64(t_rd_64, cpu_R[UCOP_REG_D]);
            if (UCOP_SET(22)) { /* double word */
                tcg_gen_qemu_st64(t_rd_64, ex_addr_LL, 1);
            } else { /* word */
                tcg_gen_qemu_st32(t_rd_64, ex_addr_LL, 1);
            }

            /* Now, t_rd_64 is used for LLbit */
            tcg_gen_movi_i64(t_rd_64, true);
            tcg_gen_br(done_label);

            gen_set_label(fail_label);
            tcg_gen_movi_i64(t_rd_64, false);

            gen_set_label(done_label);
        } else { /* SC && LLbit is false */
            tcg_gen_movi_i64(t_rd_64, false);
        }
        LLbit = false;
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
        ILLEGAL_INSN(UCOP_REG_S1 == 31);

        tcg_gen_mov_i64(cpu_R[UCOP_REG_S1], t_addr);
    }

    /* Change the value of RD, in case rd == rs1 */
    tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_rd_64);

    tcg_temp_free_i64(t_addr);
    tcg_temp_free_i64(t_op2_64);
    tcg_temp_free_i64(t_rd_64);
}

static void do_branch(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    target_ulong t_addr;

    if (UCOP_SET(28)) { /* link */
        /* r30 <- next_insn */
        tcg_gen_movi_i64(cpu_R[30], s->dc_pc + 4);
    }

    if (UCOP_OPCODE == 0xf) {
        if (!(insn & 0x00ff07ff)) { /* JUMP and CALL-R */
            ILLEGAL_INSN(UCOP_REG_S1 == 31);

            tcg_gen_mov_i64(cpu_R[31], cpu_R[UCOP_REG_S1]);
            s->dc_jmp = DISAS_JUMP;
        } else { /* RETURN and ERET */
            ILLEGAL_INSN(UCOP_SET(28));

            switch (insn & 0x00ffffff) {
            case 0x00800000:
                /* RETURN instruction: r31 <- r30 */
                tcg_gen_mov_i64(cpu_R[31], cpu_R[30]);
                s->dc_jmp = DISAS_JUMP;
                break;
            case 0x00c00000:
                /* ERET instruction: r31 <- r30, ASR <- BSR */
            default:
                ILLEGAL_INSN(true);
            }
        }
    } else { /* This branch means IMM24 */
        if (UCOP_OPCODE != 0xe) { /* conditional branch */
            s->dc_condlabel = gen_new_label(); /* label for next instruction */
            gen_test_cond(UCOP_OPCODE ^ 1, s->dc_condlabel);
            s->dc_condinsn = true;
        } /* else: UCOP_OPCODE == 0xe, it's insn CALL, just fall through */

        /* r31 <- current_insn + (signed_offset * 4) */
        t_addr = s->dc_pc + ((((int32_t)insn) << 8) >> 6);
        gen_goto_tb(s, 0, t_addr);
        s->dc_jmp = DISAS_TB_JUMP;
    }
}

static void do_coproc(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    TCGv_i64 t_creg_64, t_cop_64;

    switch (UCOP_CPNUM) {
    case 0: /* cp0 */
        ILLEGAL_INSN(UCOP_REG_D == 31);
        ILLEGAL_INSN((insn & 0xfc000003) != 0xc0000000);

        t_creg_64 = tcg_temp_new_i64();
        t_cop_64 = tcg_temp_new_i64();
        tcg_gen_movi_i64(t_creg_64, UCOP_REG_S1);
        tcg_gen_movi_i64(t_cop_64, UCOP_IMM_9);
        if (UCOP_SET(25)) { /* load */
            gen_helper_cp0_get(t_creg_64, cpu_env, t_creg_64, t_cop_64);
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], t_creg_64);
        } else { /* store */
            tcg_gen_movi_i64(t_creg_64, UCOP_REG_D);
            gen_helper_cp0_set(cpu_env, cpu_R[UCOP_REG_S1],
                               t_creg_64, t_cop_64);
        }
        tcg_temp_free(t_creg_64);
        tcg_temp_free(t_cop_64);
        break;
    case 1: /* fake ocd */
        /* ONLY handle movc p1.cd, rs1, #0 */
        ILLEGAL_INSN((insn & 0xfe0007ff) != 0xc0000000);
        ILLEGAL_INSN(UCOP_REG_S1 == 31);

        switch (UCOP_REG_D) { /* REG_D is cd */
        case 0: /* movc p1.c0, rs1, #0 */
            t_creg_64 = tcg_temp_new_i64();
            tcg_gen_mov_i64(t_creg_64, cpu_R[UCOP_REG_S1]);
            gen_helper_cp1_putc(t_creg_64);
            tcg_temp_free(t_creg_64);
            break;
        case 1: /* movc p1.c1, rs1, #0 */
            t_creg_64 = tcg_temp_new_i64();
            tcg_gen_mov_i64(t_creg_64, cpu_R[UCOP_REG_S1]);
            gen_helper_cp1_putx(t_creg_64);
            tcg_temp_free(t_creg_64);
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
    TCGv_i32 tmp;

    if ((insn & 0xff000000) == 0xf0000000) { /* JEPRIV instruction */
        /*
         * NO BSR ASR BFR AFR handling
         */
        ILLEGAL_INSN((insn & 0x00ff0000) != 0); /* Least 16 bits available */

        tmp = tcg_temp_new_i32();
        tcg_gen_movi_i64(cpu_R[31], s->dc_pc + 4);
        tcg_gen_movi_i32(tmp, UC64_EXCP_PRIV);
        gen_helper_exception(tmp);
        tcg_temp_free_i32(tmp);

        s->dc_jmp = DISAS_TB_JUMP;
    } else {
        ILLEGAL_INSN(true);
    }
}

static void disas_uc64_insn(CPUUniCore64State *env, DisasContext *s)
{
    unsigned int insn;

    insn = ldl_code(s->dc_pc);

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
        do_ldst(env, s, insn);
        LLbit = false;
        break;
    case 0x4:
        switch ((insn >> 23) & 0x3) {
        case 0x2:
            do_llsc(env, s, insn);
            break;
        case 0x3:
            do_swap(env, s, insn);
            LLbit = false;
            break;
        default:
            ILLEGAL_INSN(true);
        }
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
    dc->dc_condinsn = false;

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

        dc->dc_pc += 4;

        if (dc->dc_condinsn && !dc->dc_jmp) { /* conditional instructions */
            gen_set_label(dc->dc_condlabel);
            dc->dc_condinsn = false;
        }

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
        case DISAS_TB_JUMP:
            /* nothing more to generate */
            break;
        case DISAS_JUMP:
            tcg_gen_exit_tb(0);
            break;
        default:
            UNHANDLED_FLOW(true);
        }

        if (dc->dc_condinsn) { /* branch instructions */
            gen_set_label(dc->dc_condlabel);
            gen_goto_tb(dc, 1, dc->dc_pc);
            dc->dc_condinsn = false;
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
    cpu_fprintf(f, " NF=%16" PRIx64 "  ZF=%16" PRIx64
            "  CF=%16" PRIx64 "  VF=%16" PRIx64 "\n",
            env->NF, env->ZF, env->CF, env->VF);
}

void restore_state_to_opc(CPUUniCore64State *env, TranslationBlock *tb,
        int pc_pos)
{
    env->regs[31] = gen_opc_pc[pc_pos];
}
