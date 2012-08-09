#include "console.h"
#include "elf.h"
#include "exec-memory.h"
#include "sysbus.h"
#include "boards.h"
#include "loader.h"
#include "pc.h"

#undef DEBUG_PUV4
#include "puv4.h"

#define KERNEL_LOAD_ADDR        0x03000000
#define KERNEL_MAX_SIZE         0x00800000 /* Just a guess */

static void puv4_board_init(CPUUniCore64State *env, ram_addr_t ram_size)
{
    MemoryRegion *ram_memory = g_new(MemoryRegion, 1);

    /* SDRAM at address zero.  */
    memory_region_init_ram(ram_memory, "puv4.ram", ram_size);
    vmstate_register_ram_global(ram_memory);
    memory_region_add_subregion(get_system_memory(), 0, ram_memory);
}

static void puv4_load_kernel(const char *kernel_filename)
{
    int size;

    assert(kernel_filename != NULL);

    /* only zImage format supported */
    size = load_image_targphys(kernel_filename, KERNEL_LOAD_ADDR,
            KERNEL_MAX_SIZE);
    if (size < 0) {
        hw_error("Load kernel error: '%s'\n", kernel_filename);
    }

    /* cheat curses that we have a graphic console, only under ocd console */
    graphic_console_init(NULL, NULL, NULL, NULL, NULL);
}

static void puv4_init(ram_addr_t ram_size, const char *boot_device,
                     const char *kernel_filename, const char *kernel_cmdline,
                     const char *initrd_filename, const char *cpu_model)
{
    CPUUniCore64State *env;

    if (initrd_filename) {
        hw_error("Please use kernel built-in initramdisk.\n");
    }

    if (!cpu_model) {
        cpu_model = "UniCore-III";
    }

    env = cpu_init(cpu_model);
    if (!env) {
        hw_error("Unable to find CPU definition\n");
    }

    puv4_board_init(env, ram_size);
    puv4_load_kernel(kernel_filename);
}

static QEMUMachine puv4_machine = {
    .name = "puv4",
    .desc = "PKUnity Version-4 based on UniCore64",
    .init = puv4_init,
    .is_default = 1,
    .use_scsi = 0,
};

static void puv4_machine_init(void)
{
    qemu_register_machine(&puv4_machine);
}

machine_init(puv4_machine_init)
