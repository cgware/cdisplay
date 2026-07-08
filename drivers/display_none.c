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

static int display_none_window_init(window_t *wnd, const window_config_t *config)
{
	(void)config;

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

static int display_none_window_set_title(window_t *wnd, strv_t title)
{
	(void)title;

	if (wnd == NULL) {
		return 1;
	}

	return 0;
}

static int display_none_window_set_position(window_t *wnd, u16 x, u16 y)
{
	(void)x;
	(void)y;

	if (wnd == NULL) {
		return 1;
	}

	return 0;
}

static int display_none_window_set_size(window_t *wnd, u16 width, u16 height)
{
	(void)width;
	(void)height;

	if (wnd == NULL) {
		return 1;
	}

	return 0;
}

static int display_none_window_set_borderless(window_t *wnd, int borderless)
{
	(void)borderless;

	if (wnd == NULL) {
		return 1;
	}

	return 0;
}

static int display_none_window_set_fullscreen(window_t *wnd, int fullscreen)
{
	(void)fullscreen;

	if (wnd == NULL) {
		return 1;
	}

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
	.window_init	       = display_none_window_init,
	.window_free	       = display_none_window_free,
	.window_id	       = display_none_window_id,
	.window_set_title      = display_none_window_set_title,
	.window_set_position   = display_none_window_set_position,
	.window_set_size       = display_none_window_set_size,
	.window_set_borderless = display_none_window_set_borderless,
	.window_set_fullscreen = display_none_window_set_fullscreen,
	.window_show	       = display_none_window_show,
	.window_hide	       = display_none_window_hide,
};

DISPLAY_DRIVER(display_none, &display_none);
