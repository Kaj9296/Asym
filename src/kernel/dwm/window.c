#include "window.h"

#include "dwm.h"
#include "dwm/msg_queue.h"
#include "sched.h"
#include "vfs.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/dwm.h>
#include <sys/gfx.h>
#include <sys/math.h>

static void window_cleanup(file_t* file)
{
    window_t* window = file->private;

    window->cleanup(window);

    window_free(window);
}

static uint64_t window_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    window_t* window = file->private;

    switch (request)
    {
    case IOCTL_WINDOW_RECEIVE:
    {
        if (size != sizeof(ioctl_window_receive_t))
        {
            return ERROR(EINVAL);
        }
        ioctl_window_receive_t* receive = argp;

        msg_queue_pop(&window->messages, &receive->outMsg, receive->timeout);

        return 0;
    }
    case IOCTL_WINDOW_SEND:
    {
        if (size != sizeof(ioctl_window_send_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_window_send_t* send = argp;

        msg_queue_push(&window->messages, send->msg.type, send->msg.data, MSG_MAX_DATA);

        return 0;
    }
    case IOCTL_WINDOW_MOVE:
    {
        if (size != sizeof(ioctl_window_move_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_window_move_t* move = argp;

        LOCK_GUARD(&window->lock);
        window->pos = move->pos;

        if (window->gfx.width != move->width || window->gfx.height != move->height)
        {
            window->gfx.width = move->width;
            window->gfx.height = move->height;
            window->gfx.stride = move->width;

            free(window->gfx.buffer);
            window->gfx.buffer = calloc(move->width * move->height, sizeof(pixel_t));
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
    window_t* window = file->private;
    LOCK_GUARD(&window->lock);

    if (size != window->gfx.width * window->gfx.height * sizeof(pixel_t))
    {
        return ERROR(EBUFFER);
    }

    if (rect->left > rect->right || rect->top > rect->bottom || rect->right > window->gfx.width ||
        rect->bottom > window->gfx.height)
    {
        return ERROR(EINVAL);
    }

    for (int64_t y = 0; y < RECT_HEIGHT(rect); y++)
    {
        uint64_t index = rect->left + (rect->top + y) * window->gfx.width;
        memcpy(&window->gfx.buffer[index], &((pixel_t*)buffer)[index], RECT_WIDTH(rect) * sizeof(pixel_t));
    }

    gfx_invalidate(&window->gfx, rect);

    if (!window->shown)
    {
        window->moved = true;
    }
    window->invalid = true;

    dwm_redraw();
    return 0;
}

static uint64_t window_status(file_t* file, poll_file_t* pollFile)
{
    window_t* window = file->private;

    pollFile->occurred = POLL_READ & msg_queue_avail(&window->messages);
    return 0;
}

static file_ops_t fileOps = {
    .cleanup = window_cleanup,
    .ioctl = window_ioctl,
    .flush = window_flush,
    .status = window_status,
};

window_t* window_new(const point_t* pos, uint32_t width, uint32_t height, dwm_type_t type, void (*cleanup)(window_t*))
{
    if (type > DWM_MAX)
    {
        return NULL;
    }

    window_t* window = malloc(sizeof(window_t));
    list_entry_init(&window->base);
    window->pos = *pos;
    window->type = type;
    window->gfx.buffer = calloc(width * height, sizeof(pixel_t));
    window->gfx.width = width;
    window->gfx.height = height;
    window->gfx.stride = width;
    window->gfx.invalidRect = RECT_INIT_DIM(0, 0, width, height);
    window->invalid = false;
    window->moved = false;
    window->shown = false;
    window->prevRect = RECT_INIT_DIM(0, 0, 0, 0);
    window->cleanup = cleanup;
    lock_init(&window->lock);
    msg_queue_init(&window->messages);

    return window;
}

void window_free(window_t* window)
{
    msg_queue_cleanup(&window->messages);
    free(window->gfx.buffer);
    free(window);
}

void window_populate_file(window_t* window, file_t* file)
{
    file->private = window;
    file->ops = &fileOps;
}
