/*
 * Copyright (C) 2012 Guan Xuetao
 */

#include "cpu.h"
#include "dyngen-exec.h"
#include "gdbstub.h"
#include "helper.h"
#include "host-utils.h"

#undef DEBUG_UC64

#ifdef DEBUG_UC64
#define DPRINTF(fmt, ...) printf("%s: " fmt , __func__, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

CPUUniCore64State *uc64_cpu_init(const char *cpu_model)
{
    UniCore64CPU *cpu;
    CPUUniCore64State *env;
    static int inited = 1;

    if (object_class_by_name(cpu_model) == NULL) {
        return NULL;
    }
    cpu = UNICORE64_CPU(object_new(cpu_model));
    env = &cpu->env;

    if (inited) {
        inited = 0;
        uc64_translate_init();
    }

    qemu_init_vcpu(env);
    return env;
}

void helper_cp1_putc(target_ulong x)
{
    printf("%c", (unsigned char)x); /* Output to stdout */
    fflush(NULL);
}

void helper_cp1_putx(target_ulong x)
{
    printf("\n--%16" PRIx64 "--", x); /* Output to stdout */
    fflush(NULL);
}

void HELPER(exception)(uint32_t excp)
{
    env->exception_index = excp;
    cpu_loop_exit(env);
}

void HELPER(afr_write)(uint64_t x)
{
    env->NF = x << 60;
    env->ZF = (~x) & AFR_Z;
    env->CF = (x & AFR_C) >> 1;
    env->VF = x << 63;
}

uint64_t HELPER(afr_read)(void)
{
    return (((env->NF >> 63) << 3) | ((env->ZF == 0) << 2) |
        (env->CF << 1) | (env->VF >> 63));
}

uint32_t HELPER(clo_i32)(uint32_t x)
{
        return clo32(x);
}

uint64_t HELPER(clo_i64)(uint64_t x)
{
        return clo64(x);
}

uint32_t HELPER(clz_i32)(uint32_t x)
{
        return clz32(x);
}

uint64_t HELPER(clz_i64)(uint64_t x)
{
        return clz64(x);
}

/*
 * Flag setting arithmetic is awkward because we need to do comparisons.
 * The only way to do that in TCG is a conditional branch, which clobbers
 * all our temporaries.  For now implement these as helper functions.
 */
uint32_t HELPER(sub_cc_i32)(uint32_t a, uint32_t b)
{
    uint32_t result;
    result = a - b;
    env->NF = env->ZF = (int32_t)result;
    env->CF = a >= b;
    env->VF = ((a ^ b) & (a ^ result));
    return result;
}

uint64_t HELPER(sub_cc_i64)(uint64_t a, uint64_t b)
{
    uint64_t result;
    result = a - b;
    env->NF = env->ZF = result;
    env->CF = a >= b;
    env->VF = ((a ^ b) & (a ^ result));
    return result;
}

uint32_t HELPER(add_cc_i32)(uint32_t a, uint32_t b)
{
    uint32_t result;
    result = a + b;
    env->NF = env->ZF = (int32_t)result;
    env->CF = result < a;
    env->VF = (int32_t)((a ^ b ^ -1) & (a ^ result));
    return result;
}

uint64_t HELPER(add_cc_i64)(uint64_t a, uint64_t b)
{
    uint64_t result;
    result = a + b;
    env->NF = env->ZF = result;
    env->CF = result < a;
    env->VF = (a ^ b ^ -1) & (a ^ result);
    return result;
}

uint32_t HELPER(sbc_cc_i32)(uint32_t a, uint32_t b)
{
    uint32_t result;
    if (!env->CF) {
        result = a - b - 1;
        env->CF = a > b;
    } else {
        result = a - b;
        env->CF = a >= b;
    }
    env->VF = ((a ^ b) & (a ^ result));
    env->NF = env->ZF = (int32_t)result;
    return result;
}

uint64_t HELPER(sbc_cc_i64)(uint64_t a, uint64_t b)
{
    uint64_t result;
    if (!env->CF) {
        result = a - b - 1;
        env->CF = a > b;
    } else {
        result = a - b;
        env->CF = a >= b;
    }
    env->VF = ((a ^ b) & (a ^ result));
    env->NF = env->ZF = result;
    return result;
}

uint32_t HELPER(adc_cc_i32)(uint32_t a, uint32_t b)
{
    uint32_t result;
    if (!env->CF) {
        result = a + b;
        env->CF = result < a;
    } else {
        result = a + b + 1;
        env->CF = result <= a;
    }
    env->VF = (int32_t)((a ^ b ^ -1) & (a ^ result));
    env->NF = env->ZF = (int32_t)result;
    return result;
}

uint64_t HELPER(adc_cc_i64)(uint64_t a, uint64_t b)
{
    uint64_t result;
    if (!env->CF) {
        result = a + b;
        env->CF = result < a;
    } else {
        result = a + b + 1;
        env->CF = result <= a;
    }
    env->VF = ((a ^ b ^ -1) & (a ^ result));
    env->NF = env->ZF = result;
    return result;
}

#ifdef CONFIG_USER_ONLY
void switch_mode(CPUUniCore64State *env, int mode)
{
    if (mode != ASR_MODE_USER) {
        cpu_abort(env, "Tried to switch out of user mode\n");
    }
}

void do_interrupt(CPUUniCore64State *env)
{
    cpu_abort(env, "NO interrupt in user mode\n");
}

int uc64_cpu_handle_mmu_fault(CPUUniCore64State *env, target_ulong address,
                              int access_type, int mmu_idx)
{
    cpu_abort(env, "NO mmu fault in user mode\n");
    return 1;
}
#endif
