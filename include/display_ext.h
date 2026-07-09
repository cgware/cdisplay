#ifndef DISPLAY_EXT_H
#define DISPLAY_EXT_H

#include "display.h"

enum {
	DISPLAY_EXT_REPLY_SIZE = 32,
};

typedef struct display_ext_s {
	display_t *display;
	u8 opcode;
	u8 first_event;
	u8 first_error;
} display_ext_t;

typedef struct display_ext_reply_s {
	u8 header[DISPLAY_EXT_REPLY_SIZE];
	void *data;
	size_t size;
	alloc_t alloc;
} display_ext_reply_t;

display_ext_t *display_ext_init(display_ext_t *ext, display_t *display, strv_t name);
int display_ext_send(display_ext_t *ext, u8 opcode, const void *data, size_t size);
int display_ext_call(display_ext_t *ext, u8 opcode, const void *data, size_t size, display_ext_reply_t *reply);
void display_ext_reply_free(display_ext_reply_t *reply);

int display_alloc_id(display_t *display, u32 *id);
int display_visual_depth(display_t *display, u32 visual, u8 *depth);

#endif
