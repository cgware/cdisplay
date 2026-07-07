#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "driver.h"
#include "fs.h"
#include "sock.h"
#include "window.h"

typedef struct display_driver_s {
	const char *name;
	int (*init)(display_t *display);
	int (*free)(display_t *display);
	int (*poll_events)(display_t *display);
	int (*wait_events)(display_t *display);
	int (*window_init)(window_t *window, u16 x, u16 y, u16 width, u16 height);
	int (*window_free)(window_t *window);
	u32 (*window_id)(window_t *window);
	int (*window_set_title)(window_t *window, strv_t title);
	int (*window_set_position)(window_t *window, u16 x, u16 y);
	int (*window_set_size)(window_t *window, u16 width, u16 height);
	int (*window_set_borderless)(window_t *window, int borderless);
	int (*window_set_fullscreen)(window_t *window, int fullscreen);
	int (*window_show)(window_t *window);
	int (*window_hide)(window_t *window);
	fs_t *fs;
	sock_t *ss;
	alloc_t alloc;
} display_driver_t;

void display_emit_event(display_t *display, const display_event_t *event);

#define DISPLAY_DRIVER_TYPE 1

#define DISPLAY_DRIVER(_name, _data) DRIVER(_name, DISPLAY_DRIVER_TYPE, _data)

#endif
