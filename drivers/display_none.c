#include "display_driver.h"

#include "mem.h"

typedef struct window_none_s {
	char title[256];
	u16 x;
	u16 y;
	u16 width;
	u16 height;
	int borderless;
	int fullscreen;
} window_none_t;

static int display_none_init(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	return 0;
}

static int display_none_free(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	return 0;
}

static int display_none_poll_events(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	return 0;
}

static int display_none_wait_events(display_t *display)
{
	return display_none_poll_events(display);
}

static int display_none_monitors(display_t *display, arr_t *monitors)
{
	if (display == NULL || monitors == NULL) {
		return 1;
	}

	monitors->cnt = 0;
	return 0;
}

static int display_none_window_init(window_t *wnd, const window_config_t *config)
{
	if (wnd == NULL) {
		return 1;
	}

	if (wnd->display != NULL && wnd->display->alloc.alloc != NULL) {
		wnd->data = alloc_alloc(&wnd->display->alloc, sizeof(window_none_t));
		if (wnd->data == NULL) {
			return 1;
		}

		window_none_t *none = wnd->data;
		mem_set(none, 0, sizeof(*none));
		if (config != NULL) {
			none->x	     = config->x;
			none->y	     = config->y;
			none->width  = config->width;
			none->height = config->height;
		}
	}

	return 0;
}

static int display_none_window_free(window_t *wnd)
{
	if (wnd == NULL) {
		return 1;
	}

	if (wnd->display != NULL && wnd->display->alloc.free != NULL && wnd->data != NULL) {
		alloc_free(&wnd->display->alloc, wnd->data, sizeof(window_none_t));
		wnd->data = NULL;
	}

	return 0;
}

static u32 display_none_window_id(window_t *wnd)
{
	if (wnd == NULL) {
		return 0;
	}

	return 0;
}

static int display_none_window_set_title(window_t *wnd, strv_t title)
{
	if (wnd == NULL) {
		return 1;
	}

	if (wnd->data != NULL) {
		window_none_t *none = wnd->data;
		if (title.len >= sizeof(none->title) || (title.data == NULL && title.len > 0)) {
			return 1;
		}
		if (title.len > 0) {
			mem_copy(none->title, sizeof(none->title), title.data, title.len);
		}
		none->title[title.len] = 0;
	}

	return 0;
}

static int display_none_window_get_title(window_t *wnd, char *title, size_t size)
{
	if (wnd == NULL || wnd->data == NULL || title == NULL || size == 0) {
		return 1;
	}

	window_none_t *none = wnd->data;
	size_t len	    = 0;
	while (len < sizeof(none->title) && none->title[len] != 0) {
		len++;
	}
	if (len >= size) {
		return 1;
	}

	mem_copy(title, size, none->title, len + 1);
	return 0;
}

static int display_none_window_set_position(window_t *wnd, u16 x, u16 y)
{
	if (wnd == NULL) {
		return 1;
	}

	if (wnd->data != NULL) {
		window_none_t *none = wnd->data;
		none->x		    = x;
		none->y		    = y;
	}

	return 0;
}

static int display_none_window_get_position(window_t *wnd, u16 *x, u16 *y)
{
	if (wnd == NULL || wnd->data == NULL || x == NULL || y == NULL) {
		return 1;
	}

	window_none_t *none = wnd->data;
	*x		    = none->x;
	*y		    = none->y;
	return 0;
}

static int display_none_window_set_size(window_t *wnd, u16 width, u16 height)
{
	if (wnd == NULL) {
		return 1;
	}

	if (wnd->data != NULL) {
		window_none_t *none = wnd->data;
		none->width	    = width;
		none->height	    = height;
	}

	return 0;
}

static int display_none_window_get_size(window_t *wnd, u16 *width, u16 *height)
{
	if (wnd == NULL || wnd->data == NULL || width == NULL || height == NULL) {
		return 1;
	}

	window_none_t *none = wnd->data;
	*width		    = none->width;
	*height		    = none->height;
	return 0;
}

static int display_none_window_set_borderless(window_t *wnd, int borderless)
{
	if (wnd == NULL) {
		return 1;
	}

	if (wnd->data != NULL) {
		window_none_t *none = wnd->data;
		none->borderless    = borderless != 0;
	}

	return 0;
}

static int display_none_window_get_borderless(window_t *wnd, int *borderless)
{
	if (wnd == NULL || wnd->data == NULL || borderless == NULL) {
		return 1;
	}

	window_none_t *none = wnd->data;
	*borderless	    = none->borderless;
	return 0;
}

static int display_none_window_set_fullscreen(window_t *wnd, int fullscreen)
{
	if (wnd == NULL) {
		return 1;
	}

	if (wnd->data != NULL) {
		window_none_t *none = wnd->data;
		none->fullscreen    = fullscreen != 0;
	}

	return 0;
}

static int display_none_window_get_fullscreen(window_t *wnd, int *fullscreen)
{
	if (wnd == NULL || wnd->data == NULL || fullscreen == NULL) {
		return 1;
	}

	window_none_t *none = wnd->data;
	*fullscreen	    = none->fullscreen;
	return 0;
}

static int display_none_window_show(window_t *wnd)
{
	if (wnd == NULL) {
		return 1;
	}

	return 0;
}

static int display_none_window_hide(window_t *wnd)
{
	if (wnd == NULL) {
		return 1;
	}

	return 0;
}

static display_driver_t display_none = {
	.name		       = "none",
	.init		       = display_none_init,
	.free		       = display_none_free,
	.poll_events	       = display_none_poll_events,
	.wait_events	       = display_none_wait_events,
	.monitors	       = display_none_monitors,
	.window_init	       = display_none_window_init,
	.window_free	       = display_none_window_free,
	.window_id	       = display_none_window_id,
	.window_set_title      = display_none_window_set_title,
	.window_get_title      = display_none_window_get_title,
	.window_set_position   = display_none_window_set_position,
	.window_get_position   = display_none_window_get_position,
	.window_set_size       = display_none_window_set_size,
	.window_get_size       = display_none_window_get_size,
	.window_set_borderless = display_none_window_set_borderless,
	.window_get_borderless = display_none_window_get_borderless,
	.window_set_fullscreen = display_none_window_set_fullscreen,
	.window_get_fullscreen = display_none_window_get_fullscreen,
	.window_show	       = display_none_window_show,
	.window_hide	       = display_none_window_hide,
};

DISPLAY_DRIVER(display_none, &display_none);
