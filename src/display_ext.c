#include "display_ext.h"

#include "display_driver.h"
#include "log.h"
#include "mem.h"

display_ext_t *display_ext_init(display_ext_t *ext, display_t *display, strv_t name)
{
	if (ext == NULL || display == NULL || display->drv == NULL || display->drv->ext_init == NULL || name.data == NULL ||
	    name.len == 0) {
		log_error("cdisplay", "display_ext", NULL, "invalid display ext arguments");
		return NULL;
	}

	ext->display = display;
	if (display->drv->ext_init(ext, name)) {
		ext->display	 = NULL;
		ext->opcode	 = 0;
		ext->first_event = 0;
		ext->first_error = 0;
		return NULL;
	}

	return ext;
}

int display_ext_send(display_ext_t *ext, u8 opcode, const void *data, size_t size)
{
	if (ext == NULL || ext->display == NULL || ext->display->drv->ext_send == NULL || (size != 0 && data == NULL)) {
		log_error("cdisplay", "display_ext", NULL, "invalid display ext request");
		return 1;
	}

	return ext->display->drv->ext_send(ext, opcode, data, size);
}

int display_ext_call(display_ext_t *ext, u8 opcode, const void *data, size_t size, display_ext_reply_t *reply)
{
	if (ext == NULL || ext->display == NULL || ext->display->drv->ext_call == NULL || reply == NULL || (size != 0 && data == NULL)) {
		log_error("cdisplay", "display_ext", NULL, "invalid display ext call");
		return 1;
	}

	mem_set(reply, 0, sizeof(*reply));
	reply->alloc = ext->display->alloc.alloc == NULL ? ALLOC_STD : ext->display->alloc;
	if (ext->display->drv->ext_call(ext, opcode, data, size, reply)) {
		display_ext_reply_free(reply);
		return 1;
	}

	return 0;
}

void display_ext_reply_free(display_ext_reply_t *reply)
{
	if (reply == NULL) {
		return;
	}

	alloc_t alloc = reply->alloc.alloc == NULL ? ALLOC_STD : reply->alloc;
	alloc_free(&alloc, reply->data, reply->size);
	mem_set(reply, 0, sizeof(*reply));
}

int display_alloc_id(display_t *display, u32 *id)
{
	if (display == NULL || display->drv == NULL || display->drv->alloc_id == NULL || id == NULL) {
		log_error("cdisplay", "display_ext", NULL, "invalid display resource arguments");
		return 1;
	}

	return display->drv->alloc_id(display, id);
}

int display_visual_depth(display_t *display, u32 visual, u8 *depth)
{
	if (display == NULL || display->drv == NULL || display->drv->visual_depth == NULL || visual == 0 || depth == NULL) {
		log_error("cdisplay", "display_ext", NULL, "invalid display visual arguments");
		return 1;
	}

	return display->drv->visual_depth(display, visual, depth);
}
