#include "window.h"

#include "display_driver.h"

window_t *window_init(window_t *wnd, display_t *display, u16 x, u16 y, u16 width, u16 height)
{
	if (wnd == NULL || display == NULL || display->drv == NULL) {
		return NULL;
	}

	wnd->display = display;

	if (wnd->display->drv->window_init(wnd, x, y, width, height)) {
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
