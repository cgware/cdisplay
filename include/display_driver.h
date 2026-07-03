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
	int (*window_init)(window_t *window, u16 x, u16 y);
	int (*window_free)(window_t *window);
	fs_t *fs;
	sock_t *ss;
	alloc_t alloc;
} display_driver_t;

#define DISPLAY_DRIVER_TYPE 1

#define DISPLAY_DRIVER(_name, _data) DRIVER(_name, DISPLAY_DRIVER_TYPE, _data)

#endif
