#ifndef QEMU_HW_PUV4_H
#define QEMU_HW_PUV4_H

/* All puv4_*.c use DPRINTF for debug. */
#ifdef DEBUG_PUV4
#define DPRINTF(fmt, ...) printf("%s: " fmt , __func__, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

#define PUV4_REGS_OFFSET        (0x1000) /* 4K is reasonable */

/* Hardware interrupts */
#define UC64_INTR_NUM           (7)
#define UC64_INTR_SMP           (0)
#define UC64_INTR_LSU           (1)
#define UC64_INTR_ITM           (2)
#define UC64_INTR_OTM           (3)
#define UC64_INTR_DEV           (4)
#define UC64_INTR_PFM           (5)

#define UC64_CP0_INTRTYPE_ITM   (1 << 7)

#define UC64_CP0_INTC_BASE      (0xf10000000) /* FAKE */
#define UC64_CP0_ITM_BASE       (0xf20000000) /* FAKE */

extern uint64_t cp0_c10_0_INTR_TYPE;

#endif /* !QEMU_HW_PUV4_H */
