#include "window.h"

#include "display_driver.h"

window_t *window_init(window_t *wnd, display_t *display, const window_config_t *config)
{
	if (wnd == NULL || display == NULL || display->drv == NULL || config == NULL) {
		return NULL;
	}

	wnd->display = display;

	if (wnd->display->drv->window_init(wnd, config)) {
		wnd->display = NULL;
		wnd->data    = NULL;
		return NULL;
	}

	return wnd;
}

void window_free(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL) {
		return;
	}

	wnd->display->drv->window_free(wnd);
	wnd->display = NULL;
	wnd->data    = NULL;
}

u32 window_id(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_id == NULL) {
		return 0;
	}

	return wnd->display->drv->window_id(wnd);
}

int window_native(window_t *wnd, window_native_t *native)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_native == NULL ||
	    native == NULL) {
		return 1;
	}

	return wnd->display->drv->window_native(wnd, native);
}

int window_set_title(window_t *wnd, strv_t title)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_set_title == NULL) {
		return 1;
	}

	return wnd->display->drv->window_set_title(wnd, title);
}

int window_get_title(window_t *wnd, char *title, size_t size)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_get_title == NULL ||
	    title == NULL || size == 0) {
		return 1;
	}

	return wnd->display->drv->window_get_title(wnd, title, size);
}

int window_set_position(window_t *wnd, u16 x, u16 y)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_set_position == NULL) {
		return 1;
	}

	return wnd->display->drv->window_set_position(wnd, x, y);
}

int window_get_position(window_t *wnd, u16 *x, u16 *y)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_get_position == NULL ||
	    x == NULL || y == NULL) {
		return 1;
	}

	return wnd->display->drv->window_get_position(wnd, x, y);
}

int window_set_size(window_t *wnd, u16 width, u16 height)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_set_size == NULL) {
		return 1;
	}

	return wnd->display->drv->window_set_size(wnd, width, height);
}

int window_get_size(window_t *wnd, u16 *width, u16 *height)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_get_size == NULL ||
	    width == NULL || height == NULL) {
		return 1;
	}

	return wnd->display->drv->window_get_size(wnd, width, height);
}

int window_set_borderless(window_t *wnd, int borderless)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_set_borderless == NULL) {
		return 1;
	}

	return wnd->display->drv->window_set_borderless(wnd, borderless);
}

int window_get_borderless(window_t *wnd, int *borderless)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_get_borderless == NULL ||
	    borderless == NULL) {
		return 1;
	}

	return wnd->display->drv->window_get_borderless(wnd, borderless);
}

int window_set_fullscreen(window_t *wnd, int fullscreen)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_set_fullscreen == NULL) {
		return 1;
	}

	return wnd->display->drv->window_set_fullscreen(wnd, fullscreen);
}

int window_get_fullscreen(window_t *wnd, int *fullscreen)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_get_fullscreen == NULL ||
	    fullscreen == NULL) {
		return 1;
	}

	return wnd->display->drv->window_get_fullscreen(wnd, fullscreen);
}

int window_show(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_show == NULL) {
		return 1;
	}

	return wnd->display->drv->window_show(wnd);
}

int window_hide(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->drv == NULL || wnd->display->drv->window_hide == NULL) {
		return 1;
	}

	return wnd->display->drv->window_hide(wnd);
}
