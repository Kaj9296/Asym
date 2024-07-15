#pragma once

#include "defs.h"
#include "lock.h"
#include "log.h"

#include <sys/list.h>

typedef struct
{
    uint64_t length;
    list_t list;
    lock_t lock;
} queue_t;

static inline void queue_init(queue_t* queue)
{
    queue->length = 0;
    list_init(&queue->list);
    lock_init(&queue->lock);
}

static inline void queue_push(queue_t* queue, void* element)
{
    LOCK_GUARD(&queue->lock);

    queue->length++;
    list_push(&queue->list, element);
}

static inline void* queue_pop(queue_t* queue)
{
    LOCK_GUARD(&queue->lock);
    if (queue->length == 0)
    {
        return NULL;
    }

    queue->length--;
    return list_pop(&queue->list);
}

static inline uint64_t queue_length(const queue_t* queue)
{
    return queue->length;
}

static inline void* queue_find(queue_t* queue, bool (*predicate)(void*))
{
    LOCK_GUARD(&queue->lock);

    void* elem;
    LIST_FOR_EACH(elem, &queue->list)
    {
        list_entry_t* entry = elem;

        log_print("%a, %a, %a, %a", &queue->list.head, entry, entry->next, entry->prev);
        // log_print("loop begin");
        if (predicate(elem))
        {
            log_print("loop ret");
            return elem;
        }
        // log_print("loop end");
    }

    log_print("loop NULL");
    return NULL;
}
