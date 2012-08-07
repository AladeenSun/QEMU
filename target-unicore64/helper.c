/*
 * Copyright (C) 2012 Guan Xuetao
 */

#include "cpu.h"
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
    printf("--%16" PRIx64 "--", x); /* Output to stdout */
    fflush(NULL);
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
