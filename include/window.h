#ifndef WINDOW_H
#define WINDOW_H

#include "display.h"

typedef struct window_s {
	display_t *display;
	void *data;
} window_t;

window_t *window_init(window_t *wnd, display_t *display, u16 x, u16 y);
void window_free(window_t *wnd);

#endif
