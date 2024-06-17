#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>

#include "libs/std/internal/heap.h"

// TODO: Implement correct alignment rules

#ifdef __KERNEL__
#include "lock.h"
#include "pmm.h"
#include "vmm.h"

static Lock lock;
static uintptr_t newAddress;

extern uint64_t _kernelEnd;
#else
static fd_t zeroResource;
#endif

#define ROUND_UP(number, multiple) \
    ((((uint64_t)(number) + (uint64_t)(multiple) - 1) / (uint64_t)(multiple)) * (uint64_t)(multiple))

static heap_header_t* firstBlock;

static void _HeapBlockSplit(heap_header_t* block, uint64_t size)
{
    heap_header_t* newBlock = (heap_header_t*)((uint64_t)block + sizeof(heap_header_t) + size);
    newBlock->size = block->size - sizeof(heap_header_t) - size;
    newBlock->next = block->next;
    newBlock->reserved = false;
    newBlock->magic = HEAP_HEADER_MAGIC;

    block->size = size;
    block->next = newBlock;
}

#ifdef __KERNEL__
static heap_header_t* _HeapBlockNew(uint64_t size)
{
    uint64_t pageAmount = SIZE_IN_PAGES(size + sizeof(heap_header_t));

    heap_header_t* newBlock = (heap_header_t*)newAddress;
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        vmm_kernel_map((void*)(newAddress + i * PAGE_SIZE), pmm_alloc(), PAGE_SIZE);
    }
    newAddress += pageAmount * PAGE_SIZE;

    newBlock->size = pageAmount * PAGE_SIZE - sizeof(heap_header_t);
    newBlock->next = NULL;
    newBlock->reserved = false;
    newBlock->magic = HEAP_HEADER_MAGIC;

    return newBlock;
}

void _HeapAcquire(void)
{
    lock_acquire(&lock);
}

void _HeapRelease(void)
{
    lock_release(&lock);
}

void _HeapInit(void)
{
    newAddress = ROUND_UP((uint64_t)&_kernelEnd, PAGE_SIZE);
    firstBlock = _HeapBlockNew(PAGE_SIZE);

    lock_init(&lock);
}
#else
static heap_header_t* _HeapBlockNew(uint64_t size)
{
    size = ROUND_UP(size + sizeof(heap_header_t), PAGE_SIZE);

    heap_header_t* newBlock = mmap(zeroResource, NULL, size, PROT_READ | PROT_WRITE);
    if (newBlock == NULL)
    {
        return NULL;
    }

    newBlock->size = size - sizeof(heap_header_t);
    newBlock->next = NULL;
    newBlock->reserved = false;
    newBlock->magic = HEAP_HEADER_MAGIC;

    return newBlock;
}

// TODO: Implement user space mutex
void _HeapAcquire(void)
{
}

void _HeapRelease(void)
{
}

void _HeapInit(void)
{
    zeroResource = open("sys:/const/zero");
    firstBlock = _HeapBlockNew(PAGE_SIZE - sizeof(heap_header_t));
}
#endif

void* malloc(size_t size)
{
    _HeapAcquire();

    if (size == 0)
    {
        return NULL;
    }
    size = ROUND_UP(size, HEAP_ALIGNMENT);

    heap_header_t* currentBlock = firstBlock;
    while (true)
    {
        if (!currentBlock->reserved)
        {
            if (currentBlock->size == size)
            {
                currentBlock->reserved = true;

                _HeapRelease();
                return HEAP_HEADER_GET_START(currentBlock);
            }
            else if (currentBlock->size > size + sizeof(heap_header_t) + HEAP_ALIGNMENT)
            {
                currentBlock->reserved = true;
                _HeapBlockSplit(currentBlock, size);

                _HeapRelease();
                return HEAP_HEADER_GET_START(currentBlock);
            }
        }

        if (currentBlock->next != NULL)
        {
            currentBlock = currentBlock->next;
        }
        else
        {
            break;
        }
    }

    heap_header_t* newBlock = _HeapBlockNew(size);
    if (newBlock == NULL)
    {
        return NULL;
    }

    if (newBlock->size > size + sizeof(heap_header_t) + HEAP_ALIGNMENT)
    {
        _HeapBlockSplit(newBlock, size);
    }
    currentBlock->next = newBlock;
    newBlock->reserved = true;

    _HeapRelease();
    return HEAP_HEADER_GET_START(newBlock);
}
