#include "display_driver.h"

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

static int display_none_window_init(window_t *wnd, u16 x, u16 y)
{
	(void)x;
	(void)y;

	if (wnd == NULL) {
		return 1;
	}

	return 0;
}

static int display_none_window_free(window_t *wnd)
{
	if (wnd == NULL) {
		return 1;
	}

	return 0;
}

static display_driver_t display_none = {
	.name	     = "none",
	.init	     = display_none_init,
	.free	     = display_none_free,
	.window_init = display_none_window_init,
	.window_free = display_none_window_free,
};

DISPLAY_DRIVER(display_none, &display_none);
