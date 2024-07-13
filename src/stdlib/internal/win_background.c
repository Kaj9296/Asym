#include "win_background.h"

#include <sys/mouse.h>

#ifndef __EMBED__

static void win_draw_topbar(win_t* window, surface_t* surface)
{
    rect_t localArea = RECT_INIT_SURFACE(surface);

    rect_t topBar = (rect_t){
        .left = localArea.left + theme.edgeWidth + 2,
        .top = localArea.top + theme.edgeWidth + 2,
        .right = localArea.right - theme.edgeWidth - 2,
        .bottom = localArea.top + theme.topbarHeight + theme.edgeWidth - 2,
    };
    gfx_rect(surface, &topBar, window->selected ? theme.selected : theme.unSelected);
    gfx_edge(surface, &topBar, theme.edgeWidth, theme.shadow, theme.highlight);
}

static void win_draw_decorations(win_t* window, surface_t* surface)
{
    if (window->type == DWM_WINDOW)
    {
        rect_t localArea = RECT_INIT_SURFACE(surface);

        gfx_rect(surface, &localArea, theme.background);
        gfx_edge(surface, &localArea, theme.edgeWidth, theme.highlight, theme.shadow);

        win_draw_topbar(window, surface);
    }
}

void win_background_procedure(win_t* window, surface_t* surface, const msg_t* msg)
{
    switch (msg->type)
    {
    case MSG_MOUSE:
    {
        msg_mouse_t* data = (msg_mouse_t*)msg->data;
        if (window->type != DWM_WINDOW)
        {
            break;
        }

        rect_t topBar = (rect_t){
            .left = window->pos.x + theme.edgeWidth,
            .top = window->pos.y + theme.edgeWidth,
            .right = window->pos.x + window->width - theme.edgeWidth,
            .bottom = window->pos.y + theme.topbarHeight + theme.edgeWidth,
        };

        if (window->moving)
        {
            rect_t rect =
                RECT_INIT_DIM(window->pos.x + data->deltaX, window->pos.y + data->deltaY, window->width, window->height);
            win_move(window, &rect);

            if (!(data->buttons & MOUSE_LEFT))
            {
                window->moving = false;
            }
        }
        else if (RECT_CONTAINS_POINT(&topBar, data->pos.x, data->pos.y) && data->buttons & MOUSE_LEFT)
        {
            window->moving = true;
        }
    }
    break;
    case MSG_SELECT:
    {
        window->selected = true;
        if (window->type == DWM_WINDOW)
        {
            win_draw_topbar(window, surface);
        }
    }
    break;
    case MSG_DESELECT:
    {
        window->selected = false;
        if (window->type == DWM_WINDOW)
        {
            win_draw_topbar(window, surface);
        }
    }
    break;
    case LMSG_REDRAW:
    {
        if (window->type == DWM_WINDOW)
        {
            win_draw_decorations(window, surface);
        }
    }
    break;
    }
}

#endif
