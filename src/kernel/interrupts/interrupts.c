#include "interrupts.h"

#include "kernel/kernel.h"
#include "io/io.h"
#include "tty/tty.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "scheduler/scheduler.h"
#include "debug/debug.h"
#include "string/string.h"
#include "time/time.h"
#include "heap/heap.h"
#include "idt/idt.h"
#include "page_allocator/page_allocator.h"
#include "smp/smp.h"
#include "apic/apic.h"
#include "spin_lock/spin_lock.h"

#include "../common.h"

extern uint64_t interruptVectorsStart;
extern uint64_t interruptVectorsEnd;

extern PageDirectory* interruptPageDirectory;

const char* exception_strings[32] = 
{
    "Division Fault",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "Floating Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception"
};

void interrupts_init()
{
    tty_start_message("Interrupt vectors initializing");    

    interruptPageDirectory = kernelPageDirectory;

    tty_end_message(TTY_MESSAGE_OK);
}

void interrupts_enable()
{    
    asm volatile ("sti");
}

void interrupts_disable()
{
    asm volatile ("cli");
}

void interrupt_vectors_map(PageDirectory* pageDirectory)
{
    void* virtualAddress = (void*)round_down((uint64_t)&interruptVectorsStart, 0x1000);
    void* physicalAddress = page_directory_get_physical_address(kernelPageDirectory, virtualAddress);
    uint64_t pageAmount = GET_SIZE_IN_PAGES((uint64_t)&interruptVectorsEnd - (uint64_t)&interruptVectorsStart);

    page_directory_remap_pages(pageDirectory, virtualAddress, physicalAddress, pageAmount, PAGE_DIR_READ_WRITE);
}

void interrupt_handler(InterruptFrame* interruptFrame)
{           
    if (interruptFrame->vector < 32) //Exception
    {
        exception_handler(interruptFrame);
    }
    else if (interruptFrame->vector >= 0x20 && interruptFrame->vector <= 0x30) //IRQ
    {    
        irq_handler(interruptFrame);
    }
    else if (interruptFrame->vector == 0x80) //Syscall
    {
        syscall_handler(interruptFrame);
    } 
    else if (interruptFrame->vector >= 0x90) //Inter processor interrupt
    {
        ipi_handler(interruptFrame);
    }
}

void irq_handler(InterruptFrame* interruptFrame)
{
    uint64_t irq = interruptFrame->vector - 0x20;

    switch (irq)
    {
    case IRQ_PIT:
    {
        time_tick();

        if (time_get_tick() % (TICKS_PER_SECOND / 2) == 0) //For testing
        {   
            smp_send_ipi_to_others(IPI_SCHEDULE);
        }
    }
    break;
    default:
    {
        //Not implemented
    }
    break;
    }        

    io_pic_eoi(irq); 
}

void ipi_handler(InterruptFrame* interruptFrame)
{        
    switch (interruptFrame->vector)
    {
    case IPI_HALT:
    {
        interrupts_disable();

        while (1)
        {
            asm volatile("hlt");
        }
    }
    break;
    case IPI_SCHEDULE:
    {
        scheduler_acquire();

        interrupt_frame_copy(scheduler_running_process()->interruptFrame, interruptFrame);
        scheduler_schedule();    
        interrupt_frame_copy(interruptFrame, scheduler_running_process()->interruptFrame);

        scehduler_release();
    }
    break;
    default:
    {
        //Not implemented
    }
    break;
    }

    local_apic_eoi();
}

void exception_handler(InterruptFrame* interruptFrame)
{    
    uint64_t randomNumber = 0;

    Pixel black;
    black.a = 255;
    black.r = 0;
    black.g = 0;
    black.b = 0;

    Pixel white;
    white.a = 255;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    Pixel red;
    red.a = 255;
    red.r = 224;
    red.g = 108;
    red.b = 117;

    uint64_t scale = 3;

    Point startPoint;
    startPoint.x = 100;
    startPoint.y = 50;

    tty_set_scale(scale);

    //tty_clear();

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.x, startPoint.y);
    tty_print("KERNEL PANIC!\n\r");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 1 * scale);
    tty_print("// ");
    tty_print(errorJokes[randomNumber]);

    tty_set_background(black);
    tty_set_foreground(red);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 3 * scale);
    tty_print("\"");
    tty_print(exception_strings[interruptFrame->vector]);
    tty_print("\": ");
    for (int i = 0; i < 32; i++)
    {
        tty_put('0' + ((interruptFrame->errorCode >> (i)) & 1));
    }

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 5 * scale);
    tty_print("OS_VERSION = ");
    tty_print(OS_VERSION);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 7 * scale);
    tty_print("Interrupt Stack Frame: ");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 8 * scale);
    tty_print("Instruction pointer = ");
    tty_printx(interruptFrame->instructionPointer);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 9 * scale);
    tty_print("Code segment = ");
    tty_printx(interruptFrame->codeSegment);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 10 * scale);
    tty_print("Rflags = ");
    tty_printx(interruptFrame->flags);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 11 * scale);
    tty_print("Stack pointer = ");
    tty_printx(interruptFrame->stackPointer);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 12 * scale);
    tty_print("Stack segment = ");
    tty_printx(interruptFrame->stackSegment);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 14 * scale);
    tty_print("Memory: ");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 15 * scale);
    tty_print("Used Heap = ");
    tty_printi(heap_reserved_size());
    tty_print(" B");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 16 * scale);
    tty_print("Free Heap = ");
    tty_printi(heap_free_size());
    tty_print(" B");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 17 * scale);
    tty_print("Locked Pages = ");
    tty_printi(page_allocator_locked_amount());

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 18 * scale);
    tty_print("Unlocked Pages = ");
    tty_printi(page_allocator_unlocked_amount());

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 20 * scale);
    tty_print("Please manually reboot your machine.");

    smp_send_ipi_to_others(IPI_HALT);

    while (1)
    {
        asm volatile("hlt");
    }
}