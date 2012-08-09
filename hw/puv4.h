#ifndef QEMU_HW_PUV4_H
#define QEMU_HW_PUV4_H

/* All puv4_*.c use DPRINTF for debug. */
#ifdef DEBUG_PUV4
#define DPRINTF(fmt, ...) printf("%s: " fmt , __func__, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

#endif /* !QEMU_HW_PUV4_H */
