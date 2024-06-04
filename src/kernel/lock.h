#pragma once

#include <stdatomic.h>

#include "defs.h"

typedef struct
{
    _Atomic(uint32_t) nextTicket;
    _Atomic(uint32_t) nowServing;
} Lock;

#define LOCK_GUARD(lock) \
    __attribute__((cleanup(lock_cleanup))) Lock* CONCAT(l, __COUNTER__) = (lock); \
    lock_acquire((lock))

void lock_init(Lock* lock);

void lock_acquire(Lock* lock);

void lock_release(Lock* lock);

static inline void lock_cleanup(Lock** lock)
{
    lock_release(*lock);
}