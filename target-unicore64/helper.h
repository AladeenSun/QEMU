/*
 * Copyright (C) 2012 Guan Xuetao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or (at your option) any
 * later version. See the COPYING file in the top-level directory.
 */
#include "def-helper.h"

DEF_HELPER_1(cp1_putc, void, i64)
DEF_HELPER_1(cp1_putx, void, i64)
DEF_HELPER_0(ocd_output, void)
DEF_HELPER_3(cp0_get, i64, env, i64, i64)
DEF_HELPER_4(cp0_set, void, env, i64, i64, i64)

DEF_HELPER_1(exception, void, i32)
DEF_HELPER_0(afr_read, i64)
DEF_HELPER_0(asr_read, i64)
DEF_HELPER_1(afr_write, void, i64)
DEF_HELPER_1(asr_write, void, i64)
DEF_HELPER_1(clz_i32, i32, i32)
DEF_HELPER_1(clz_i64, i64, i64)
DEF_HELPER_1(clo_i32, i32, i32)
DEF_HELPER_1(clo_i64, i64, i64)
DEF_HELPER_2(sub_cc_i32, i32, i32, i32)
DEF_HELPER_2(sub_cc_i64, i64, i64, i64)
DEF_HELPER_2(add_cc_i32, i32, i32, i32)
DEF_HELPER_2(add_cc_i64, i64, i64, i64)
DEF_HELPER_2(sbc_cc_i32, i32, i32, i32)
DEF_HELPER_2(sbc_cc_i64, i64, i64, i64)
DEF_HELPER_2(adc_cc_i32, i32, i32, i32)
DEF_HELPER_2(adc_cc_i64, i64, i64, i64)

DEF_HELPER_1(ucf64_get_fpsr, i64, env)
DEF_HELPER_2(ucf64_set_fpsr, void, env, i64)

DEF_HELPER_3(ucf64_adds, f32, f32, f32, env)
DEF_HELPER_3(ucf64_addd, f64, f64, f64, env)
DEF_HELPER_3(ucf64_subs, f32, f32, f32, env)
DEF_HELPER_3(ucf64_subd, f64, f64, f64, env)
DEF_HELPER_3(ucf64_muls, f32, f32, f32, env)
DEF_HELPER_3(ucf64_muld, f64, f64, f64, env)
DEF_HELPER_3(ucf64_divs, f32, f32, f32, env)
DEF_HELPER_3(ucf64_divd, f64, f64, f64, env)
DEF_HELPER_1(ucf64_negs, f32, f32)
DEF_HELPER_1(ucf64_negd, f64, f64)
DEF_HELPER_1(ucf64_abss, f32, f32)
DEF_HELPER_1(ucf64_absd, f64, f64)

DEF_HELPER_2(ucf64_sf2df, f64, f32, env)
DEF_HELPER_2(ucf64_df2sf, f32, f64, env)

DEF_HELPER_2(ucf64_si2sf, f32, f32, env)
DEF_HELPER_2(ucf64_si2df, f64, f32, env)

DEF_HELPER_2(ucf64_sf2si, f32, f32, env)
DEF_HELPER_2(ucf64_df2si, f32, f64, env)

#include "def-helper.h"
