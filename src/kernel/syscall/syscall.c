#include "syscall.h"

#include <stdint.h>

#include <lib-system.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "vmm/vmm.h"
#include "time/time.h"
#include "vfs/vfs.h"
#include "utils/utils.h"
#include "smp/smp.h"
#include "debug/debug.h"
#include "interrupts/interrupts.h"
#include "scheduler/scheduler.h"
#include "program_loader/program_loader.h"

static inline uint8_t verify_pointer(const void* pointer, uint64_t size)
{
    if ((uint64_t)pointer > VMM_LOWER_HALF_MAX || (uint64_t)pointer + size > VMM_LOWER_HALF_MAX)
    {
        return 0;
    }

    return 1;
}

static inline uint8_t verify_string(const char* string)
{
    return verify_pointer(string, 0);
}

int64_t syscall_exit(Status status)
{
    scheduler_exit(status);
    debug_panic("Returned from exit");
    return 0;
}

int64_t syscall_spawn(const char* path)
{
    if (!verify_string(path))
    {
        scheduler_thread()->status = STATUS_INVALID_POINTER;
        return -1;
    }

    int64_t pid = scheduler_spawn(path);
    if (pid == -1)
    {
        scheduler_thread()->status = STATUS_FAILURE;
        return -1;
    }

    return pid;
}

int64_t syscall_map(void* address, uint64_t length)
{
    address_space_map(scheduler_process()->addressSpace, address, SIZE_IN_PAGES(length));

    return 0;
}

Status syscall_status(void)
{
    Thread* thread = scheduler_thread();
    Status temp = thread->status;
    thread->status = STATUS_SUCCESS;
    return temp;
}

int64_t syscall_test(const char* string)
{
    tty_acquire();
    
    uint8_t cpuId = smp_self_unsafe()->id;

    uint8_t oldRow = tty_get_row();
    uint8_t oldColumn = tty_get_column();

    tty_set_column(0);
    tty_set_row(cpuId);

    tty_print("CPU: ");
    tty_printi(cpuId); 
    tty_print(" THREAD AMOUNT: "); 
    tty_printi(scheduler_local_thread_amount());
    tty_print(" PID: "); 
    tty_printi(scheduler_process()->id);
    tty_print(" TID: "); 
    tty_printi(scheduler_thread()->id);
    if (string != 0)
    {
        tty_print(" STRING: ");
        tty_print(string);
    }
    tty_print(" TIME: ");
    tty_printi(time_milliseconds());
    tty_print(" MS                                ");

    tty_set_row(oldRow);
    tty_set_column(oldColumn);

    tty_release();

    return 0;
}

void syscall_handler_end(void)
{
    if (scheduler_process()->killed)
    {
        scheduler_exit(STATUS_SUCCESS);
        debug_panic("Returned from exit");
    }
    else
    {
        scheduler_yield();
    }
}

void* syscallTable[] =
{
    syscall_exit,
    syscall_spawn,
    syscall_map,
    syscall_status,
    syscall_test
};