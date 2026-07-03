#include "display.h"

#include "display_driver.h"

display_t *display_init(display_t *display, struct display_driver_s *drv, fs_t *fs, proc_t *proc, sock_t *ss, alloc_t alloc)
{
	if (display == NULL || fs == NULL || proc == NULL || ss == NULL || drv == NULL) {
		return NULL;
	}

	display->drv   = drv;
	display->fs    = fs;
	display->proc  = proc;
	display->ss    = ss;
	display->alloc = alloc;

	if (display->drv->init(display)) {
		display->drv   = NULL;
		display->fs    = NULL;
		display->proc  = NULL;
		display->ss    = NULL;
		display->alloc = (alloc_t){0};
		display->data  = NULL;
		return NULL;
	}

	return display;
}

void display_free(display_t *display)
{
	if (display == NULL || display->drv == NULL) {
		return;
	}

	display->drv->free(display);
	display->drv   = NULL;
	display->fs    = NULL;
	display->proc  = NULL;
	display->ss    = NULL;
	display->alloc = (alloc_t){0};
	display->data  = NULL;
}

int display_poll_event(display_t *display, display_event_t *event)
{
	if (display == NULL || display->drv == NULL || display->drv->poll_event == NULL || event == NULL) {
		return 1;
	}

	return display->drv->poll_event(display, event);
}

int display_wait_event(display_t *display, display_event_t *event)
{
	if (display == NULL || display->drv == NULL || display->drv->wait_event == NULL || event == NULL) {
		return 1;
	}

	return display->drv->wait_event(display, event);
}
