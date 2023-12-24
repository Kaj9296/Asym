#include "kernel.h"

#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "heap/heap.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "file_system/file_system.h"
#include "page_allocator/page_allocator.h"
#include "scheduler/scheduler.h"
#include "acpi/acpi.h"
#include "io/io.h"
#include "interrupt_stack/interrupt_stack.h"
#include "hpet/hpet.h"
#include "interrupts/interrupts.h"
#include "time/time.h"

#include "../common.h"

extern uint64_t _kernelStart;
extern uint64_t _kernelEnd;

void kernel_init(BootInfo* bootInfo)
{    
    tty_init(bootInfo->framebuffer, bootInfo->font);
    tty_print("Hello from the kernel!\n\r");

    page_allocator_init(bootInfo->memoryMap, bootInfo->framebuffer);

    page_directory_init(bootInfo->memoryMap, bootInfo->framebuffer);

    heap_init();

    gdt_init();

    acpi_init(bootInfo->xsdp);

    idt_init(); 
    
    file_system_init(bootInfo->rootDirectory);
    
    syscall_init();

    hpet_init(TICKS_PER_SECOND);

    time_init();

    scheduler_init();

    io_pic_clear_mask(IRQ_KEYBOARD);
}
