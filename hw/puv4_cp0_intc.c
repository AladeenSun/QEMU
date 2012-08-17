/*
 * Fake CP0 INTC device simulation in PKUnity SoC
 *
 * Copyright (C) 2012 Guan Xuetao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or any later version.
 * See the COPYING file in the top-level directory.
 */
#include "sysbus.h"

#define DEBUG_PUV4
#include "puv4.h"

uint64_t cp0_c10_0_INTR_TYPE = 0; /* p0.c10 #0 : interrupt type */

/* puv4 intc implementation. */
typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;
    qemu_irq parent_irq;

    uint64_t reg_FAKEICMR; /* in fact, it's asr */
} UC64CP0State;

static uint64_t uc64_cp0_intc_read(void *opaque, target_phys_addr_t offset,
        unsigned size)
{
    uint64_t ret = 0;

    DPRINTF("offset 0x%x, value 0x%" PRIx64 "\n", (int)offset, ret);
    return ret;
}

static void uc64_cp0_intc_write(void *opaque, target_phys_addr_t offset,
        uint64_t value, unsigned size)
{
    UC64CP0State *s = opaque;

    switch (offset) {
    case 0x10: /* fake asr */
        s->reg_FAKEICMR = value;
        break;
    case 0x14: /* MSB 32 bit, the value is 0 */
        break;
    default:
        DPRINTF("Bad offset %x\n", (int)offset);
    }
}

static const MemoryRegionOps uc64_cp0_intc_ops = {
    .read = uc64_cp0_intc_read,
    .write = uc64_cp0_intc_write,
    .impl = {
        .min_access_size = 8,
        .max_access_size = 8,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/* Process a change in an external INTC input. */
static void uc64_cp0_intc_handler(void *opaque, int irq, int level)
{
    UC64CP0State *s = opaque;
    int raise_parent_irq = false;

    if (irq != UC64_INTR_ITM) { /* ITIMER */
        DPRINTF("irq 0x%x, level 0x%x\n", irq, level);
        abort();
    } else {
        irq = UC64_CP0_INTRTYPE_ITM;
    }

    if (level) {
        cp0_c10_0_INTR_TYPE |= irq;
        if (!(s->reg_FAKEICMR & UC64_CP0_INTRTYPE_ITM)) {
            raise_parent_irq = true;
        }
    } else {
        cp0_c10_0_INTR_TYPE &= ~irq;
    }

    /* Update interrupt status after enabled or pending bits have been changed.  */
    if (raise_parent_irq) {
        qemu_irq_raise(s->parent_irq);
    } else {
        qemu_irq_lower(s->parent_irq);
    }
}

static int uc64_cp0_intc_init(SysBusDevice *dev)
{
    UC64CP0State *s = FROM_SYSBUS(UC64CP0State, dev);

    /* Why not 6 times */
    qdev_init_gpio_in(&s->busdev.qdev, uc64_cp0_intc_handler, 6);
    sysbus_init_irq(&s->busdev, &s->parent_irq);

    memory_region_init_io(&s->iomem, &uc64_cp0_intc_ops, s, "uc64_intc",
            PUV4_REGS_OFFSET);
    sysbus_init_mmio(dev, &s->iomem);

    return 0;
}

static void uc64_cp0_intc_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = uc64_cp0_intc_init;
}

static const TypeInfo uc64_cp0_intc_info = {
    .name = "uc64_intc",
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(UC64CP0State),
    .class_init = uc64_cp0_intc_class_init,
};

static void uc64_cp0_intc_register_type(void)
{
    type_register_static(&uc64_cp0_intc_info);
}

type_init(uc64_cp0_intc_register_type)
