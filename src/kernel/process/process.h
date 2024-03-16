#pragma once

#include <stdint.h>

#include <lib-system.h>

#include "lock/lock.h"
#include "vfs/vfs.h"
#include "file_table/file_table.h"
#include "interrupt_frame/interrupt_frame.h"
#include "vmm/vmm.h"

#define THREAD_MASTER_ID 0

#define THREAD_STATE_ACTIVE 0
#define THREAD_STATE_KILLED 1

#define THREAD_PRIORITY_LEVELS 4
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

typedef struct
{        
    uint64_t id;

    AddressSpace* addressSpace;
    FileTable* fileTable;

    uint8_t killed;

    _Atomic uint64_t threadCount;
    _Atomic uint64_t newTid;
} Process;

typedef struct
{
    Process* process;

    uint64_t id;

    void* kernelStackTop;
    void* kernelStackBottom;

    uint64_t timeEnd;
    uint64_t timeStart;

    InterruptFrame* interruptFrame;
    Status status;
    
    uint8_t state;
    uint8_t priority;
    uint8_t boost;
} Thread;

Process* process_new(void);

Thread* thread_new(Process* process, void* entry, uint8_t priority);

void thread_free(Thread* thread);