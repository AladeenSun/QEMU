/*
 * UniCore-F64 simulation helpers for QEMU.
 *
 * Copyright (C) 2010-2012 Guan Xuetao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or any later version.
 * See the COPYING file in the top-level directory.
 */
#include "cpu.h"
#include "helper.h"

/* UniCore-F64 system registers.  */
#define UC64_UCF64_FPSCR                (31)
#define UCF64_FPSCR_MASK                (0x7ffffff)
#define UCF64_FPSCR_RND_MASK            (0x7)
#define UCF64_FPSCR_RND(r)              (((r) >>  0) & UCF64_FPSCR_RND_MASK)
#define UCF64_FPSCR_TRAPEN_MASK         (0x7f)
#define UCF64_FPSCR_TRAPEN(r)           (((r) >> 10) & UCF64_FPSCR_TRAPEN_MASK)
#define UCF64_FPSCR_FLAG_MASK           (0x1ff)
#define UCF64_FPSCR_FLAG(r)             (((r) >> 17) & UCF64_FPSCR_FLAG_MASK)
#define UCF64_FPSCR_FLAG_ZERO           (1 << 17)
#define UCF64_FPSCR_FLAG_INFINITY       (1 << 18)
#define UCF64_FPSCR_FLAG_INVALID        (1 << 19)
#define UCF64_FPSCR_FLAG_UNDERFLOW      (1 << 20)
#define UCF64_FPSCR_FLAG_OVERFLOW       (1 << 21)
#define UCF64_FPSCR_FLAG_INEXACT        (1 << 22)
#define UCF64_FPSCR_FLAG_HUGEINT        (1 << 23)
#define UCF64_FPSCR_FLAG_DENORMAL       (1 << 24)
#define UCF64_FPSCR_FLAG_DIVZERO        (1 << 25)

/*
 * The convention used for UniCore-F64 instructions:
 *  Single precition routines have a "s" suffix
 *  Double precision routines have a "d" suffix.
 */

/* Convert host exception flags to f64 form.  */
static inline int ucf64_exceptbits_from_host(int host_bits)
{
    int target_bits = 0;

    if (host_bits & float_flag_invalid) {
        target_bits |= UCF64_FPSCR_FLAG_INVALID;
    }
    if (host_bits & float_flag_divbyzero) {
        target_bits |= UCF64_FPSCR_FLAG_DIVZERO;
    }
    if (host_bits & float_flag_overflow) {
        target_bits |= UCF64_FPSCR_FLAG_OVERFLOW;
    }
    if (host_bits & float_flag_underflow) {
        target_bits |= UCF64_FPSCR_FLAG_UNDERFLOW;
    }
    if (host_bits & float_flag_inexact) {
        target_bits |= UCF64_FPSCR_FLAG_INEXACT;
    }
    return target_bits;
}

uint64_t HELPER(ucf64_get_fpscr)(CPUUniCore64State *env)
{
    int i;
    uint64_t fpscr;

    fpscr = (env->ucf64.xregs[UC64_UCF64_FPSCR] & UCF64_FPSCR_MASK);
    i = get_float_exception_flags(&env->ucf64.fp_status);
    fpscr |= (uint64_t)ucf64_exceptbits_from_host(i);
    return fpscr;
}

/* Convert ucf64 exception flags to target form.  */
static inline int ucf64_exceptbits_to_host(int target_bits)
{
    int host_bits = 0;

    if (target_bits & UCF64_FPSCR_FLAG_INVALID) {
        host_bits |= float_flag_invalid;
    }
    if (target_bits & UCF64_FPSCR_FLAG_DIVZERO) {
        host_bits |= float_flag_divbyzero;
    }
    if (target_bits & UCF64_FPSCR_FLAG_OVERFLOW) {
        host_bits |= float_flag_overflow;
    }
    if (target_bits & UCF64_FPSCR_FLAG_UNDERFLOW) {
        host_bits |= float_flag_underflow;
    }
    if (target_bits & UCF64_FPSCR_FLAG_INEXACT) {
        host_bits |= float_flag_inexact;
    }
    return host_bits;
}

void HELPER(ucf64_set_fpscr)(CPUUniCore64State *env, uint64_t val)
{
    int i;
    uint64_t changed;

    changed = env->ucf64.xregs[UC64_UCF64_FPSCR];
    env->ucf64.xregs[UC64_UCF64_FPSCR] = (val & UCF64_FPSCR_MASK);

    changed ^= val;
    if (changed & (UCF64_FPSCR_RND_MASK)) {
        i = UCF64_FPSCR_RND(val);
        switch (i) {
        case 0:
            i = float_round_nearest_even;
            break;
        case 1:
            i = float_round_to_zero;
            break;
        case 2:
            i = float_round_up;
            break;
        case 3:
            i = float_round_down;
            break;
        default: /* 100 and 101 not implement */
            cpu_abort(env, "Unsupported UniCore64-F64 round mode");
        }
        set_float_rounding_mode(i, &env->ucf64.fp_status);
    }

    i = ucf64_exceptbits_to_host(UCF64_FPSCR_TRAPEN(val));
    set_float_exception_flags(i, &env->ucf64.fp_status);
}

float32 HELPER(ucf64_adds)(float32 a, float32 b, CPUUniCore64State *env)
{
    return float32_add(a, b, &env->ucf64.fp_status);
}

float64 HELPER(ucf64_addd)(float64 a, float64 b, CPUUniCore64State *env)
{
    return float64_add(a, b, &env->ucf64.fp_status);
}

float32 HELPER(ucf64_subs)(float32 a, float32 b, CPUUniCore64State *env)
{
    return float32_sub(a, b, &env->ucf64.fp_status);
}

float64 HELPER(ucf64_subd)(float64 a, float64 b, CPUUniCore64State *env)
{
    return float64_sub(a, b, &env->ucf64.fp_status);
}

float32 HELPER(ucf64_muls)(float32 a, float32 b, CPUUniCore64State *env)
{
    return float32_mul(a, b, &env->ucf64.fp_status);
}

float64 HELPER(ucf64_muld)(float64 a, float64 b, CPUUniCore64State *env)
{
    return float64_mul(a, b, &env->ucf64.fp_status);
}

float32 HELPER(ucf64_divs)(float32 a, float32 b, CPUUniCore64State *env)
{
    return float32_div(a, b, &env->ucf64.fp_status);
}

float64 HELPER(ucf64_divd)(float64 a, float64 b, CPUUniCore64State *env)
{
    return float64_div(a, b, &env->ucf64.fp_status);
}

float32 HELPER(ucf64_negs)(float32 a)
{
    return float32_chs(a);
}

float64 HELPER(ucf64_negd)(float64 a)
{
    return float64_chs(a);
}

float32 HELPER(ucf64_abss)(float32 a)
{
    return float32_abs(a);
}

float64 HELPER(ucf64_absd)(float64 a)
{
    return float64_abs(a);
}

/* Helper routines to perform bitwise copies between float and int.  */
static inline float32 ucf64_itos(uint32_t i)
{
    union {
        uint32_t i;
        float32 s;
    } v;

    v.i = i;
    return v.s;
}

static inline uint32_t ucf64_stoi(float32 s)
{
    union {
        uint32_t i;
        float32 s;
    } v;

    v.s = s;
    return v.i;
}

static inline float64 ucf64_itod(uint64_t i)
{
    union {
        uint64_t i;
        float64 d;
    } v;

    v.i = i;
    return v.d;
}

static inline uint64_t ucf64_dtoi(float64 d)
{
    union {
        uint64_t i;
        float64 d;
    } v;

    v.d = d;
    return v.i;
}

/* Integer to float conversion.  */
float32 HELPER(ucf64_si2sf)(float32 x, CPUUniCore64State *env)
{
    return int32_to_float32(ucf64_stoi(x), &env->ucf64.fp_status);
}

float64 HELPER(ucf64_si2df)(float32 x, CPUUniCore64State *env)
{
    return int32_to_float64(ucf64_stoi(x), &env->ucf64.fp_status);
}

/* Float to integer conversion.  */
float32 HELPER(ucf64_sf2si)(float32 x, CPUUniCore64State *env)
{
    return ucf64_itos(float32_to_int32(x, &env->ucf64.fp_status));
}

float32 HELPER(ucf64_df2si)(float64 x, CPUUniCore64State *env)
{
    return ucf64_itos(float64_to_int32(x, &env->ucf64.fp_status));
}

/* floating point conversion */
float64 HELPER(ucf64_sf2df)(float32 x, CPUUniCore64State *env)
{
    return float32_to_float64(x, &env->ucf64.fp_status);
}

float32 HELPER(ucf64_df2sf)(float64 x, CPUUniCore64State *env)
{
    return float64_to_float32(x, &env->ucf64.fp_status);
}
