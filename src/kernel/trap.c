#include "trap.h"

#include "apic.h"
#include "gdt.h"
#include "irq.h"
#include "log.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "vectors.h"

void cli_push(void)
{
    if (!smp_initialized())
    {
        return;
    }

    // Race condition does not matter
    uint64_t rflags = rflags_read();
    asm volatile("cli");

    cpu_t* cpu = smp_self_unsafe();
    if (cpu->cliAmount == 0)
    {
        cpu->prevFlags = rflags;
    }
    cpu->cliAmount++;
}

void cli_pop(void)
{
    if (!smp_initialized())
    {
        return;
    }

    cpu_t* cpu = smp_self_unsafe();

    LOG_ASSERT(cpu->cliAmount != 0, "cli amount underflow");

    cpu->cliAmount--;
    if (cpu->cliAmount == 0 && cpu->prevFlags & RFLAGS_INTERRUPT_ENABLE && cpu->trapDepth == 0)
    {
        asm volatile("sti");
    }
}

static void exception_handler(const trap_frame_t* trapFrame)
{
    if (trapFrame->ss == GDT_KERNEL_DATA)
    {
        log_panic(trapFrame, "Exception");
    }
    else
    {
        log_panic(trapFrame, "Unhandled User Exception");
        // sched_process_exit(1);
    }
}

static void ipi_handler(trap_frame_t* trapFrame)
{
    cpu_t* cpu = smp_self_unsafe();
    while (1)
    {
        ipi_t ipi = smp_recieve(cpu);
        if (ipi == NULL)
        {
            break;
        }

        ipi(trapFrame);
    }

    lapic_eoi();
}

void trap_handler(trap_frame_t* trapFrame)
{
    if (trapFrame->vector < VECTOR_IRQ_BASE)
    {
        exception_handler(trapFrame);
    }

    cpu_t* cpu = smp_self_unsafe();
    cpu->trapDepth++;

    if (trapFrame->vector >= VECTOR_IRQ_BASE && trapFrame->vector < VECTOR_IRQ_BASE + IRQ_AMOUNT)
    {
        irq_dispatch(trapFrame);
    }
    else if (trapFrame->vector == VECTOR_IPI)
    {
        ipi_handler(trapFrame);
    }
    else if (trapFrame->vector == VECTOR_SCHED_TIMER)
    {
        lapic_eoi();
    }
    else if (trapFrame->vector == VECTOR_SCHED_INVOKE)
    {
        // Do nothing, sheduling happens below
    }
    else
    {
        log_panic(trapFrame, "Unknown vector");
    }

    sched_schedule(trapFrame);

    cpu->trapDepth--;
}
