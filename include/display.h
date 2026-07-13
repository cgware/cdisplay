#ifndef DISPLAY_H
#define DISPLAY_H

#include "fs.h"
#include "proc.h"
#include "sock.h"

typedef enum display_event_type_e {
	DISPLAY_EVENT_NONE,
	DISPLAY_EVENT_CLOSE,
	DISPLAY_EVENT_RESIZE,
	DISPLAY_EVENT_KEY_DOWN,
	DISPLAY_EVENT_KEY_UP,
	DISPLAY_EVENT_MOUSE_MOVE,
	DISPLAY_EVENT_MOUSE_DOWN,
	DISPLAY_EVENT_MOUSE_UP,
	DISPLAY_EVENT_FOCUS_GAINED,
	DISPLAY_EVENT_FOCUS_LOST,
} display_event_type_t;

typedef enum display_key_e {
	DISPLAY_KEY_UNKNOWN,
	DISPLAY_KEY_A,
	DISPLAY_KEY_B,
	DISPLAY_KEY_C,
	DISPLAY_KEY_D,
	DISPLAY_KEY_E,
	DISPLAY_KEY_F,
	DISPLAY_KEY_G,
	DISPLAY_KEY_H,
	DISPLAY_KEY_I,
	DISPLAY_KEY_J,
	DISPLAY_KEY_K,
	DISPLAY_KEY_L,
	DISPLAY_KEY_M,
	DISPLAY_KEY_N,
	DISPLAY_KEY_O,
	DISPLAY_KEY_P,
	DISPLAY_KEY_Q,
	DISPLAY_KEY_R,
	DISPLAY_KEY_S,
	DISPLAY_KEY_T,
	DISPLAY_KEY_U,
	DISPLAY_KEY_V,
	DISPLAY_KEY_W,
	DISPLAY_KEY_X,
	DISPLAY_KEY_Y,
	DISPLAY_KEY_Z,
	DISPLAY_KEY_0,
	DISPLAY_KEY_1,
	DISPLAY_KEY_2,
	DISPLAY_KEY_3,
	DISPLAY_KEY_4,
	DISPLAY_KEY_5,
	DISPLAY_KEY_6,
	DISPLAY_KEY_7,
	DISPLAY_KEY_8,
	DISPLAY_KEY_9,
	DISPLAY_KEY_GRAVE,
	DISPLAY_KEY_MINUS,
	DISPLAY_KEY_EQUAL,
	DISPLAY_KEY_LEFT_BRACKET,
	DISPLAY_KEY_RIGHT_BRACKET,
	DISPLAY_KEY_BACKSLASH,
	DISPLAY_KEY_SEMICOLON,
	DISPLAY_KEY_APOSTROPHE,
	DISPLAY_KEY_COMMA,
	DISPLAY_KEY_PERIOD,
	DISPLAY_KEY_SLASH,
	DISPLAY_KEY_SPACE,
	DISPLAY_KEY_ENTER,
	DISPLAY_KEY_TAB,
	DISPLAY_KEY_BACKSPACE,
	DISPLAY_KEY_ESCAPE,
	DISPLAY_KEY_CAPS_LOCK,
	DISPLAY_KEY_NUM_LOCK,
	DISPLAY_KEY_SCROLL_LOCK,
	DISPLAY_KEY_PAUSE,
	DISPLAY_KEY_PRINT_SCREEN,
	DISPLAY_KEY_INSERT,
	DISPLAY_KEY_DELETE,
	DISPLAY_KEY_HOME,
	DISPLAY_KEY_END,
	DISPLAY_KEY_PAGE_UP,
	DISPLAY_KEY_PAGE_DOWN,
	DISPLAY_KEY_UP,
	DISPLAY_KEY_DOWN,
	DISPLAY_KEY_LEFT,
	DISPLAY_KEY_RIGHT,
	DISPLAY_KEY_LEFT_SHIFT,
	DISPLAY_KEY_RIGHT_SHIFT,
	DISPLAY_KEY_LEFT_CONTROL,
	DISPLAY_KEY_RIGHT_CONTROL,
	DISPLAY_KEY_LEFT_ALT,
	DISPLAY_KEY_RIGHT_ALT,
	DISPLAY_KEY_LEFT_SUPER,
	DISPLAY_KEY_RIGHT_SUPER,
	DISPLAY_KEY_MENU,
	DISPLAY_KEY_F1,
	DISPLAY_KEY_F2,
	DISPLAY_KEY_F3,
	DISPLAY_KEY_F4,
	DISPLAY_KEY_F5,
	DISPLAY_KEY_F6,
	DISPLAY_KEY_F7,
	DISPLAY_KEY_F8,
	DISPLAY_KEY_F9,
	DISPLAY_KEY_F10,
	DISPLAY_KEY_F11,
	DISPLAY_KEY_F12,
	DISPLAY_KEY_KP_0,
	DISPLAY_KEY_KP_1,
	DISPLAY_KEY_KP_2,
	DISPLAY_KEY_KP_3,
	DISPLAY_KEY_KP_4,
	DISPLAY_KEY_KP_5,
	DISPLAY_KEY_KP_6,
	DISPLAY_KEY_KP_7,
	DISPLAY_KEY_KP_8,
	DISPLAY_KEY_KP_9,
	DISPLAY_KEY_KP_DECIMAL,
	DISPLAY_KEY_KP_DIVIDE,
	DISPLAY_KEY_KP_MULTIPLY,
	DISPLAY_KEY_KP_SUBTRACT,
	DISPLAY_KEY_KP_ADD,
	DISPLAY_KEY_KP_ENTER,
	__DISPLAY_KEY_MAX,
} display_key_t;

typedef enum display_mouse_e {
	DISPLAY_MOUSE_UNKNOWN,
	DISPLAY_MOUSE_LEFT,
	DISPLAY_MOUSE_MIDDLE,
	DISPLAY_MOUSE_RIGHT,
	DISPLAY_MOUSE_WHEEL_UP,
	DISPLAY_MOUSE_WHEEL_DOWN,
	DISPLAY_MOUSE_WHEEL_LEFT,
	DISPLAY_MOUSE_WHEEL_RIGHT,
	DISPLAY_MOUSE_BACK,
	DISPLAY_MOUSE_FORWARD,
	__DISPLAY_MOUSE_MAX,
} display_mouse_t;

typedef enum display_modifier_e {
	DISPLAY_MOD_NONE	     = 0,
	DISPLAY_MOD_SHIFT	     = 1u << 0,
	DISPLAY_MOD_CAPS_LOCK	     = 1u << 1,
	DISPLAY_MOD_CONTROL	     = 1u << 2,
	DISPLAY_MOD_ALT		     = 1u << 3,
	DISPLAY_MOD_NUM_LOCK	     = 1u << 4,
	DISPLAY_MOD_SUPER	     = 1u << 5,
	DISPLAY_MOD_MOUSE_LEFT	     = 1u << 6,
	DISPLAY_MOD_MOUSE_MIDDLE     = 1u << 7,
	DISPLAY_MOD_MOUSE_RIGHT	     = 1u << 8,
	DISPLAY_MOD_MOUSE_WHEEL_UP   = 1u << 9,
	DISPLAY_MOD_MOUSE_WHEEL_DOWN = 1u << 10,
	DISPLAY_MOD_UNKNOWN,
	__DISPLAY_MOD_MAX,
} display_modifier_t;

typedef struct display_event_s {
	display_event_type_t type;
	u32 window;
	u16 x;
	u16 y;
	u16 width;
	u16 height;
	display_key_t key;
	display_mouse_t button;
	display_modifier_t modifiers;
} display_event_t;

typedef struct display_s display_t;
typedef void (*display_event_cb_t)(display_t *display, const display_event_t *event, void *user);

typedef enum display_native_type_e {
	DISPLAY_NATIVE_NONE,
	DISPLAY_NATIVE_X11,
	DISPLAY_NATIVE_WINDOWS,
} display_native_type_t;

typedef struct display_native_s {
	display_native_type_t type;
	void *display;
	int screen;
} display_native_t;

struct display_s {
	const struct display_driver_s *drv;
	fs_t *fs;
	proc_t *proc;
	sock_t *ss;
	alloc_t alloc;
	display_event_cb_t event_cb;
	void *event_user;
	void *data;
};

display_t *display_init(display_t *display, struct display_driver_s *drv, fs_t *fs, proc_t *proc, sock_t *ss, alloc_t alloc);
void display_free(display_t *display);
int display_set_event_callback(display_t *display, display_event_cb_t cb, void *user);
int display_poll_events(display_t *display);
int display_wait_events(display_t *display);
int display_native(display_t *display, display_native_t *native);
int display_native_free(display_t *display, void *data);
const char *display_event_type_name(display_event_type_t type);
const char *display_key_name(display_key_t key);
const char *display_mouse_name(display_mouse_t button);
const char *display_modifier_name(display_modifier_t modifier);
void display_modifiers_format(display_modifier_t modifiers, char *buf, size_t size);
void display_event_log(const display_event_t *event);

#endif
