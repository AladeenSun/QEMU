/*
 * Copyright (C) 2012 Guan Xuetao
 */
#ifndef QEMU_UC64_SYSCALL_H
#define QEMU_UC64_SYSCALL_H

struct target_pt_regs {
    abi_ulong uc64_regs[36];
};

#define UC64_R00                uc64_regs[0]
#define UC64_R01                uc64_regs[1]
#define UC64_R02                uc64_regs[2]
#define UC64_R03                uc64_regs[3]
#define UC64_R04                uc64_regs[4]
#define UC64_R05                uc64_regs[5]
#define UC64_R06                uc64_regs[6]
#define UC64_R07                uc64_regs[7]
#define UC64_R08                uc64_regs[8]
#define UC64_R09                uc64_regs[9]
#define UC64_R10                uc64_regs[10]
#define UC64_R11                uc64_regs[11]
#define UC64_R12                uc64_regs[12]
#define UC64_R13                uc64_regs[13]
#define UC64_R14                uc64_regs[14]
#define UC64_R15                uc64_regs[15]
#define UC64_R16                uc64_regs[16]
#define UC64_R17                uc64_regs[17]
#define UC64_R18                uc64_regs[18]
#define UC64_R19                uc64_regs[19]
#define UC64_R20                uc64_regs[20]
#define UC64_R21                uc64_regs[21]
#define UC64_R22                uc64_regs[22]
#define UC64_R23                uc64_regs[23]
#define UC64_R24                uc64_regs[24]
#define UC64_R25                uc64_regs[25]
#define UC64_R26                uc64_regs[26]
#define UC64_R27                uc64_regs[27]
#define UC64_R28                uc64_regs[28]
#define UC64_R29                uc64_regs[29]
#define UC64_R30                uc64_regs[30]
#define UC64_R31                uc64_regs[31]
#define UC64_ASR                uc64_regs[32]
#define UC64_AFR                uc64_regs[33]
#define UC64_SW0                uc64_regs[34] /* CP0_SYSU #0 */
#define UC64_O00                uc64_regs[35] /* Orig R00 */

#define UNAME_MACHINE           "UniCore64"

#endif /* QEMU_UC64_SYSCALL_H */
