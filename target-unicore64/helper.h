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

DEF_HELPER_2(sub_cc_i32, i32, i32, i32)
DEF_HELPER_2(sub_cc_i64, i64, i64, i64)
DEF_HELPER_2(add_cc_i32, i32, i32, i32)
DEF_HELPER_2(add_cc_i64, i64, i64, i64)

#include "def-helper.h"
