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

#define UCOP_UCF64_FMT          (((insn) >> 25) & 0x03)

static inline long ucf64_reg_offset(int reg)
{
    if (reg & 1) {
        return offsetof(CPUUniCore64State, ucf64.regs[reg >> 1])
          + offsetof(CPU_DoubleU, l.upper);
    } else {
        return offsetof(CPUUniCore64State, ucf64.regs[reg >> 1])
          + offsetof(CPU_DoubleU, l.lower);
    }
}

static void do_ucf64_trans(CPUUniCore64State *env, DisasContext *s,
                           uint32_t insn)
{
    TCGv_i64 tmp;

    tmp = tcg_temp_new_i64();
    if ((insn & 0xfde007ff) == 0xc0400000) { /* insn MFF MTF */
        ILLEGAL_INSN(UCOP_REG_D == 31);
        if (UCOP_SET(25)) { /* MFF */
            tcg_gen_ld_i64(tmp, cpu_env, ucf64_reg_offset(UCOP_REG_S1));
            tcg_gen_mov_i64(cpu_R[UCOP_REG_D], tmp);
        } else { /* MTF */
            tcg_gen_mov_i64(tmp, cpu_R[UCOP_REG_D]);
            tcg_gen_st_i64(tmp, cpu_env, ucf64_reg_offset(UCOP_REG_S1));
        }
        return;
    }

    if ((insn & 0xfde0ffff) == 0xc4400000) { /* insn CFF CTF */
        if (UCOP_SET(25)) { /* CFF */
            if (UCOP_REG_D != 31) {
                gen_helper_ucf64_get_fpsr(tmp, cpu_env);
                tcg_gen_mov_i64(cpu_R[UCOP_REG_D], tmp);
            } else { /* FLAG = FPU.FLAG*/
                gen_helper_ucf64_get_fpfr(tmp, cpu_env);
                gen_helper_afr_write(tmp);
            }
        } else { /* CTF */
            if (UCOP_REG_D != 31) {
                tcg_gen_mov_i64(tmp, cpu_R[UCOP_REG_D]);
                gen_helper_ucf64_set_fpsr(cpu_env, tmp);
            } else { /* FPU.FLAG = FLAG */
                gen_helper_afr_read(tmp);
                gen_helper_ucf64_set_fpfr(cpu_env, tmp);
            }
        }
        return;
    }

    tcg_temp_free(tmp);
    ILLEGAL_INSN(true);
}

static void do_ucf64_fcvt(CPUUniCore64State *env, DisasContext *s,
                          uint32_t insn)
{
    TCGv_i32 t_F0s;
    TCGv_i64 t_F0d;

    t_F0s = tcg_temp_new_i32();
    t_F0d = tcg_temp_new_i64();

    ILLEGAL_INSN(UCOP_UCF64_FMT == 3); /* 26 and 25 bits cannot be 0x3 */
    ILLEGAL_INSN(UCOP_REG_S1); /* UCOP_REG_S1 must be 0 */

    switch ((insn >> 2) & 0xf) { /* op2 */
    case 0: /* cvt.s */
        switch (UCOP_UCF64_FMT) {
        case 1: /* d */
            tcg_gen_ld_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            gen_helper_ucf64_df2sf(t_F0s, t_F0d, cpu_env);
            tcg_gen_st_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        case 2: /* w */
            tcg_gen_ld_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            gen_helper_ucf64_si2sf(t_F0s, t_F0s, cpu_env);
            tcg_gen_st_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        default: /* undefined */
            ILLEGAL_INSN(true);
        }
        break;
    case 1: /* cvt.d */
        switch (UCOP_UCF64_FMT) {
        case 0: /* s */
            tcg_gen_ld_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            gen_helper_ucf64_sf2df(t_F0d, t_F0s, cpu_env);
            tcg_gen_st_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        case 2: /* w */
            tcg_gen_ld_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            gen_helper_ucf64_si2df(t_F0d, t_F0s, cpu_env);
            tcg_gen_st_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        default: /* undefined */
            ILLEGAL_INSN(true);
        }
        break;
    case 4: /* cvt.w */
        switch (UCOP_UCF64_FMT) {
        case 0: /* s */
            tcg_gen_ld_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            gen_helper_ucf64_sf2si(t_F0s, t_F0s, cpu_env);
            tcg_gen_st_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        case 1: /* d */
            tcg_gen_ld_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            gen_helper_ucf64_df2si(t_F0s, t_F0d, cpu_env);
            tcg_gen_st_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        default: /* undefined */
           ILLEGAL_INSN(true);
        }
        break;
    default:
        ILLEGAL_INSN(true);
    }
    tcg_temp_free_i32(t_F0s);
    tcg_temp_free_i64(t_F0d);
}

static void do_ucf64_fcmp(CPUUniCore64State *env, DisasContext *s,
                          uint32_t insn)
{
    TCGv_i32 t_F0s;
    TCGv_i64 t_F0d;
    TCGv_i32 t_F1s;
    TCGv_i64 t_F1d;

    TCGv_i32 cond;

    t_F0s = tcg_temp_new_i32();
    t_F0d = tcg_temp_new_i64();
    t_F1s = tcg_temp_new_i32();
    t_F1d = tcg_temp_new_i64();

    cond = tcg_temp_new_i32();

    ILLEGAL_INSN(UCOP_SET(26));

    tcg_gen_movi_i32(cond, ((insn >> 2) & 0xf));

    if (UCOP_SET(25)) {
        tcg_gen_ld_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_S1));
        tcg_gen_ld_i64(t_F1d, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
        gen_helper_ucf64_cmpd(t_F0d, t_F1d, cond, cpu_env);
    } else {
        tcg_gen_ld_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_S1));
        tcg_gen_ld_i32(t_F1s, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
        gen_helper_ucf64_cmps(t_F0s, t_F1s, cond, cpu_env);
    }
    tcg_temp_free_i32(t_F0s);
    tcg_temp_free_i64(t_F0d);
    tcg_temp_free_i32(t_F1s);
    tcg_temp_free_i64(t_F1d);

    tcg_temp_free_i32(cond);
}

#define UCF64_OP1(name)    do {                           \
        ILLEGAL_INSN(UCOP_REG_S1);                        \
        switch (UCOP_UCF64_FMT) {                         \
        case 0 /* s */:                                   \
            tcg_gen_ld_i32(t_F0s, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_S2));\
            gen_helper_ucf64_##name##s(t_F0s, t_F0s);     \
            tcg_gen_st_i32(t_F0s, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_D)); \
            break;                                        \
        case 1 /* d */:                                   \
            tcg_gen_ld_i64(t_F0d, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_S2));\
            gen_helper_ucf64_##name##d(t_F0d, t_F0d);     \
            tcg_gen_st_i64(t_F0d, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_D)); \
            break;                                        \
        case 2 /* w */:                                   \
            ILLEGAL_INSN(true);                           \
            break;                                        \
        }                                                 \
    } while (0)

#define UCF64_OP2(name)    do {                           \
        switch (UCOP_UCF64_FMT) {                         \
        case 0 /* s */:                                   \
            tcg_gen_ld_i32(t_F0s, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_S1));\
            tcg_gen_ld_i32(t_F1s, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_S2));\
            gen_helper_ucf64_##name##s(t_F0s,             \
                           t_F0s, t_F1s, cpu_env);        \
            tcg_gen_st_i32(t_F0s, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_D)); \
            break;                                        \
        case 1 /* d */:                                   \
            tcg_gen_ld_i64(t_F0d, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_S1));\
            tcg_gen_ld_i64(t_F1d, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_S2));\
            gen_helper_ucf64_##name##d(t_F0d,             \
                           t_F0d, t_F1d, cpu_env);        \
            tcg_gen_st_i64(t_F0d, cpu_env,                \
                           ucf64_reg_offset(UCOP_REG_D)); \
            break;                                        \
        case 2 /* w */:                                   \
            ILLEGAL_INSN(true);                           \
            break;                                        \
        }                                                 \
    } while (0)

static void do_ucf64_datap(CPUUniCore64State *env, DisasContext *s,
                           uint32_t insn)
{
    TCGv_i32 t_F0s;
    TCGv_i64 t_F0d;
    TCGv_i32 t_F1s;
    TCGv_i64 t_F1d;
    TCGv_i32 t_cond;

    t_F0s = tcg_temp_new_i32();
    t_F0d = tcg_temp_new_i64();
    t_F1s = tcg_temp_new_i32();
    t_F1d = tcg_temp_new_i64();
    t_cond = tcg_temp_new_i32();

    ILLEGAL_INSN(UCOP_UCF64_FMT == 3);

    switch ((insn >> 2) & 0xf) { /* op2 */
    case 0: /* add */
        UCF64_OP2(add);
        break;
    case 1: /* sub */
        UCF64_OP2(sub);
        break;
    case 2: /* mul */
        UCF64_OP2(mul);
        break;
    case 4: /* div */
        UCF64_OP2(div);
        break;
    case 5: /* abs */
        UCF64_OP1(abs);
        break;
    case 6: /* mov */
        ILLEGAL_INSN(UCOP_REG_S1);
        switch (UCOP_UCF64_FMT) {
        case 0 /* s */:
            tcg_gen_ld_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            tcg_gen_st_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        case 1 /* d */:
            tcg_gen_ld_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            tcg_gen_st_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        case 2 /* w */:
            tcg_gen_ld_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            tcg_gen_st_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        default:
            ILLEGAL_INSN(true);
        }
        break;
    case 7: /* neg */
        UCF64_OP1(neg);
        break;
    case 9: /* mov.t mov.f */
        ILLEGAL_INSN((insn >> 12) & 0xf);
        if (UCOP_SET(11)) {
            tcg_gen_movi_i32(t_cond, 1);
        } else {
            tcg_gen_movi_i32(t_cond, 0);
        }
        switch (UCOP_UCF64_FMT) {
        case 0 /* s */:
            tcg_gen_ld_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            tcg_gen_ld_i32(t_F1s, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            gen_helper_ucf64_movts(t_F1s, t_F0s, t_cond, cpu_env);
            tcg_gen_st_i32(t_F1s, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        case 1 /* d */:
            tcg_gen_ld_i64(t_F0d, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            tcg_gen_ld_i64(t_F1d, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            gen_helper_ucf64_movtd(t_F1d, t_F0d, t_cond, cpu_env);
            tcg_gen_st_i64(t_F1d, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        case 2 /* w */:
            tcg_gen_ld_i32(t_F0s, cpu_env, ucf64_reg_offset(UCOP_REG_S2));
            tcg_gen_ld_i32(t_F1s, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            gen_helper_ucf64_movtw(t_F1s, t_F0s, t_cond, cpu_env);
            tcg_gen_st_i32(t_F1s, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            break;
        default:
            ILLEGAL_INSN(true);
        }
        break;
    default:
        ILLEGAL_INSN(true);
    }
    tcg_temp_free_i32(t_F0s);
    tcg_temp_free_i64(t_F0d);
    tcg_temp_free_i32(t_F1s);
    tcg_temp_free_i64(t_F1d);
    tcg_temp_free_i32(t_cond);
}

static void do_ucf64_ldst(CPUUniCore64State *env, DisasContext *s,
                          uint32_t insn)
{
    TCGv_i64 t_addr, t_addrh, t_rd_64;
    TCGv_i32 t_rd_32;
    int offset, freg;

    t_addr = tcg_temp_new_i64();
    t_addrh = tcg_temp_new_i64();
    t_rd_64 = tcg_temp_new_i64();
    t_rd_32 = tcg_temp_new_i32();

    /* Prepare base address */
    if (UCOP_REG_S1 == 31) {
        tcg_gen_movi_i64(t_addr, s->dc_pc);
    } else {
        tcg_gen_mov_i64(t_addr, cpu_R[UCOP_REG_S1]);
    }

    /* Prepare offset */
    offset = UCOP_IMM_9 << 2;

    if (UCOP_SET(27)) { /* pre */
        if (UCOP_SET(28)) { /* add */
            tcg_gen_addi_i64(t_addr, t_addr, offset);
        } else { /* sub */
            tcg_gen_subi_i64(t_addr, t_addr, offset);
        }
    }

    if (UCOP_SET(25)) { /* load */
        if (UCOP_SET(0)) { /* dword */
            freg = UCOP_REG_D & 0x1e; /* rd should be 0, 2, 4... */

            tcg_gen_qemu_ld32u(t_rd_64, t_addr, 1);
            tcg_gen_trunc_i64_i32(t_rd_32, t_rd_64);
            tcg_gen_st_i32(t_rd_32, cpu_env, ucf64_reg_offset(freg));

            tcg_gen_addi_i64(t_addrh, t_addr, 4);

            tcg_gen_qemu_ld32u(t_rd_64, t_addrh, 1);
            tcg_gen_trunc_i64_i32(t_rd_32, t_rd_64);
            tcg_gen_st_i32(t_rd_32, cpu_env, ucf64_reg_offset(freg + 1));
        } else { /* word */
            tcg_gen_qemu_ld32u(t_rd_64, t_addr, 1);
            tcg_gen_trunc_i64_i32(t_rd_32, t_rd_64);
            tcg_gen_st_i32(t_rd_32, cpu_env, ucf64_reg_offset(UCOP_REG_D));
        }

    } else { /* store */
        if (UCOP_SET(0)) { /* dword */
           freg = UCOP_REG_D & 0x1e; /* rd should be 0, 2, 4... */

            tcg_gen_ld_i32(t_rd_32, cpu_env, ucf64_reg_offset(freg));
            tcg_gen_extu_i32_i64(t_rd_64, t_rd_32);
            tcg_gen_qemu_st32(t_rd_64, t_addr, 1);

            tcg_gen_addi_i64(t_addrh, t_addr, 4);

            tcg_gen_ld_i32(t_rd_32, cpu_env, ucf64_reg_offset(freg + 1));
            tcg_gen_extu_i32_i64(t_rd_64, t_rd_32);
            tcg_gen_qemu_st32(t_rd_64, t_addrh, 1);
        } else { /* word */
            tcg_gen_ld_i32(t_rd_32, cpu_env, ucf64_reg_offset(UCOP_REG_D));
            tcg_gen_extu_i32_i64(t_rd_64, t_rd_32);
            tcg_gen_qemu_st32(t_rd_64, t_addr, 1);
        }
    }

    if (!UCOP_SET(27)) { /* post */
        if (UCOP_SET(28)) { /* add */
            tcg_gen_addi_i64(t_addr, t_addr, offset);
        } else { /* sub */
            tcg_gen_subi_i64(t_addr, t_addr, offset);
        }
    }

    if (UCOP_SET(26)) { /* writeback */
        ILLEGAL_INSN(UCOP_REG_S1 == 31);
        tcg_gen_mov_i64(cpu_R[UCOP_REG_S1], t_addr);
    }

    tcg_temp_free_i64(t_addr);
    tcg_temp_free_i64(t_addrh);
    tcg_temp_free_i64(t_rd_64);
    tcg_temp_free_i32(t_rd_32);
}

static void do_ucf64(CPUUniCore64State *env, DisasContext *s, uint32_t insn)
{
    switch (insn & 0x3) {
    case 0: /* reg trans*/
        do_ucf64_trans(env, s, insn);
        break;
    case 1: /* data proc */
        switch ((insn >> 27) & 0x3) {
        case 0:
            do_ucf64_datap(env, s, insn);
            break;
        case 1:
            ILLEGAL_INSN(true);
        case 2:
            do_ucf64_fcvt(env, s, insn);
            break;
        case 3:
            do_ucf64_fcmp(env, s, insn);
        }
        break;
    case 2: /* ls word*/
    case 3: /* ls dword*/
        do_ucf64_ldst(env, s, insn);
        break;
    }
}
