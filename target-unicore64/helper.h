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

DEF_HELPER_1(exception, void, i32)
DEF_HELPER_0(afr_read, i64)
DEF_HELPER_0(asr_read, i64)
DEF_HELPER_0(bfr_read, i64)
DEF_HELPER_0(bsr_read, i64)
DEF_HELPER_1(afr_write, void, i64)
DEF_HELPER_1(asr_write, void, i64)
DEF_HELPER_1(bfr_write, void, i64)
DEF_HELPER_1(bsr_write, void, i64)
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

#include "def-helper.h"
