#include "sys/gfx.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/keyboard.h>
#include <sys/proc.h>
#include <sys/win.h>

#define WINDOW_WIDTH 350
#define WINDOW_HEIGHT 400

#define BUTTON_WIDTH 200
#define BUTTON_HEIGHT 100

#define BUTTON_ID 1

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_BUTTON:
    {
        lmsg_button_t* data = (lmsg_button_t*)msg->data;
        if (!data->pressed)
        {
            if (data->id == BUTTON_ID)
            {
                spawn("calculator.elf");
            }
        }
    }
    break;
    }

    return 0;
}

int main(void)
{
    rect_t rect = RECT_INIT_DIM(500, 200, WINDOW_WIDTH, WINDOW_HEIGHT);
    win_expand_to_window(&rect, DWM_WINDOW);

    win_t* window = win_new("Calculator", DWM_WINDOW, &rect, procedure);
    if (window == NULL)
    {
        return EXIT_FAILURE;
    }

    rect_t buttonRect = RECT_INIT_DIM(WINDOW_WIDTH / 2 - BUTTON_WIDTH / 2, 100, BUTTON_WIDTH, BUTTON_HEIGHT);
    widget_t* button = win_widget_new(window, win_widget_button, "Press Me!", &buttonRect, BUTTON_ID);

    wmsg_set_text setText = {.height = 32, .foreground = 0xFF000000, .background = 0};
    win_widget_send(button, WMSG_SET_TEXT, &setText, sizeof(wmsg_set_text));

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(window, &msg, NEVER);
        win_dispatch(window, &msg);
    }

    win_free(window);
    return EXIT_SUCCESS;
}
