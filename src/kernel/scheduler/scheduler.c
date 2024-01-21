#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"
#include "idt/idt.h"
#include "time/time.h"
#include "gdt/gdt.h"
#include "apic/apic.h"

Scheduler* schedulers;

uint64_t nextBalancing;

Scheduler* least_loaded_scheduler()
{
    uint64_t shortestLength = -1;
    Scheduler* leastLoadedScheduler = 0;
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        uint64_t length = 0;
        for (int64_t p = TASK_PRIORITY_LEVELS - 1; p >= 0; p--) 
        {
            length += queue_length(schedulers[i].queues[p]);
        }
        if (schedulers[i].runningTask != 0)
        {
            length += 1;
        }

        if (shortestLength > length)
        {
            shortestLength = length;
            leastLoadedScheduler = &schedulers[i];
        }
    }

    return leastLoadedScheduler;
}

void scheduler_init()
{
    tty_start_message("Scheduler initializing");
    schedulers = kmalloc(sizeof(Scheduler) * smp_cpu_amount());

    nextBalancing = 0;

    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        schedulers[i].cpu = smp_cpu(i);

        for (uint64_t p = 0; p < TASK_PRIORITY_LEVELS; p++)    
        {
            schedulers[i].queues[p] = queue_new();
        }
        schedulers[i].runningTask = 0;

        schedulers[i].nextPreemption = 0;
        schedulers[i].lock = spin_lock_new();
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_tick(InterruptFrame* interruptFrame)
{
    if (nextBalancing <= time_nanoseconds())
    {
        scheduler_balance();

        nextBalancing = time_nanoseconds() + SCHEDULER_BALANCING_PERIOD;
    }
}

void scheduler_balance()
{
    scheduler_acquire_all();
    
    uint64_t totalTasks = 0;
    for (int i = 0; i < smp_cpu_amount(); i++)
    {
        totalTasks += queue_length(schedulers[i].queues[TASK_PRIORITY_NORMAL]);
        if (schedulers[i].runningTask != 0)
        {
            totalTasks++;
        }
    }

    uint64_t averageTasks = totalTasks / smp_cpu_amount();

    for (int j = 0; j < SCHEDULER_BALANCING_ITERATIONS; j++)
    {
        Task* poppedTask = 0;
        for (int i = 0; i < smp_cpu_amount(); i++)
        {
            Queue* queue = schedulers[i].queues[TASK_PRIORITY_NORMAL];
            uint64_t queueLength = queue_length(queue);

            uint64_t taskAmount = queueLength;
            if (schedulers[i].runningTask != 0)
            {
                taskAmount += 1;
            }

            if (queueLength != 0 && taskAmount > averageTasks && poppedTask == 0)
            {
                poppedTask = queue_pop(queue);
            }
            else if (taskAmount < averageTasks && poppedTask != 0)
            {
                queue_push(queue, poppedTask);
                poppedTask = 0;
            }
        }
    
        if (poppedTask != 0)
        {
            for (int i = 0; i < smp_cpu_amount(); i++)
            {
                queue_push(schedulers[i].queues[TASK_PRIORITY_NORMAL], poppedTask);
                break;
            }
        }
    }

    scheduler_release_all();
}

void scheduler_acquire_all()
{
    for (int i = 0; i < smp_cpu_amount(); i++)
    {  
        spin_lock_acquire(&schedulers[i].lock);
    }
}

void scheduler_release_all()
{
    for (int i = 0; i < smp_cpu_amount(); i++)
    {
        spin_lock_release(&schedulers[i].lock);
    }
}

void scheduler_push(Process* process, InterruptFrame* interruptFrame, uint8_t priority)
{
    if (priority >= TASK_PRIORITY_LEVELS)
    {
        debug_panic("Priority level out of bounds");
    }

    Task* newTask = kmalloc(sizeof(Task));
    newTask->process = process;
    newTask->interruptFrame = interruptFrame;
    newTask->state = TASK_STATE_READY;
    newTask->priority = priority;

    scheduler_acquire_all();

    Scheduler* scheduler = least_loaded_scheduler();
    queue_push(scheduler->queues[priority], newTask);

    scheduler_release_all();
}

Scheduler* scheduler_get_local()
{
    return &schedulers[smp_current_cpu()->id];
}

void local_scheduler_push(Process* process, InterruptFrame* interruptFrame, uint8_t priority)
{
    if (priority >= TASK_PRIORITY_LEVELS)
    {
        debug_panic("Priority level out of bounds");
    }

    Task* newTask = kmalloc(sizeof(Task));
    newTask->process = process;
    newTask->interruptFrame = interruptFrame;
    newTask->state = TASK_STATE_READY;
    newTask->priority = priority;

    Scheduler* scheduler = scheduler_get_local();
    queue_push(scheduler->queues[priority], newTask);
}

void local_scheduler_tick(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_get_local();

    if (scheduler->nextPreemption < time_nanoseconds())
    {
        local_scheduler_schedule(interruptFrame);
    }
    else if (scheduler->runningTask != 0)
    {
        uint8_t runningPriority = scheduler->runningTask->priority;
        for (int64_t i = TASK_PRIORITY_LEVELS - 1; i > runningPriority; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                local_scheduler_schedule(interruptFrame);
                break;
            }
        }
    }
}

void local_scheduler_schedule(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_get_local();
        
    Task* newTask = 0;
    for (int64_t i = TASK_PRIORITY_LEVELS - 1; i >= 0; i--) 
    {
        if (queue_length(scheduler->queues[i]) != 0)
        {
            newTask = queue_pop(scheduler->queues[i]);
            break;
        }
    }        

    if (newTask != 0)
    {
        if (scheduler->runningTask != 0)
        {
            Task* oldTask = scheduler->runningTask;
            interrupt_frame_copy(oldTask->interruptFrame, interruptFrame);

            oldTask->state = TASK_STATE_READY;
            oldTask->priority = TASK_PRIORITY_NORMAL;
            queue_push(scheduler->queues[TASK_PRIORITY_NORMAL], oldTask);                
        }

        newTask->state = TASK_STATE_RUNNING;
        scheduler->runningTask = newTask;

        interrupt_frame_copy(interruptFrame, newTask->interruptFrame);

        scheduler->nextPreemption = time_nanoseconds() + SCHEDULER_TIME_SLICE;
    }
    else if (scheduler->runningTask == 0)
    {
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->cr3 = (uint64_t)kernelPageDirectory;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->stackPointer = tss_get(smp_current_cpu()->id)->rsp0;

        scheduler->nextPreemption = 0;
    }
    else
    {
        //Keep running the same task, change to 0?
        scheduler->nextPreemption = time_nanoseconds() + SCHEDULER_TIME_SLICE;
    }
}

void local_scheduler_block(InterruptFrame* interruptFrame, Blocker blocker)
{    
    /*Scheduler* scheduler = scheduler_get_local();

    BlockedTask* blockedTasks = vector_array(scheduler->blockedTasks);
    uint64_t i = 0;
    for (; i < vector_length(scheduler->blockedTasks); i++)
    {
        if (blockedTasks[i].blocker.timeout >= blocker.timeout)
        {
            vector_insert(scheduler->blockedTasks, i, &blocker);
            return;
        }
    }

    vector_push(scheduler->blockedTasks, &blocker);*/
}   

void local_scheduler_exit()
{
    Scheduler* scheduler = scheduler_get_local();

    process_free(scheduler->runningTask->process);
    interrupt_frame_free(scheduler->runningTask->interruptFrame);
    kfree(scheduler->runningTask);

    scheduler->runningTask = 0;
}

void local_scheduler_acquire()
{
    spin_lock_acquire(&scheduler_get_local()->lock);
}

void local_scheduler_release()
{
    spin_lock_release(&scheduler_get_local()->lock);
}

uint64_t local_scheduler_task_amount()
{
    Scheduler* scheduler = scheduler_get_local();

    uint64_t amount = 0;
    for (int i = 0; i < TASK_PRIORITY_LEVELS; i++)
    {
        amount += queue_length(scheduler->queues[i]);
    }
    if (scheduler->runningTask != 0)
    {
        amount += 1;
    }

    return amount;
}

Task* local_scheduler_running_task()
{
    return scheduler_get_local()->runningTask;
}