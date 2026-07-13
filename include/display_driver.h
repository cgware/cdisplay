#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "display_ext.h"
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
	int (*native)(display_t *display, display_native_t *native);
	int (*native_free)(display_t *display, void *data);
	int (*window_init)(window_t *window, const window_config_t *config);
	int (*window_free)(window_t *window);
	u32 (*window_id)(window_t *window);
	int (*window_native)(window_t *window, window_native_t *native);
	int (*window_set_title)(window_t *window, strv_t title);
	int (*window_set_position)(window_t *window, u16 x, u16 y);
	int (*window_set_size)(window_t *window, u16 width, u16 height);
	int (*window_set_borderless)(window_t *window, int borderless);
	int (*window_set_fullscreen)(window_t *window, int fullscreen);
	int (*window_show)(window_t *window);
	int (*window_hide)(window_t *window);
	int (*ext_init)(display_ext_t *ext, strv_t name);
	int (*ext_send)(display_ext_t *ext, u8 opcode, const void *data, size_t size);
	int (*ext_call)(display_ext_t *ext, u8 opcode, const void *data, size_t size, display_ext_reply_t *reply);
	int (*alloc_id)(display_t *display, u32 *id);
	int (*visual_depth)(display_t *display, u32 visual, u8 *depth);
	fs_t *fs;
	sock_t *ss;
	alloc_t alloc;
} display_driver_t;

void display_emit_event(display_t *display, const display_event_t *event);

#define DISPLAY_DRIVER_TYPE 0x445350

#define DISPLAY_DRIVER(_name, _data) DRIVER(_name, DISPLAY_DRIVER_TYPE, _data)

#endif
