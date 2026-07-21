#ifndef MONITOR_H
#define MONITOR_H

#include "arr.h"
#include "dst.h"

typedef struct display_s display_t;

typedef struct display_monitor_s {
	u32 id;
	char name[128];
	s32 x;
	s32 y;
	u32 width;
	u32 height;
	u32 physical_width;
	u32 physical_height;
	u32 refresh_rate;
	u32 scale;
	int primary;
	void *native;
} display_monitor_t;

int display_monitors(display_t *display, arr_t *monitors);
size_t monitor_print(const display_monitor_t *monitor, dst_t dst);

#endif
