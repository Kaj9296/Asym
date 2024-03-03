#include <common/boot_info/boot_info.h>
#include <stdint.h>

#include "tty/tty.h"
#include "smp/smp.h"
#include "time/time.h"
#include "kernel/kernel.h"
#include "scheduler/scheduler.h"

#include "interrupts/interrupts.h"

void main(BootInfo* bootInfo)
{
    kernel_init(bootInfo);

    tty_print("\n");

#if 1
    for (uint64_t i = 0; i < 8; i++)
    {
        tty_print("Loading parent...\n");

        scheduler_spawn("ram:/bin/parent.elf");
    }
#else
    for (uint64_t i = 0; i < 4; i++)
    {
        tty_print("Loading sleep_test...\n");

        worker_pool_spawn("ram:/bin/sleep_test.elf");
    }
#endif

    tty_print("\n\rKernel Initialized!\n\n");

    //Temporary for testing
    tty_clear();
    tty_set_row(20);

    Ipi ipi = 
    {
        .type = IPI_TYPE_SCHEDULE
    };
    smp_send_ipi_to_all(ipi);

    while (1)
    {
        asm volatile("hlt");
    }
}
