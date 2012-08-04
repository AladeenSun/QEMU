/*
 * Copyright (C) 2012 Guan Xuetao
 */
#ifndef TARGET_SIGNAL_H
#define TARGET_SIGNAL_H

/* this struct defines a stack used during syscall handling */
typedef struct target_sigaltstack {
    abi_ulong ss_sp;
    abi_ulong ss_flags;
    abi_ulong ss_size;
} target_stack_t;

/*
 * sigaltstack controls
 */
#define TARGET_SS_ONSTACK               1
#define TARGET_SS_DISABLE               2

#define get_sp_from_cpustate(cpustate)  (cpustate->regs[29])

#endif /* TARGET_SIGNAL_H */
