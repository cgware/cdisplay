#ifndef DISPLAY_H
#define DISPLAY_H

#include "fs.h"
#include "proc.h"
#include "sock.h"

typedef enum display_event_type_e {
	DISPLAY_EVENT_NONE,
	DISPLAY_EVENT_CLOSE,
	DISPLAY_EVENT_RESIZE,
	DISPLAY_EVENT_KEY_DOWN,
	DISPLAY_EVENT_KEY_UP,
	DISPLAY_EVENT_MOUSE_MOVE,
	DISPLAY_EVENT_MOUSE_DOWN,
	DISPLAY_EVENT_MOUSE_UP,
	DISPLAY_EVENT_FOCUS_GAINED,
	DISPLAY_EVENT_FOCUS_LOST,
} display_event_type_t;

typedef struct display_event_s {
	display_event_type_t type;
	u32 window;
	u16 x;
	u16 y;
	u16 width;
	u16 height;
	u32 key;
	u32 button;
	u32 modifiers;
} display_event_t;

typedef struct display_s {
	const struct display_driver_s *drv;
	fs_t *fs;
	proc_t *proc;
	sock_t *ss;
	alloc_t alloc;
	void *data;
} display_t;

display_t *display_init(display_t *display, struct display_driver_s *drv, fs_t *fs, proc_t *proc, sock_t *ss, alloc_t alloc);
void display_free(display_t *display);
int display_poll_event(display_t *display, display_event_t *event);
int display_wait_event(display_t *display, display_event_t *event);

#endif
