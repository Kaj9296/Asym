#include "process.h"

#include "heap/heap.h"
#include "string/string.h"
#include "page_allocator/page_allocator.h"
#include "tty/tty.h"
#include "lock/lock.h"
#include "gdt/gdt.h"
#include "debug/debug.h"

#include <stdatomic.h>

atomic_size_t pid;

void pid_init()
{
    atomic_init(&pid, 1);
}

uint64_t pid_new()
{
    return atomic_fetch_add_explicit(&pid, 1, memory_order_seq_cst);
}

Process* process_new()
{
    Process* newProcess = kmalloc(sizeof(Process));

    newProcess->pageDirectory = page_directory_new();
    newProcess->firstBlock = 0;
    newProcess->lastBlock = 0;
    newProcess->id = pid_new();
    newProcess->taskAmount = 0;

    process_allocate_pages(newProcess, PROCESS_ADDRESS_SPACE_USER_STACK, 1);
    
    return newProcess;
}

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t pageAmount)
{
    ProcessBlock* newBlock = kmalloc(sizeof(ProcessBlock));

    void* physicalAddress = page_allocator_request_amount(pageAmount);

    newBlock->physicalAddress = physicalAddress;
    newBlock->virtualAddress = virtualAddress;
    newBlock->pageAmount = pageAmount;
    newBlock->next = 0;

    if (process->firstBlock == 0)
    {
        process->firstBlock = newBlock;
        process->lastBlock = newBlock;
    }
    else
    {
        process->lastBlock->next = newBlock;
        process->lastBlock = newBlock;
    }
    
    page_directory_remap_pages(process->pageDirectory, virtualAddress, physicalAddress, pageAmount, PAGE_DIR_READ_WRITE | PAGE_DIR_USER_SUPERVISOR);

    return physicalAddress;
}

Task* task_new(Process* process, InterruptFrame* interruptFrame, uint8_t priority)
{
    if (priority > TASK_PRIORITY_MAX)
    {
        debug_panic("Priority level out of bounds");
    }

    process->taskAmount++;

    Task* newTask = kmalloc(sizeof(Task));
    newTask->process = process;
    newTask->interruptFrame = interruptFrame;
    newTask->state = TASK_STATE_READY;
    newTask->priority = priority;

    return newTask;
}

void task_free(Task* task)
{        
    Process* process = task->process;

    process->taskAmount--;
    if (process->taskAmount == 0)
    {
        page_directory_free(process->pageDirectory);

        if (process->firstBlock != 0)
        {
            ProcessBlock* currentBlock = process->firstBlock;
            while (1)
            {
                ProcessBlock* nextBlock = currentBlock->next;

                page_allocator_unlock_pages(currentBlock->physicalAddress, currentBlock->pageAmount);

                kfree(currentBlock);           

                if (nextBlock != 0)
                {
                    currentBlock = nextBlock;
                }
                else
                {
                    break;
                }
            }
        }

        kfree(process);
    }

    interrupt_frame_free(task->interruptFrame);
    kfree(task);
}