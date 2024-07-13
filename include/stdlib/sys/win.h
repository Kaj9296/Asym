#ifndef _SYS_WIN_H
#define _SYS_WIN_H 1

#include <stdbool.h>
#include <stdint.h>
#include <sys/dwm.h>
#include <sys/gfx.h>
#include <sys/io.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/NULL.h"
#include "_AUX/config.h"
#include "_AUX/nsec_t.h"
#include "_AUX/pixel_t.h"
#include "_AUX/point_t.h"
#include "_AUX/rect_t.h"

#ifndef _WIN_INTERNAL
typedef uint8_t win_t;
typedef uint8_t widget_t;
#endif

typedef uint16_t widget_id_t;

typedef struct win_theme
{
    uint32_t edgeWidth;
    uint32_t ridgeWidth;
    pixel_t highlight;
    pixel_t shadow;
    pixel_t background;
    pixel_t selected;
    pixel_t unSelected;
    uint64_t topbarHeight;
} win_theme_t;

// Library messages
typedef struct
{
    const char* name;
    dwm_type_t type;
    uint8_t rectIsClient;
    rect_t rect;
    void* private;
} lmsg_init_t;

typedef struct
{
    uint8_t pressed;
    widget_id_t id;
} lmsg_button_t;

#define LMSG_BASE (1 << 14)
#define LMSG_INIT (LMSG_BASE + 0)
#define LMSG_FREE (LMSG_BASE + 1)
#define LMSG_QUIT (LMSG_BASE + 2)
#define LMSG_REDRAW (LMSG_BASE + 3)
#define LMSG_BUTTON (LMSG_BASE + 4)

// Widget messages
typedef struct
{
    void* private;
} wmsg_init_t;

#define WMSG_BASE (1 << 15)
#define WMSG_INIT (WMSG_BASE + 0)
#define WMSG_FREE (WMSG_BASE + 1)
#define WMSG_REDRAW (WMSG_BASE + 2)

// User messages
#define UMSG_BASE ((1 << 15) | (1 << 14))

typedef uint64_t (*win_proc_t)(win_t*, void*, surface_t*, msg_t*);
typedef uint64_t (*widget_proc_t)(widget_t*, void*, win_t*, surface_t*, msg_t*);

win_t* win_new(win_proc_t procedure);

uint64_t win_free(win_t* window);

uint64_t win_send(win_t* window, msg_type_t type, const void* data, uint64_t size);

uint64_t win_receive(win_t* window, msg_t* msg, nsec_t timeout);

uint64_t win_dispatch(win_t* window, msg_t* msg);

uint64_t win_move(win_t* window, const rect_t* rect);

void win_screen_window_area(win_t* window, rect_t* rect);

void win_screen_client_area(win_t* window, rect_t* rect);

void win_client_area(win_t* window, rect_t* rect);

void win_screen_to_window(win_t* window, point_t* point);

void win_screen_to_client(win_t* window, point_t* point);

void win_window_to_client(win_t* window, point_t* point);

widget_t* win_widget_new(win_t* window, widget_proc_t procedure, const char* name, const rect_t* rect, widget_id_t id);

void win_widget_free(widget_t* widget);

uint64_t win_widget_send(widget_t* widget, msg_type_t type, const void* data, uint64_t size);

void win_widget_rect(widget_t* widget, rect_t* rect);

uint64_t win_screen_rect(rect_t* rect);

void win_theme(win_theme_t* winTheme);

uint64_t win_widget_button(widget_t* widget, void* private, win_t* window, surface_t* surface, msg_t* msg);

#if defined(__cplusplus)
}
#endif

#endif
