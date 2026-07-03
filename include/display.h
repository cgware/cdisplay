#ifndef DISPLAY_H
#define DISPLAY_H

#include "fs.h"
#include "proc.h"
#include "sock.h"

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

#endif
