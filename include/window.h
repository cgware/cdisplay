#ifndef WINDOW_H
#define WINDOW_H

#include "display.h"

typedef struct window_s {
	display_t *display;
	void *data;
} window_t;

window_t *window_init(window_t *wnd, display_t *display, u16 x, u16 y, u16 width, u16 height);
void window_free(window_t *wnd);

u32 window_id(window_t *wnd);
int window_set_title(window_t *wnd, strv_t title);
int window_set_position(window_t *wnd, u16 x, u16 y);
int window_set_size(window_t *wnd, u16 width, u16 height);
int window_show(window_t *wnd);
int window_hide(window_t *wnd);

#endif
