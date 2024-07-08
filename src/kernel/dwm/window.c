#include "window.h"

#include "dwm.h"
#include "log.h"
#include "sched.h"
#include "sys/io.h"
#include "vfs.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/math.h>

static void window_cleanup(file_t* file)
{
    window_t* window = file->internal;

    window->cleanup(window);

    window_free(window);
}

static uint64_t window_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    window_t* window = file->internal;

    switch (request)
    {
    case IOCTL_WIN_RECEIVE:
    {
        if (size != sizeof(ioctl_win_receive_t))
        {
            return ERROR(EINVAL);
        }
        ioctl_win_receive_t* receive = argp;

        message_t message;
        if (SCHED_WAIT(message_queue_pop(&window->messages, &message), receive->timeout) == SCHED_WAIT_TIMEOUT)
        {
            receive->outType = MSG_NONE;
            return 0;
        }

        memcpy(receive->outData, message.data, message.size);
        receive->outType = message.type;

        return 0;
    }
    case IOCTL_WIN_SEND:
    {
        if (size != sizeof(ioctl_win_send_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_win_send_t* send = argp;

        message_queue_push(&window->messages, send->type, send->data, MSG_MAX_DATA);

        return 0;
    }
    case IOCTL_WIN_MOVE:
    {
        if (size != sizeof(ioctl_win_move_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_win_move_t* move = argp;

        LOCK_GUARD(&window->lock);
        window->pos = move->pos;

        if (window->surface.width != move->width || window->surface.height != move->height)
        {
            window->surface.width = move->width;
            window->surface.height = move->height;
            window->surface.stride = move->width;

            free(window->surface.buffer);
            window->surface.buffer = calloc(move->width * move->height, sizeof(pixel_t));
        }

        window->moved = true;
        dwm_redraw();
        return 0;
    }
    default:
    {
        return ERROR(EREQ);
    }
    }
}

static uint64_t window_flush(file_t* file, const void* buffer, uint64_t size, const rect_t* rect)
{
    window_t* window = file->internal;
    LOCK_GUARD(&window->lock);

    if (size != window->surface.width * window->surface.height * sizeof(pixel_t))
    {
        return ERROR(EBUFFER);
    }

    if (rect->left > rect->right || rect->top > rect->bottom || rect->right > window->surface.width ||
        rect->bottom > window->surface.height)
    {
        return ERROR(EINVAL);
    }

    for (int64_t y = 0; y < RECT_HEIGHT(rect); y++)
    {
        uint64_t index = rect->left + (rect->top + y) * window->surface.width;
        memcpy(&window->surface.buffer[index], &((pixel_t*)buffer)[index], RECT_WIDTH(rect) * sizeof(pixel_t));
    }

    gfx_invalidate(&window->surface, rect);

    window->invalid = true;
    dwm_redraw();
    return 0;
}

static uint64_t window_status(file_t* file, poll_file_t* pollFile)
{
    window_t* window = file->internal;

    pollFile->occurred = POLL_READ & message_queue_avail(&window->messages);
    return 0;
}

static file_ops_t fileOps = {
    .cleanup = window_cleanup,
    .ioctl = window_ioctl,
    .flush = window_flush,
    .status = window_status,
};

window_t* window_new(const point_t* pos, uint32_t width, uint32_t height, win_type_t type, void (*cleanup)(window_t*))
{
    if (type > WIN_MAX)
    {
        return NULL;
    }

    window_t* window = malloc(sizeof(window_t));
    list_entry_init(&window->base);
    window->pos = *pos;
    window->type = type;
    window->surface.buffer = calloc(width * height, sizeof(pixel_t));
    window->surface.width = width;
    window->surface.height = height;
    window->surface.stride = width;
    window->surface.invalidArea = RECT_INIT_DIM(0, 0, width, height);
    window->invalid = false;
    window->moved = false;
    window->prevRect = RECT_INIT_DIM(pos->x, pos->y, width, height);
    window->cleanup = cleanup;
    lock_init(&window->lock);
    message_queue_init(&window->messages);

    return window;
}

void window_free(window_t* window)
{
    free(window->surface.buffer);
    free(window);
}

void window_populate_file(window_t* window, file_t* file)
{
    file->internal = window;
    file->ops = &fileOps;
}
