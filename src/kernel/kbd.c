#include "kbd.h"
#include "lock.h"
#include "sys/kbd.h"
#include "sysfs.h"
#include "time.h"

#include <stdlib.h>
#include <sys/math.h>

static uint64_t kbd_read(file_t* file, void* buffer, uint64_t count)
{
    kbd_t* kbd = file->private;

    count = ROUND_DOWN(count, sizeof(kbd_event_t));
    for (uint64_t i = 0; i < count / sizeof(kbd_event_t); i++)
    {
        if (SCHED_BLOCK_LOCK(&kbd->blocker, &kbd->lock, file->pos != kbd->writeIndex) != BLOCK_NORM)
        {
            lock_release(&kbd->lock);
            return i * sizeof(kbd_event_t);
        }

        ((kbd_event_t*)buffer)[i] = kbd->events[file->pos];
        file->pos = (file->pos + 1) % KBD_MAX_EVENT;

        lock_release(&kbd->lock);
    }

    return count;
}

static uint64_t kbd_status(file_t* file, poll_file_t* pollFile)
{
    kbd_t* kbd = file->private;
    pollFile->occurred = POLL_READ & (kbd->writeIndex != file->pos);
    return 0;
}

static file_ops_t fileOps = {
    .read = kbd_read,
    .status = kbd_status,
};

static void kbd_delete(void* private)
{
    kbd_t* kbd = private;
    free(kbd);
}

kbd_t* kbd_new(const char* name)
{
    kbd_t* kbd = malloc(sizeof(kbd_t));
    kbd->writeIndex = 0;
    kbd->mods = KBD_MOD_NONE;
    kbd->resource = sysfs_expose("/kbd", name, &fileOps, kbd, NULL, kbd_delete);
    blocker_init(&kbd->blocker);
    lock_init(&kbd->lock);

    return kbd;
}

void kbd_free(kbd_t* kbd)
{
    sysfs_hide(kbd->resource);
}

static void kbd_update_mod(kbd_t* kbd, kbd_event_type_t type, kbd_mods_t mod)
{
    if (type == KBD_PRESS)
    {
        kbd->mods |= mod;
    }
    else if (type == KBD_RELEASE)
    {
        kbd->mods &= ~mod;
    }
}

void kbd_push(kbd_t* kbd, kbd_event_type_t type, keycode_t code)
{
    LOCK_GUARD(&kbd->lock);

    switch (code)
    {
    case KEY_CAPS_LOCK:
        kbd_update_mod(kbd, type, KBD_MOD_CAPS);
        break;
    case KEY_LEFT_SHIFT:
    case KEY_RIGHT_SHIFT:
        kbd_update_mod(kbd, type, KBD_MOD_SHIFT);
        break;
    case KEY_LEFT_CTRL:
    case KEY_RIGHT_CTRL:
        kbd_update_mod(kbd, type, KBD_MOD_CTRL);
        break;
    case KEY_LEFT_ALT:
    case KEY_RIGHT_ALT:
        kbd_update_mod(kbd, type, KBD_MOD_ALT);
        break;
    case KEY_LEFT_SUPER:
    case KEY_RIGHT_SUPER:
        kbd_update_mod(kbd, type, KBD_MOD_SUPER);
        break;
    default:
        break;
    }

    kbd->events[kbd->writeIndex] = (kbd_event_t){
        .time = time_uptime(),
        .code = code,
        .mods = kbd->mods,
        .type = type,
    };
    kbd->writeIndex = (kbd->writeIndex + 1) % KBD_MAX_EVENT;
    sched_unblock(&kbd->blocker);
}
