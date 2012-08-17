/*
 * Fake CP0 ITM device simulation in PKUnity SoC
 *
 * Copyright (C) 2012 Guan Xuetao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or any later version.
 * See the COPYING file in the top-level directory.
 */
#include "sysbus.h"
#include "ptimer.h"

#define DEBUG_PUV4
#include "puv4.h"

/* puv4 ostimer implementation. */
typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;
    QEMUBH *bh;
    qemu_irq irq_itimer;
    ptimer_state *ptimer;

    uint64_t reg_ITIMERCR; /* p0.c10 #9 : itimer counter reg */
    uint64_t reg_ITIMERMR; /* p0.c10 #10: itimer match reg */
} UC64CP0State;

static uint64_t uc64_cp0_itm_read(void *opaque, target_phys_addr_t offset,
        unsigned size)
{
    UC64CP0State *s = opaque;
    uint64_t ret = 0;

    switch (offset) {
    case 0x00: /* Interrupt type */
    case 0x04:
        ret = cp0_c10_0_INTR_TYPE;
        break;
    case 0x90: /* Counter Register */
    case 0x94:
        ret = s->reg_ITIMERMR - (uint64_t)ptimer_get_count(s->ptimer);
        break;
    default:
        DPRINTF("Bad offset %x\n", (int)offset);
    }

    if (offset & 0x4) { /* MSB 32 bit */
        return (uint32_t)(ret >> 32);
    } else { /* LSB 32 bit */
        return (uint32_t)ret;
    }
}

static void uc64_cp0_itm_write(void *opaque, target_phys_addr_t offset,
        uint64_t value, unsigned size)
{
    UC64CP0State *s = opaque;

    switch (offset) {
    case 0x00: /* Interrupt type */
        if (cp0_c10_0_INTR_TYPE & UC64_CP0_INTRTYPE_ITM) {
            cp0_c10_0_INTR_TYPE &= ~UC64_CP0_INTRTYPE_ITM;
            qemu_irq_lower(s->irq_itimer);
        }
        break;
    case 0x04: /* MSB 32bit of interrupt type */
        break;
    case 0xa0: /* Match Register */
        s->reg_ITIMERMR = value;
        break;
    case 0xa4: /* MSB 32bit of Match Register */
        s->reg_ITIMERMR |= (value << 32);
        if (s->reg_ITIMERMR > s->reg_ITIMERCR) {
            ptimer_set_count(s->ptimer, s->reg_ITIMERMR - s->reg_ITIMERCR);
        } else {
            ptimer_set_count(s->ptimer, s->reg_ITIMERMR +
                    (0xffffffffffffffff - s->reg_ITIMERCR));
        }
        ptimer_run(s->ptimer, 2);
        break;
    default:
        DPRINTF("Bad offset %x\n", (int)offset);
    }
}

static const MemoryRegionOps uc64_cp0_itm_ops = {
    .read = uc64_cp0_itm_read,
    .write = uc64_cp0_itm_write,
    .impl = {
        .min_access_size = 8,
        .max_access_size = 8,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void uc64_cp0_itm_tick(void *opaque)
{
    UC64CP0State *s = opaque;

    DPRINTF("Itimer hit when counter from 0x%" PRIx64 " to 0x%" PRIx64 "!\n",
            s->reg_ITIMERCR, s->reg_ITIMERMR);

    s->reg_ITIMERCR = s->reg_ITIMERMR;
    cp0_c10_0_INTR_TYPE |= UC64_CP0_INTRTYPE_ITM;
    qemu_irq_raise(s->irq_itimer);
}

static int uc64_cp0_itm_init(SysBusDevice *dev)
{
    UC64CP0State *s = FROM_SYSBUS(UC64CP0State, dev);

    s->reg_ITIMERCR = 0;
    s->reg_ITIMERMR = 0;

    sysbus_init_irq(dev, &s->irq_itimer);

    s->bh = qemu_bh_new(uc64_cp0_itm_tick, s);
    s->ptimer = ptimer_init(s->bh);
    ptimer_set_freq(s->ptimer, 50 * 1000 * 1000);

    memory_region_init_io(&s->iomem, &uc64_cp0_itm_ops, s, "uc64_itm",
            PUV4_REGS_OFFSET);
    sysbus_init_mmio(dev, &s->iomem);

    return 0;
}

static void uc64_cp0_itm_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = uc64_cp0_itm_init;
}

static const TypeInfo uc64_cp0_itm_info = {
    .name = "uc64_itm",
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(UC64CP0State),
    .class_init = uc64_cp0_itm_class_init,
};

static void uc64_cp0_itm_register_type(void)
{
    type_register_static(&uc64_cp0_itm_info);
}

type_init(uc64_cp0_itm_register_type)
