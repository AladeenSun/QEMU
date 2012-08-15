/*
 * QEMU UniCore64 CPU
 *
 * Copyright (c) 2012 Guan Xue-tao
 */

#include "cpu-qom.h"
#include "qemu-common.h"

/* CPU models */

typedef struct UniCore64CPUInfo {
    const char *name;
    void (*instance_init)(Object *obj);
} UniCore64CPUInfo;

static void uc64_unicore3_cpu_initfn(Object *obj)
{
    UniCore64CPU *cpu = UNICORE64_CPU(obj);
    CPUUniCore64State *env = &cpu->env;

    env->cp0.c0_cpuid = 0x4d110863;
    env->cp0.c0_cachetype = 0x0519a19a;
    env->uncached_asr = ASR_MODE_PRIV;
    env->regs[31] = 0x03000000;
}

static void uc64_any_cpu_initfn(Object *obj)
{
    UniCore64CPU *cpu = UNICORE64_CPU(obj);
    CPUUniCore64State *env = &cpu->env;

    env->cp0.c0_cpuid = 0xffffffff;
    env->uncached_asr = ASR_MODE_USER;
    env->regs[31] = 0;
}

static const UniCore64CPUInfo uc64_cpus[] = {
    { .name = "UniCore-III", .instance_init = uc64_unicore3_cpu_initfn },
    { .name = "any",         .instance_init = uc64_any_cpu_initfn },
};

static void uc64_cpu_initfn(Object *obj)
{
    UniCore64CPU *cpu = UNICORE64_CPU(obj);
    CPUUniCore64State *env = &cpu->env;

    cpu_exec_init(env);
    env->cpu_model_str = object_get_typename(obj);

    tlb_flush(env, 1);
}

static void uc64_register_cpu_type(const UniCore64CPUInfo *info)
{
    TypeInfo type_info = {
        .name = info->name,
        .parent = TYPE_UNICORE64_CPU,
        .instance_init = info->instance_init,
    };

    type_register_static(&type_info);
}

static const TypeInfo uc64_cpu_type_info = {
    .name = TYPE_UNICORE64_CPU,
    .parent = TYPE_CPU,
    .instance_size = sizeof(UniCore64CPU),
    .instance_init = uc64_cpu_initfn,
    .abstract = true,
    .class_size = sizeof(UniCore64CPUClass),
};

static void uc64_cpu_register_types(void)
{
    int i;

    type_register_static(&uc64_cpu_type_info);
    for (i = 0; i < ARRAY_SIZE(uc64_cpus); i++) {
        uc64_register_cpu_type(&uc64_cpus[i]);
    }
}

type_init(uc64_cpu_register_types)
