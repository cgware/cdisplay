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

static const char *s_key_str[] = {
	[DISPLAY_KEY_UNKNOWN]	    = "unknown",
	[DISPLAY_KEY_A]		    = "a",
	[DISPLAY_KEY_B]		    = "b",
	[DISPLAY_KEY_C]		    = "c",
	[DISPLAY_KEY_D]		    = "d",
	[DISPLAY_KEY_E]		    = "e",
	[DISPLAY_KEY_F]		    = "f",
	[DISPLAY_KEY_G]		    = "g",
	[DISPLAY_KEY_H]		    = "h",
	[DISPLAY_KEY_I]		    = "i",
	[DISPLAY_KEY_J]		    = "j",
	[DISPLAY_KEY_K]		    = "k",
	[DISPLAY_KEY_L]		    = "l",
	[DISPLAY_KEY_M]		    = "m",
	[DISPLAY_KEY_N]		    = "n",
	[DISPLAY_KEY_O]		    = "o",
	[DISPLAY_KEY_P]		    = "p",
	[DISPLAY_KEY_Q]		    = "q",
	[DISPLAY_KEY_R]		    = "r",
	[DISPLAY_KEY_S]		    = "s",
	[DISPLAY_KEY_T]		    = "t",
	[DISPLAY_KEY_U]		    = "u",
	[DISPLAY_KEY_V]		    = "v",
	[DISPLAY_KEY_W]		    = "w",
	[DISPLAY_KEY_X]		    = "x",
	[DISPLAY_KEY_Y]		    = "y",
	[DISPLAY_KEY_Z]		    = "z",
	[DISPLAY_KEY_0]		    = "0",
	[DISPLAY_KEY_1]		    = "1",
	[DISPLAY_KEY_2]		    = "2",
	[DISPLAY_KEY_3]		    = "3",
	[DISPLAY_KEY_4]		    = "4",
	[DISPLAY_KEY_5]		    = "5",
	[DISPLAY_KEY_6]		    = "6",
	[DISPLAY_KEY_7]		    = "7",
	[DISPLAY_KEY_8]		    = "8",
	[DISPLAY_KEY_9]		    = "9",
	[DISPLAY_KEY_GRAVE]	    = "grave",
	[DISPLAY_KEY_MINUS]	    = "minus",
	[DISPLAY_KEY_EQUAL]	    = "equal",
	[DISPLAY_KEY_LEFT_BRACKET]  = "left bracket",
	[DISPLAY_KEY_RIGHT_BRACKET] = "right bracket",
	[DISPLAY_KEY_BACKSLASH]	    = "backslash",
	[DISPLAY_KEY_SEMICOLON]	    = "semicolon",
	[DISPLAY_KEY_APOSTROPHE]    = "apostrophe",
	[DISPLAY_KEY_COMMA]	    = "comma",
	[DISPLAY_KEY_PERIOD]	    = "period",
	[DISPLAY_KEY_SLASH]	    = "slash",
	[DISPLAY_KEY_SPACE]	    = "space",
	[DISPLAY_KEY_ENTER]	    = "enter",
	[DISPLAY_KEY_TAB]	    = "tab",
	[DISPLAY_KEY_BACKSPACE]	    = "backspace",
	[DISPLAY_KEY_ESCAPE]	    = "escape",
	[DISPLAY_KEY_CAPS_LOCK]	    = "caps lock",
	[DISPLAY_KEY_NUM_LOCK]	    = "num lock",
	[DISPLAY_KEY_SCROLL_LOCK]   = "scroll lock",
	[DISPLAY_KEY_PAUSE]	    = "pause",
	[DISPLAY_KEY_PRINT_SCREEN]  = "print screen",
	[DISPLAY_KEY_INSERT]	    = "insert",
	[DISPLAY_KEY_DELETE]	    = "delete",
	[DISPLAY_KEY_HOME]	    = "home",
	[DISPLAY_KEY_END]	    = "end",
	[DISPLAY_KEY_PAGE_UP]	    = "page up",
	[DISPLAY_KEY_PAGE_DOWN]	    = "page down",
	[DISPLAY_KEY_UP]	    = "up",
	[DISPLAY_KEY_DOWN]	    = "down",
	[DISPLAY_KEY_LEFT]	    = "left",
	[DISPLAY_KEY_RIGHT]	    = "right",
	[DISPLAY_KEY_LEFT_SHIFT]    = "left shift",
	[DISPLAY_KEY_RIGHT_SHIFT]   = "right shift",
	[DISPLAY_KEY_LEFT_CONTROL]  = "left control",
	[DISPLAY_KEY_RIGHT_CONTROL] = "right control",
	[DISPLAY_KEY_LEFT_ALT]	    = "left alt",
	[DISPLAY_KEY_RIGHT_ALT]	    = "right alt",
	[DISPLAY_KEY_LEFT_SUPER]    = "left super",
	[DISPLAY_KEY_RIGHT_SUPER]   = "right super",
	[DISPLAY_KEY_MENU]	    = "menu",
	[DISPLAY_KEY_F1]	    = "f1",
	[DISPLAY_KEY_F2]	    = "f2",
	[DISPLAY_KEY_F3]	    = "f3",
	[DISPLAY_KEY_F4]	    = "f4",
	[DISPLAY_KEY_F5]	    = "f5",
	[DISPLAY_KEY_F6]	    = "f6",
	[DISPLAY_KEY_F7]	    = "f7",
	[DISPLAY_KEY_F8]	    = "f8",
	[DISPLAY_KEY_F9]	    = "f9",
	[DISPLAY_KEY_F10]	    = "f10",
	[DISPLAY_KEY_F11]	    = "f11",
	[DISPLAY_KEY_F12]	    = "f12",
	[DISPLAY_KEY_KP_0]	    = "kp 0",
	[DISPLAY_KEY_KP_1]	    = "kp 1",
	[DISPLAY_KEY_KP_2]	    = "kp 2",
	[DISPLAY_KEY_KP_3]	    = "kp 3",
	[DISPLAY_KEY_KP_4]	    = "kp 4",
	[DISPLAY_KEY_KP_5]	    = "kp 5",
	[DISPLAY_KEY_KP_6]	    = "kp 6",
	[DISPLAY_KEY_KP_7]	    = "kp 7",
	[DISPLAY_KEY_KP_8]	    = "kp 8",
	[DISPLAY_KEY_KP_9]	    = "kp 9",
	[DISPLAY_KEY_KP_DECIMAL]    = "kp decimal",
	[DISPLAY_KEY_KP_DIVIDE]	    = "kp divide",
	[DISPLAY_KEY_KP_MULTIPLY]   = "kp multiply",
	[DISPLAY_KEY_KP_SUBTRACT]   = "kp subtract",
	[DISPLAY_KEY_KP_ADD]	    = "kp add",
	[DISPLAY_KEY_KP_ENTER]	    = "kp enter",
};

const char *display_key_name(display_key_t key)
{
	if (key < DISPLAY_KEY_UNKNOWN || key >= __DISPLAY_KEY_MAX) {
		key = DISPLAY_KEY_UNKNOWN;
	}

	return s_key_str[key];
}

static const char *s_mouse_str[] = {
	[DISPLAY_MOUSE_UNKNOWN]	    = "unknown",
	[DISPLAY_MOUSE_LEFT]	    = "left",
	[DISPLAY_MOUSE_MIDDLE]	    = "middle",
	[DISPLAY_MOUSE_RIGHT]	    = "right",
	[DISPLAY_MOUSE_WHEEL_UP]    = "wheel up",
	[DISPLAY_MOUSE_WHEEL_DOWN]  = "wheel down",
	[DISPLAY_MOUSE_WHEEL_LEFT]  = "wheel left",
	[DISPLAY_MOUSE_WHEEL_RIGHT] = "wheel right",
	[DISPLAY_MOUSE_BACK]	    = "back",
	[DISPLAY_MOUSE_FORWARD]	    = "forward",
};

const char *display_mouse_name(display_mouse_t button)
{
	if (button < DISPLAY_MOUSE_UNKNOWN || button >= __DISPLAY_MOUSE_MAX) {
		button = DISPLAY_MOUSE_UNKNOWN;
	}

	return s_mouse_str[button];
}

static const char *s_modifier_str[] = {
	[DISPLAY_MOD_NONE]	       = "none",
	[DISPLAY_MOD_SHIFT]	       = "shift",
	[DISPLAY_MOD_CAPS_LOCK]	       = "caps lock",
	[DISPLAY_MOD_CONTROL]	       = "control",
	[DISPLAY_MOD_ALT]	       = "alt",
	[DISPLAY_MOD_NUM_LOCK]	       = "num lock",
	[DISPLAY_MOD_SUPER]	       = "super",
	[DISPLAY_MOD_MOUSE_LEFT]       = "mouse left",
	[DISPLAY_MOD_MOUSE_MIDDLE]     = "mouse middle",
	[DISPLAY_MOD_MOUSE_RIGHT]      = "mouse right",
	[DISPLAY_MOD_MOUSE_WHEEL_UP]   = "mouse wheel up",
	[DISPLAY_MOD_MOUSE_WHEEL_DOWN] = "mouse wheel down",
	[DISPLAY_MOD_UNKNOWN]	       = "unknown",
};

const char *display_modifier_name(display_modifier_t modifier)
{
	if (modifier < DISPLAY_MOD_NONE || modifier >= __DISPLAY_MOD_MAX) {
		modifier = DISPLAY_MOD_UNKNOWN;
	}

	return s_modifier_str[modifier];
}

static size_t display_str_len(const char *str)
{
	size_t len = 0;

	while (str[len] != 0) {
		len++;
	}

	return len;
}

static void modifier_format_add(display_modifier_t *written, char *buf, size_t size, const char *name)
{
	size_t off = display_str_len(buf);

	if (*written != DISPLAY_MOD_NONE && off + 1 < size) {
		buf[off++] = '|';
		buf[off]   = 0;
	}

	if (off < size) {
		c_sprintf(buf, size, off, "%s", name);
	}
	*written = (display_modifier_t)(*written | 1);
}

void display_modifiers_format(display_modifier_t modifiers, char *buf, size_t size)
{
	display_modifier_t written = DISPLAY_MOD_NONE;
	display_modifier_t known =
		(display_modifier_t)(DISPLAY_MOD_SHIFT | DISPLAY_MOD_CAPS_LOCK | DISPLAY_MOD_CONTROL | DISPLAY_MOD_ALT |
				     DISPLAY_MOD_NUM_LOCK | DISPLAY_MOD_SUPER | DISPLAY_MOD_MOUSE_LEFT | DISPLAY_MOD_MOUSE_MIDDLE |
				     DISPLAY_MOD_MOUSE_RIGHT | DISPLAY_MOD_MOUSE_WHEEL_UP | DISPLAY_MOD_MOUSE_WHEEL_DOWN);

	if (buf == NULL || size == 0) {
		return;
	}

	buf[0] = 0;

	if (modifiers == DISPLAY_MOD_NONE) {
		c_sprintf(buf, size, 0, "%s", display_modifier_name(DISPLAY_MOD_NONE));
		return;
	}

	if (modifiers & DISPLAY_MOD_SHIFT) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_SHIFT));
	}
	if (modifiers & DISPLAY_MOD_CAPS_LOCK) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_CAPS_LOCK));
	}
	if (modifiers & DISPLAY_MOD_CONTROL) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_CONTROL));
	}
	if (modifiers & DISPLAY_MOD_ALT) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_ALT));
	}
	if (modifiers & DISPLAY_MOD_NUM_LOCK) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_NUM_LOCK));
	}
	if (modifiers & DISPLAY_MOD_SUPER) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_SUPER));
	}
	if (modifiers & DISPLAY_MOD_MOUSE_LEFT) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_MOUSE_LEFT));
	}
	if (modifiers & DISPLAY_MOD_MOUSE_MIDDLE) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_MOUSE_MIDDLE));
	}
	if (modifiers & DISPLAY_MOD_MOUSE_RIGHT) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_MOUSE_RIGHT));
	}
	if (modifiers & DISPLAY_MOD_MOUSE_WHEEL_UP) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_MOUSE_WHEEL_UP));
	}
	if (modifiers & DISPLAY_MOD_MOUSE_WHEEL_DOWN) {
		modifier_format_add(&written, buf, size, display_modifier_name(DISPLAY_MOD_MOUSE_WHEEL_DOWN));
	}
	if (modifiers & ~known) {
		modifier_format_add(&written, buf, size, display_modifier_name((display_modifier_t)~known));
	}
}

void display_event_log(const display_event_t *event)
{
	if (event == NULL) {
		return;
	}

	char modifiers[256] = {0};
	display_modifiers_format(event->modifiers, modifiers, sizeof(modifiers));

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
			 "event=%s window=%u key=%s pos=%u,%u mods=%s",
			 display_event_type_name(event->type),
			 event->window,
			 display_key_name(event->key),
			 event->x,
			 event->y,
			 modifiers);
		break;
	case DISPLAY_EVENT_MOUSE_MOVE:
		log_info("cdisplay",
			 "display",
			 NULL,
			 "event=%s window=%u pos=%u,%u mods=%s",
			 display_event_type_name(event->type),
			 event->window,
			 event->x,
			 event->y,
			 modifiers);
		break;
	case DISPLAY_EVENT_MOUSE_DOWN:
	case DISPLAY_EVENT_MOUSE_UP:
		log_info("cdisplay",
			 "display",
			 NULL,
			 "event=%s window=%u button=%s pos=%u,%u mods=%s",
			 display_event_type_name(event->type),
			 event->window,
			 display_mouse_name(event->button),
			 event->x,
			 event->y,
			 modifiers);
		break;
	default:
		log_info("cdisplay", "display", NULL, "event=%s window=%u", display_event_type_name(event->type), event->window);
		break;
	}
}
