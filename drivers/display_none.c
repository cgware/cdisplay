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

static int display_none_poll_event(display_t *display, display_event_t *event)
{
	if (display == NULL || event == NULL) {
		return 1;
	}

	*event = (display_event_t){0};
	return 1;
}

static int display_none_wait_event(display_t *display, display_event_t *event)
{
	return display_none_poll_event(display, event);
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

static u32 display_none_window_id(window_t *wnd)
{
	if (wnd == NULL) {
		return 0;
	}

	return 0;
}

static display_driver_t display_none = {
	.name	     = "none",
	.init	     = display_none_init,
	.free	     = display_none_free,
	.poll_event  = display_none_poll_event,
	.wait_event  = display_none_wait_event,
	.window_init = display_none_window_init,
	.window_free = display_none_window_free,
	.window_id   = display_none_window_id,
};

DISPLAY_DRIVER(display_none, &display_none);
