/*
 * QEMU UniCore64 CPU
 *
 * Copyright (c) 2012 SUSE LINUX Products GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or (at your option) any
 * later version. See the COPYING file in the top-level directory.
 */
#ifndef QEMU_UC64_CPU_QOM_H
#define QEMU_UC64_CPU_QOM_H

#include "qemu/cpu.h"
#include "cpu.h"

#define TYPE_UNICORE64_CPU "unicore64-cpu"

#define UNICORE64_CPU_CLASS(klass) \
    OBJECT_CLASS_CHECK(UniCore64CPUClass, (klass), TYPE_UNICORE64_CPU)
#define UNICORE64_CPU(obj) \
    OBJECT_CHECK(UniCore64CPU, (obj), TYPE_UNICORE64_CPU)
#define UNICORE64_CPU_GET_CLASS(obj) \
    OBJECT_GET_CLASS(UniCore64CPUClass, (obj), TYPE_UNICORE64_CPU)

/**
 * UniCore64CPUClass:
 *
 * A UniCore64 CPU model.
 */
typedef struct UniCore64CPUClass {
    /*< private >*/
    CPUClass parent_class;
    /*< public >*/
} UniCore64CPUClass;

/**
 * UniCore64CPU:
 * @env: #CPUUniCore64State
 *
 * A UniCore64 CPU.
 */
typedef struct UniCore64CPU {
    /*< private >*/
    CPUState parent_obj;
    /*< public >*/

    CPUUniCore64State env;
} UniCore64CPU;

static inline UniCore64CPU *uc64_env_get_cpu(CPUUniCore64State *env)
{
    return UNICORE64_CPU(container_of(env, UniCore64CPU, env));
}

#define ENV_GET_CPU(e) CPU(uc64_env_get_cpu(e))


#endif
