#pragma once

#include <stdint.h>
#include <sys/mouse.h>

#include "sysfs.h"

#define MOUSE_MAX_EVENT 32

typedef struct
{
    mouse_event_t events[MOUSE_MAX_EVENT];
    uint64_t writeIndex;
    resource_t* resource;
    blocker_t blocker;
    lock_t lock;
} mouse_t;

mouse_t* mouse_new(const char* name);

void mouse_free(mouse_t* mouse);

void mouse_push(mouse_t* mouse, mouse_buttons_t buttons, const point_t* delta);
