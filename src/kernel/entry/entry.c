#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "page_allocator/page_allocator.h"
#include "heap/heap.h"
#include "hpet/hpet.h"
#include "kernel/kernel.h"
#include "debug/debug.h"
#include "queue/queue.h"
#include "global_heap/global_heap.h"
#include "io/io.h"
#include "apic/apic.h"
#include "vector/vector.h"
#include "list/list.h"
#include "vfs/vfs.h"

#include "master/master.h"

#include "worker_pool/worker_pool.h"

#include "../common.h"

#include <libc/string.h>

void main(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    tty_print("\n\r");

#if 1
    for (uint64_t i = 0; i < 6; i++)
    {
        tty_print("Loading fork_test...\n\r");    
        
        worker_pool_spawn("ram:/bin/parent.elf");
    }
#else
    for (uint64_t i = 0; i < 4; i++)
    {
        tty_print("Loading sleep_test...\n\r");
        
        worker_pool_spawn("/bin/sleep_test.elf");
    }
#endif

    tty_print("\n\rKernel Initialized!\n\n\r");

    //Temporary for testing
    tty_clear();
    tty_set_cursor_pos(0, 16 * 20);

    master_entry();

    while (1)
    {
        asm volatile("hlt");
    }
}