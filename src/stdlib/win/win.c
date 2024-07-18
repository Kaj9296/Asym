#ifndef __EMBED__

typedef struct win win_t;
typedef struct widget widget_t;

#define _WIN_INTERNAL
#include <sys/win.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/mouse.h>

#define WIN_WIDGET_MAX_MSG 8

typedef struct win
{
    fd_t fd;
    pixel_t* buffer;
    point_t pos;
    uint32_t width;
    uint32_t height;
    rect_t clientArea;
    dwm_type_t type;
    win_proc_t procedure;
    list_t widgets;
    bool selected;
    bool moving;
    gfx_psf_t psf;
    char name[DWM_MAX_NAME];
} win_t;

typedef struct widget
{
    list_entry_t base;
    widget_id_t id;
    widget_proc_t procedure;
    rect_t rect;
    win_t* window;
    void* private;
    msg_t messages[WIN_WIDGET_MAX_MSG];
    uint8_t writeIndex;
    uint8_t readIndex;
    char name[DWM_MAX_NAME];
} widget_t;

// TODO: this should be stored in some sort of config file, lua? make something custom?
#define WIN_DEFAULT_FONT "home:/usr/fonts/zap-vga16.psf"
win_theme_t theme = {
    .edgeWidth = 3,
    .rimWidth = 3,
    .ridgeWidth = 2,
    .highlight = 0xFFFCFCFC,
    .shadow = 0xFF6F6F6F,
    .bright = 0xFFFFFFFF,
    .dark = 0xFF000000,
    .background = 0xFFBFBFBF,
    .selected = 0xFF00007F,
    .unSelected = 0xFF7F7F7F,
    .topbarHeight = 40,
    .topbarPadding = 2,
};

static uint64_t win_widget_dispatch(widget_t* widget, const msg_t* msg);

static uint64_t win_set_area(win_t* window, const rect_t* rect)
{
    window->pos = (point_t){.x = rect->left, .y = rect->top};
    window->width = RECT_WIDTH(rect);
    window->height = RECT_HEIGHT(rect);

    window->clientArea = RECT_INIT_DIM(0, 0, window->width, window->height);
    win_shrink_to_client(&window->clientArea, window->type);

    return 0;
}

static inline void win_window_surface(win_t* window, gfx_t* gfx)
{
    gfx->invalidArea = (rect_t){0};
    gfx->buffer = window->buffer;
    gfx->width = window->width;
    gfx->height = window->height;
    gfx->stride = gfx->width;
}

static inline void win_client_surface(win_t* window, gfx_t* gfx)
{
    gfx->invalidArea = (rect_t){0};
    gfx->width = RECT_WIDTH(&window->clientArea);
    gfx->height = RECT_HEIGHT(&window->clientArea);
    gfx->stride = window->width;
    gfx->buffer = (pixel_t*)((uint64_t)window->buffer + (window->clientArea.left * sizeof(pixel_t)) +
        (window->clientArea.top * gfx->stride * sizeof(pixel_t)));
}

static void win_draw_close_button(win_t* window, gfx_t* gfx, const rect_t* topbar)
{
    uint64_t width = RECT_HEIGHT(topbar);

    rect_t rect = {
        .left = topbar->right - width,
        .top = topbar->top,
        .right = topbar->right,
        .bottom = topbar->bottom,
    };

    gfx_rim(gfx, &rect, theme.rimWidth, theme.dark);
    RECT_SHRINK(&rect, theme.rimWidth);

    gfx_edge(gfx, &rect, theme.edgeWidth, theme.highlight, theme.shadow);
    RECT_SHRINK(&rect, theme.edgeWidth);
    gfx_rect(gfx, &rect, theme.background);

    RECT_EXPAND(&rect, 32);
    gfx_psf(gfx, &window->psf, &rect, GFX_CENTER, GFX_CENTER, 32, "x", theme.shadow, 0);
}

static void win_draw_topbar(win_t* window, gfx_t* gfx)
{
    rect_t rect = {
        .left = theme.edgeWidth + theme.topbarPadding,
        .top = theme.edgeWidth + theme.topbarPadding,
        .right = gfx->width - theme.edgeWidth - theme.topbarPadding,
        .bottom = theme.topbarHeight + theme.edgeWidth - theme.topbarPadding,
    };
    gfx_edge(gfx, &rect, theme.edgeWidth, theme.dark, theme.highlight);
    RECT_SHRINK(&rect, theme.edgeWidth);
    gfx_rect(gfx, &rect, window->selected ? theme.selected : theme.unSelected);

    win_draw_close_button(window, gfx, &rect);

    rect.left += theme.topbarPadding * 3;
    rect.right -= theme.topbarHeight;
    gfx_psf(gfx, &window->psf, &rect, GFX_MIN, GFX_CENTER, 16, "Calculator", theme.background, 0);
}

static void win_draw_border_and_background(win_t* window, gfx_t* gfx)
{
    if (window->type == DWM_WINDOW)
    {
        rect_t localArea = RECT_INIT_GFX(gfx);

        gfx_rect(gfx, &localArea, theme.background);
        gfx_edge(gfx, &localArea, theme.edgeWidth, theme.bright, theme.dark);
    }
}

static void win_handle_drag(win_t* window, const msg_mouse_t* data)
{
    rect_t topBar = (rect_t){
        .left = window->pos.x + theme.edgeWidth,
        .top = window->pos.y + theme.edgeWidth,
        .right = window->pos.x + window->width - theme.edgeWidth,
        .bottom = window->pos.y + theme.topbarHeight + theme.edgeWidth,
    };

    if (window->moving)
    {
        rect_t rect = RECT_INIT_DIM(window->pos.x + data->deltaX, window->pos.y + data->deltaY, window->width, window->height);
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

static void win_background_procedure(win_t* window, const msg_t* msg)
{
    gfx_t gfx;
    win_window_surface(window, &gfx);

    switch (msg->type)
    {
    case MSG_MOUSE:
    {
        msg_mouse_t* data = (msg_mouse_t*)msg->data;

        if (window->type == DWM_WINDOW)
        {
            win_handle_drag(window, data);
        }

        wmsg_mouse_t wmsg;
        wmsg.buttons = data->buttons;
        wmsg.pos = data->pos;
        wmsg.deltaX = data->deltaX;
        wmsg.deltaY = data->deltaY;
        win_widget_send_all(window, WMSG_MOUSE, &wmsg, sizeof(wmsg_mouse_t));
    }
    break;
    case MSG_SELECT:
    {
        window->selected = true;
        if (window->type == DWM_WINDOW)
        {
            win_draw_topbar(window, &gfx);
        }
    }
    break;
    case MSG_DESELECT:
    {
        window->selected = false;
        if (window->type == DWM_WINDOW)
        {
            win_draw_topbar(window, &gfx);
        }
    }
    break;
    case LMSG_REDRAW:
    {
        if (window->type == DWM_WINDOW)
        {
            win_draw_border_and_background(window, &gfx);
            win_draw_topbar(window, &gfx);
        }

        win_widget_send_all(window, WMSG_REDRAW, NULL, 0);
    }
    break;
    }

    if (RECT_AREA(&gfx.invalidArea) != 0 &&
        flush(window->fd, window->buffer, window->width * window->height * sizeof(pixel_t), &gfx.invalidArea) == ERR)
    {
        win_send(window, LMSG_QUIT, NULL, 0);
    }
}

win_t* win_new(const char* name, dwm_type_t type, const rect_t* rect, win_proc_t procedure)
{
    if (RECT_AREA(rect) == 0 || strlen(name) >= DWM_MAX_NAME || name == NULL)
    {
        return NULL;
    }

    win_t* window = malloc(sizeof(win_t));
    if (window == NULL)
    {
        return NULL;
    }

    window->fd = open("sys:/server/dwm");
    if (window->fd == ERR)
    {
        free(window);
        return NULL;
    }

    ioctl_dwm_create_t create;
    create.pos.x = rect->left;
    create.pos.y = rect->top;
    create.width = RECT_WIDTH(rect);
    create.height = RECT_HEIGHT(rect);
    create.type = type;
    strcpy(create.name, name);
    if (ioctl(window->fd, IOCTL_DWM_CREATE, &create, sizeof(ioctl_dwm_create_t)) == ERR)
    {
        free(window);
        close(window->fd);
        return NULL;
    }

    window->buffer = calloc(create.width * create.height, sizeof(pixel_t));
    if (window->buffer == NULL)
    {
        free(window);
        close(window->fd);
        return NULL;
    }

    window->type = type;
    list_init(&window->widgets);
    window->selected = false;
    window->moving = false;
    window->procedure = procedure;
    strcpy(window->name, name);
    win_set_area(window, rect);
    if (gfx_psf_load(&window->psf, WIN_DEFAULT_FONT) == ERR)
    {
        free(window);
        free(window->buffer);
        close(window->fd);
        return NULL;
    }

    win_send(window, LMSG_REDRAW, NULL, 0);

    return window;
}

uint64_t win_free(win_t* window)
{
    if (close(window->fd) == ERR)
    {
        return ERR;
    }

    widget_t* widget;
    LIST_FOR_EACH(widget, &window->widgets)
    {
        msg_t msg = {.type = WMSG_FREE};
        win_widget_dispatch(widget, &msg);
    }

    free(window->buffer);
    free(window);
    return 0;
}

uint64_t win_send(win_t* window, msg_type_t type, const void* data, uint64_t size)
{
    if (size >= MSG_MAX_DATA)
    {
        errno = EINVAL;
        return ERR;
    }

    ioctl_window_send_t send = {.msg.type = type};
    memcpy(send.msg.data, data, size);

    if (ioctl(window->fd, IOCTL_WINDOW_SEND, &send, sizeof(ioctl_window_send_t)) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t win_receive(win_t* window, msg_t* msg, nsec_t timeout)
{
    ioctl_window_receive_t receive = {.timeout = timeout};
    if (ioctl(window->fd, IOCTL_WINDOW_RECEIVE, &receive, sizeof(ioctl_window_receive_t)) == ERR)
    {
        return ERR;
    }

    *msg = receive.outMsg;
    return receive.outMsg.type != MSG_NONE;
}

uint64_t win_dispatch(win_t* window, const msg_t* msg)
{
    win_background_procedure(window, msg);
    uint64_t result = window->procedure(window, msg);

    widget_t* widget;
    LIST_FOR_EACH(widget, &window->widgets)
    {
        while (widget->readIndex != widget->writeIndex)
        {
            win_widget_dispatch(widget, &widget->messages[widget->readIndex]);
            widget->readIndex = (widget->readIndex + 1) % WIN_WIDGET_MAX_MSG;
        }
    }

    return result;
}

uint64_t win_draw_begin(win_t* window, gfx_t* gfx)
{
    win_client_surface(window, gfx);
    return 0;
}

uint64_t win_draw_end(win_t* window, gfx_t* gfx)
{
    rect_t rect = (rect_t){
        .left = window->clientArea.left + gfx->invalidArea.left,
        .top = window->clientArea.top + gfx->invalidArea.top,
        .right = window->clientArea.left + gfx->invalidArea.right,
        .bottom = window->clientArea.top + gfx->invalidArea.bottom,
    };

    if (flush(window->fd, window->buffer, window->width * window->height * sizeof(pixel_t), &rect) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t win_move(win_t* window, const rect_t* rect)
{
    ioctl_window_move_t move;
    move.pos.x = rect->left;
    move.pos.y = rect->top;
    move.width = RECT_WIDTH(rect);
    move.height = RECT_HEIGHT(rect);

    void* newBuffer = NULL;
    if (window->width != move.width || window->height != move.height)
    {
        newBuffer = calloc(move.width * move.height, sizeof(pixel_t));
        if (newBuffer == NULL)
        {
            return ERR;
        }
    }

    if (ioctl(window->fd, IOCTL_WINDOW_MOVE, &move, sizeof(ioctl_window_move_t)) == ERR)
    {
        return ERR;
    }

    if (newBuffer != NULL)
    {
        free(window->buffer);
        window->buffer = newBuffer;

        win_send(window, LMSG_REDRAW, NULL, 0);
    }

    win_set_area(window, rect);

    return 0;
}

const char* win_name(win_t* window)
{
    return window->name;
}

void win_screen_window_area(win_t* window, rect_t* rect)
{
    *rect = RECT_INIT_DIM(window->pos.x, window->pos.y, window->width, window->height);
}

void win_screen_client_area(win_t* window, rect_t* rect)
{
    *rect = (rect_t){
        .left = window->pos.x + window->clientArea.left,
        .top = window->pos.y + window->clientArea.top,
        .right = window->pos.x + window->clientArea.right,
        .bottom = window->pos.y + window->clientArea.bottom,
    };
}

void win_client_area(win_t* window, rect_t* rect)
{
    *rect = window->clientArea;
}

void win_screen_to_window(win_t* window, point_t* point)
{
    point->x -= window->pos.x;
    point->y -= window->pos.y;
}

void win_screen_to_client(win_t* window, point_t* point)
{
    point->x -= window->pos.x + window->clientArea.left;
    point->y -= window->pos.y + window->clientArea.top;
}

void win_window_to_client(win_t* window, point_t* point)
{
    point->x -= window->clientArea.left;
    point->y -= window->clientArea.top;
}

gfx_psf_t* win_font(win_t* window)
{
    return &window->psf;
}

uint64_t win_font_set(win_t* window, const char* path)
{
    gfx_psf_cleanup(&window->psf);
    return gfx_psf_load(&window->psf, path);
}

widget_t* win_widget_new(win_t* window, widget_proc_t procedure, const char* name, const rect_t* rect, widget_id_t id)
{
    if (strlen(name) >= DWM_MAX_NAME)
    {
        return NULL;
    }

    widget_t* widget = malloc(sizeof(widget_t));
    list_entry_init(&widget->base);
    widget->id = id;
    widget->procedure = procedure;
    widget->rect = *rect;
    widget->window = window;
    widget->private = NULL;
    widget->readIndex = 0;
    widget->writeIndex = 0;
    strcpy(widget->name, name);
    list_push(&window->widgets, widget);

    win_widget_send(widget, WMSG_INIT, NULL, 0);
    win_widget_send(widget, WMSG_REDRAW, NULL, 0);

    return widget;
}

void win_widget_free(widget_t* widget)
{
    msg_t freeMsg = {.type = WMSG_FREE};
    win_widget_dispatch(widget, &freeMsg);

    list_remove(widget);
    free(widget);
}

uint64_t win_widget_send(widget_t* widget, msg_type_t type, const void* data, uint64_t size)
{
    widget->messages[widget->writeIndex].type = type;
    memcpy(widget->messages[widget->writeIndex].data, data, size);
    widget->writeIndex = (widget->writeIndex + 1) % WIN_WIDGET_MAX_MSG;

    return 0;
}

uint64_t win_widget_send_all(win_t* window, msg_type_t type, const void* data, uint64_t size)
{
    widget_t* widget;
    LIST_FOR_EACH(widget, &window->widgets)
    {
        win_widget_send(widget, type, data, size);
    }

    return 0;
}

static uint64_t win_widget_dispatch(widget_t* widget, const msg_t* msg)
{
    return widget->procedure(widget, widget->window, msg);
}

void win_widget_rect(widget_t* widget, rect_t* rect)
{
    *rect = widget->rect;
}

widget_id_t win_widget_id(widget_t* widget)
{
    return widget->id;
}

const char* win_widget_name(widget_t* widget)
{
    return widget->name;
}

void* win_widget_private(widget_t* widget)
{
    return widget->private;
}

void win_widget_private_set(widget_t* widget, void* private)
{
    widget->private = private;
}

uint64_t win_screen_rect(rect_t* rect)
{
    fd_t fd = open("sys:/server/dwm");
    if (fd == ERR)
    {
        return ERR;
    }

    ioctl_dwm_size_t size;
    if (ioctl(fd, IOCTL_DWM_SIZE, &size, sizeof(ioctl_dwm_size_t)) == ERR)
    {
        return ERR;
    }

    close(fd);

    *rect = (rect_t){
        .left = 0,
        .top = 0,
        .right = size.outWidth,
        .bottom = size.outHeight,
    };
    return 0;
}

void win_theme(win_theme_t* out)
{
    *out = theme;
}

void win_expand_to_window(rect_t* clientArea, dwm_type_t type)
{
    if (type == DWM_WINDOW)
    {
        clientArea->left -= theme.edgeWidth;
        clientArea->top -= theme.edgeWidth + theme.topbarHeight + theme.topbarPadding;
        clientArea->right += theme.edgeWidth;
        clientArea->bottom += theme.edgeWidth;
    }
}

void win_shrink_to_client(rect_t* windowArea, dwm_type_t type)
{
    if (type == DWM_WINDOW)
    {
        windowArea->left += theme.edgeWidth;
        windowArea->top += theme.edgeWidth + theme.topbarHeight + theme.topbarPadding;
        windowArea->right -= theme.edgeWidth;
        windowArea->bottom -= theme.edgeWidth;
    }
}

#endif
