#include "display.h"

#include "display_driver.h"
#include "log.h"

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

const char *display_event_type_name(display_event_type_t type)
{
	switch (type) {
	case DISPLAY_EVENT_NONE:
		return "none";
	case DISPLAY_EVENT_CLOSE:
		return "close";
	case DISPLAY_EVENT_RESIZE:
		return "resize";
	case DISPLAY_EVENT_KEY_DOWN:
		return "key down";
	case DISPLAY_EVENT_KEY_UP:
		return "key up";
	case DISPLAY_EVENT_MOUSE_MOVE:
		return "mouse move";
	case DISPLAY_EVENT_MOUSE_DOWN:
		return "mouse down";
	case DISPLAY_EVENT_MOUSE_UP:
		return "mouse up";
	case DISPLAY_EVENT_FOCUS_GAINED:
		return "focus gained";
	case DISPLAY_EVENT_FOCUS_LOST:
		return "focus lost";
	default:
		return "unknown";
	}
}

const char *display_key_name(display_key_t key)
{
	switch (key) {
	case DISPLAY_KEY_UNKNOWN:
		return "unknown";
	case DISPLAY_KEY_A:
		return "a";
	case DISPLAY_KEY_B:
		return "b";
	case DISPLAY_KEY_C:
		return "c";
	case DISPLAY_KEY_D:
		return "d";
	case DISPLAY_KEY_E:
		return "e";
	case DISPLAY_KEY_F:
		return "f";
	case DISPLAY_KEY_G:
		return "g";
	case DISPLAY_KEY_H:
		return "h";
	case DISPLAY_KEY_I:
		return "i";
	case DISPLAY_KEY_J:
		return "j";
	case DISPLAY_KEY_K:
		return "k";
	case DISPLAY_KEY_L:
		return "l";
	case DISPLAY_KEY_M:
		return "m";
	case DISPLAY_KEY_N:
		return "n";
	case DISPLAY_KEY_O:
		return "o";
	case DISPLAY_KEY_P:
		return "p";
	case DISPLAY_KEY_Q:
		return "q";
	case DISPLAY_KEY_R:
		return "r";
	case DISPLAY_KEY_S:
		return "s";
	case DISPLAY_KEY_T:
		return "t";
	case DISPLAY_KEY_U:
		return "u";
	case DISPLAY_KEY_V:
		return "v";
	case DISPLAY_KEY_W:
		return "w";
	case DISPLAY_KEY_X:
		return "x";
	case DISPLAY_KEY_Y:
		return "y";
	case DISPLAY_KEY_Z:
		return "z";
	case DISPLAY_KEY_0:
		return "0";
	case DISPLAY_KEY_1:
		return "1";
	case DISPLAY_KEY_2:
		return "2";
	case DISPLAY_KEY_3:
		return "3";
	case DISPLAY_KEY_4:
		return "4";
	case DISPLAY_KEY_5:
		return "5";
	case DISPLAY_KEY_6:
		return "6";
	case DISPLAY_KEY_7:
		return "7";
	case DISPLAY_KEY_8:
		return "8";
	case DISPLAY_KEY_9:
		return "9";
	case DISPLAY_KEY_ESCAPE:
		return "escape";
	case DISPLAY_KEY_ENTER:
		return "enter";
	case DISPLAY_KEY_TAB:
		return "tab";
	case DISPLAY_KEY_BACKSPACE:
		return "backspace";
	case DISPLAY_KEY_SPACE:
		return "space";
	case DISPLAY_KEY_LEFT:
		return "left";
	case DISPLAY_KEY_RIGHT:
		return "right";
	case DISPLAY_KEY_UP:
		return "up";
	case DISPLAY_KEY_DOWN:
		return "down";
	case DISPLAY_KEY_LEFT_SHIFT:
		return "left shift";
	case DISPLAY_KEY_RIGHT_SHIFT:
		return "right shift";
	case DISPLAY_KEY_LEFT_CONTROL:
		return "left control";
	case DISPLAY_KEY_RIGHT_CONTROL:
		return "right control";
	case DISPLAY_KEY_LEFT_ALT:
		return "left alt";
	case DISPLAY_KEY_RIGHT_ALT:
		return "right alt";
	case DISPLAY_KEY_LEFT_SUPER:
		return "left super";
	case DISPLAY_KEY_RIGHT_SUPER:
		return "right super";
	case DISPLAY_KEY_F1:
		return "f1";
	case DISPLAY_KEY_F2:
		return "f2";
	case DISPLAY_KEY_F3:
		return "f3";
	case DISPLAY_KEY_F4:
		return "f4";
	case DISPLAY_KEY_F5:
		return "f5";
	case DISPLAY_KEY_F6:
		return "f6";
	case DISPLAY_KEY_F7:
		return "f7";
	case DISPLAY_KEY_F8:
		return "f8";
	case DISPLAY_KEY_F9:
		return "f9";
	case DISPLAY_KEY_F10:
		return "f10";
	case DISPLAY_KEY_F11:
		return "f11";
	case DISPLAY_KEY_F12:
		return "f12";
	default:
		return "unknown";
	}
}

const char *display_mouse_name(display_mouse_t button)
{
	switch (button) {
	case DISPLAY_MOUSE_UNKNOWN:
		return "unknown";
	case DISPLAY_MOUSE_LEFT:
		return "left";
	case DISPLAY_MOUSE_MIDDLE:
		return "middle";
	case DISPLAY_MOUSE_RIGHT:
		return "right";
	case DISPLAY_MOUSE_WHEEL_UP:
		return "wheel up";
	case DISPLAY_MOUSE_WHEEL_DOWN:
		return "wheel down";
	case DISPLAY_MOUSE_WHEEL_LEFT:
		return "wheel left";
	case DISPLAY_MOUSE_WHEEL_RIGHT:
		return "wheel right";
	case DISPLAY_MOUSE_BACK:
		return "back";
	case DISPLAY_MOUSE_FORWARD:
		return "forward";
	default:
		return "unknown";
	}
}

void display_event_log(const display_event_t *event)
{
	if (event == NULL) {
		return;
	}

	switch (event->type) {
	case DISPLAY_EVENT_RESIZE:
		log_info("cdisplay",
			 "display",
			 NULL,
			 "event=%s window=%u pos=%u,%u size=%ux%u",
			 display_event_type_name(event->type),
			 event->window,
			 event->x,
			 event->y,
			 event->width,
			 event->height);
		break;
	case DISPLAY_EVENT_KEY_DOWN:
	case DISPLAY_EVENT_KEY_UP:
		log_info("cdisplay",
			 "display",
			 NULL,
			 "event=%s window=%u key=%s pos=%u,%u mods=%u",
			 display_event_type_name(event->type),
			 event->window,
			 display_key_name(event->key),
			 event->x,
			 event->y,
			 event->modifiers);
		break;
	case DISPLAY_EVENT_MOUSE_MOVE:
		log_info("cdisplay",
			 "display",
			 NULL,
			 "event=%s window=%u pos=%u,%u mods=%u",
			 display_event_type_name(event->type),
			 event->window,
			 event->x,
			 event->y,
			 event->modifiers);
		break;
	case DISPLAY_EVENT_MOUSE_DOWN:
	case DISPLAY_EVENT_MOUSE_UP:
		log_info("cdisplay",
			 "display",
			 NULL,
			 "event=%s window=%u button=%s pos=%u,%u mods=%u",
			 display_event_type_name(event->type),
			 event->window,
			 display_mouse_name(event->button),
			 event->x,
			 event->y,
			 event->modifiers);
		break;
	default:
		log_info("cdisplay", "display", NULL, "event=%s window=%u", display_event_type_name(event->type), event->window);
		break;
	}
}
