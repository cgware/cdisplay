#include "display_x11_priv.h"

#include "log.h"
#include "mem.h"

enum {
	XK_APOSTROPHE	 = 0x0027,
	XK_COMMA	 = 0x002c,
	XK_MINUS	 = 0x002d,
	XK_PERIOD	 = 0x002e,
	XK_SLASH	 = 0x002f,
	XK_SEMICOLON	 = 0x003b,
	XK_EQUAL	 = 0x003d,
	XK_LEFT_BRACKET	 = 0x005b,
	XK_BACKSLASH	 = 0x005c,
	XK_RIGHT_BRACKET = 0x005d,
	XK_GRAVE	 = 0x0060,
	XK_BACKSPACE	 = 0xff08,
	XK_TAB		 = 0xff09,
	XK_RETURN	 = 0xff0d,
	XK_PAUSE	 = 0xff13,
	XK_SCROLL_LOCK	 = 0xff14,
	XK_ESCAPE	 = 0xff1b,
	XK_HOME		 = 0xff50,
	XK_LEFT		 = 0xff51,
	XK_UP		 = 0xff52,
	XK_RIGHT	 = 0xff53,
	XK_DOWN		 = 0xff54,
	XK_PAGE_UP	 = 0xff55,
	XK_PAGE_DOWN	 = 0xff56,
	XK_END		 = 0xff57,
	XK_PRINT	 = 0xff61,
	XK_INSERT	 = 0xff63,
	XK_MENU		 = 0xff67,
	XK_KP_ENTER	 = 0xff8d,
	XK_KP_HOME	 = 0xff95,
	XK_KP_LEFT	 = 0xff96,
	XK_KP_UP	 = 0xff97,
	XK_KP_RIGHT	 = 0xff98,
	XK_KP_DOWN	 = 0xff99,
	XK_KP_PAGE_UP	 = 0xff9a,
	XK_KP_PAGE_DOWN	 = 0xff9b,
	XK_KP_END	 = 0xff9c,
	XK_KP_BEGIN	 = 0xff9d,
	XK_KP_INSERT	 = 0xff9e,
	XK_KP_DELETE	 = 0xff9f,
	XK_KP_MULTIPLY	 = 0xffaa,
	XK_KP_ADD	 = 0xffab,
	XK_KP_SUBTRACT	 = 0xffad,
	XK_KP_DECIMAL	 = 0xffae,
	XK_KP_DIVIDE	 = 0xffaf,
	XK_KP_0		 = 0xffb0,
	XK_KP_1		 = 0xffb1,
	XK_KP_2		 = 0xffb2,
	XK_KP_3		 = 0xffb3,
	XK_KP_4		 = 0xffb4,
	XK_KP_5		 = 0xffb5,
	XK_KP_6		 = 0xffb6,
	XK_KP_7		 = 0xffb7,
	XK_KP_8		 = 0xffb8,
	XK_KP_9		 = 0xffb9,
	XK_F1		 = 0xffbe,
	XK_F2		 = 0xffbf,
	XK_F3		 = 0xffc0,
	XK_F4		 = 0xffc1,
	XK_F5		 = 0xffc2,
	XK_F6		 = 0xffc3,
	XK_F7		 = 0xffc4,
	XK_F8		 = 0xffc5,
	XK_F9		 = 0xffc6,
	XK_F10		 = 0xffc7,
	XK_F11		 = 0xffc8,
	XK_F12		 = 0xffc9,
	XK_SHIFT_L	 = 0xffe1,
	XK_SHIFT_R	 = 0xffe2,
	XK_CONTROL_L	 = 0xffe3,
	XK_CONTROL_R	 = 0xffe4,
	XK_CAPS_LOCK	 = 0xffe5,
	XK_NUM_LOCK	 = 0xff7f,
	XK_DELETE	 = 0xffff,
	XK_ALT_L	 = 0xffe9,
	XK_ALT_R	 = 0xffea,
	XK_SUPER_L	 = 0xffeb,
	XK_SUPER_R	 = 0xffec,
};

size_t x11_pad4(size_t length)
{
	return (X11_PAD_SIZE - (length & (X11_PAD_SIZE - 1))) & (X11_PAD_SIZE - 1);
}

s32 x11_s16(u16 value)
{
	return value <= (u16)S16_MAX ? (s32)value : (s32)value - 0x10000;
}

display_key_t x11_key_from_keysym(u32 keysym)
{
	if (keysym >= 'a' && keysym <= 'z') {
		return (display_key_t)(DISPLAY_KEY_A + keysym - 'a');
	}

	if (keysym >= 'A' && keysym <= 'Z') {
		return (display_key_t)(DISPLAY_KEY_A + keysym - 'A');
	}

	if (keysym >= '0' && keysym <= '9') {
		return (display_key_t)(DISPLAY_KEY_0 + keysym - '0');
	}

	if (keysym >= XK_KP_0 && keysym <= XK_KP_9) {
		return (display_key_t)(DISPLAY_KEY_KP_0 + keysym - XK_KP_0);
	}

	if (keysym >= XK_F1 && keysym <= XK_F12) {
		return (display_key_t)(DISPLAY_KEY_F1 + keysym - XK_F1);
	}

	switch (keysym) {
	case XK_GRAVE:
		return DISPLAY_KEY_GRAVE;
	case XK_MINUS:
		return DISPLAY_KEY_MINUS;
	case XK_EQUAL:
		return DISPLAY_KEY_EQUAL;
	case XK_LEFT_BRACKET:
		return DISPLAY_KEY_LEFT_BRACKET;
	case XK_RIGHT_BRACKET:
		return DISPLAY_KEY_RIGHT_BRACKET;
	case XK_BACKSLASH:
		return DISPLAY_KEY_BACKSLASH;
	case XK_SEMICOLON:
		return DISPLAY_KEY_SEMICOLON;
	case XK_APOSTROPHE:
		return DISPLAY_KEY_APOSTROPHE;
	case XK_COMMA:
		return DISPLAY_KEY_COMMA;
	case XK_PERIOD:
		return DISPLAY_KEY_PERIOD;
	case XK_SLASH:
		return DISPLAY_KEY_SLASH;
	case ' ':
		return DISPLAY_KEY_SPACE;
	case XK_RETURN:
		return DISPLAY_KEY_ENTER;
	case XK_TAB:
		return DISPLAY_KEY_TAB;
	case XK_BACKSPACE:
		return DISPLAY_KEY_BACKSPACE;
	case XK_ESCAPE:
		return DISPLAY_KEY_ESCAPE;
	case XK_CAPS_LOCK:
		return DISPLAY_KEY_CAPS_LOCK;
	case XK_NUM_LOCK:
		return DISPLAY_KEY_NUM_LOCK;
	case XK_SCROLL_LOCK:
		return DISPLAY_KEY_SCROLL_LOCK;
	case XK_PAUSE:
		return DISPLAY_KEY_PAUSE;
	case XK_PRINT:
		return DISPLAY_KEY_PRINT_SCREEN;
	case XK_INSERT:
		return DISPLAY_KEY_INSERT;
	case XK_DELETE:
		return DISPLAY_KEY_DELETE;
	case XK_HOME:
		return DISPLAY_KEY_HOME;
	case XK_END:
		return DISPLAY_KEY_END;
	case XK_PAGE_UP:
		return DISPLAY_KEY_PAGE_UP;
	case XK_PAGE_DOWN:
		return DISPLAY_KEY_PAGE_DOWN;
	case XK_UP:
		return DISPLAY_KEY_UP;
	case XK_DOWN:
		return DISPLAY_KEY_DOWN;
	case XK_LEFT:
		return DISPLAY_KEY_LEFT;
	case XK_RIGHT:
		return DISPLAY_KEY_RIGHT;
	case XK_SHIFT_L:
		return DISPLAY_KEY_LEFT_SHIFT;
	case XK_SHIFT_R:
		return DISPLAY_KEY_RIGHT_SHIFT;
	case XK_CONTROL_L:
		return DISPLAY_KEY_LEFT_CONTROL;
	case XK_CONTROL_R:
		return DISPLAY_KEY_RIGHT_CONTROL;
	case XK_ALT_L:
		return DISPLAY_KEY_LEFT_ALT;
	case XK_ALT_R:
		return DISPLAY_KEY_RIGHT_ALT;
	case XK_SUPER_L:
		return DISPLAY_KEY_LEFT_SUPER;
	case XK_SUPER_R:
		return DISPLAY_KEY_RIGHT_SUPER;
	case XK_MENU:
		return DISPLAY_KEY_MENU;
	case XK_KP_INSERT:
		return DISPLAY_KEY_KP_0;
	case XK_KP_END:
		return DISPLAY_KEY_KP_1;
	case XK_KP_DOWN:
		return DISPLAY_KEY_KP_2;
	case XK_KP_PAGE_DOWN:
		return DISPLAY_KEY_KP_3;
	case XK_KP_LEFT:
		return DISPLAY_KEY_KP_4;
	case XK_KP_BEGIN:
		return DISPLAY_KEY_KP_5;
	case XK_KP_RIGHT:
		return DISPLAY_KEY_KP_6;
	case XK_KP_HOME:
		return DISPLAY_KEY_KP_7;
	case XK_KP_UP:
		return DISPLAY_KEY_KP_8;
	case XK_KP_PAGE_UP:
		return DISPLAY_KEY_KP_9;
	case XK_KP_DELETE:
	case XK_KP_DECIMAL:
		return DISPLAY_KEY_KP_DECIMAL;
	case XK_KP_DIVIDE:
		return DISPLAY_KEY_KP_DIVIDE;
	case XK_KP_MULTIPLY:
		return DISPLAY_KEY_KP_MULTIPLY;
	case XK_KP_SUBTRACT:
		return DISPLAY_KEY_KP_SUBTRACT;
	case XK_KP_ADD:
		return DISPLAY_KEY_KP_ADD;
	case XK_KP_ENTER:
		return DISPLAY_KEY_KP_ENTER;
	default:
		return DISPLAY_KEY_UNKNOWN;
	}
}

display_modifier_t x11_modifier_from_key(display_key_t key)
{
	switch (key) {
	case DISPLAY_KEY_LEFT_SHIFT:
	case DISPLAY_KEY_RIGHT_SHIFT:
		return DISPLAY_MOD_SHIFT;
	case DISPLAY_KEY_CAPS_LOCK:
		return DISPLAY_MOD_CAPS_LOCK;
	case DISPLAY_KEY_LEFT_CONTROL:
	case DISPLAY_KEY_RIGHT_CONTROL:
		return DISPLAY_MOD_CONTROL;
	case DISPLAY_KEY_LEFT_ALT:
	case DISPLAY_KEY_RIGHT_ALT:
		return DISPLAY_MOD_ALT;
	case DISPLAY_KEY_NUM_LOCK:
		return DISPLAY_MOD_NUM_LOCK;
	case DISPLAY_KEY_LEFT_SUPER:
	case DISPLAY_KEY_RIGHT_SUPER:
		return DISPLAY_MOD_SUPER;
	default:
		return DISPLAY_MOD_NONE;
	}
}

display_mouse_t x11_mouse_from_button(u32 button, const char *tag)
{
	switch (button) {
	case 1:
		return DISPLAY_MOUSE_LEFT;
	case 2:
		return DISPLAY_MOUSE_MIDDLE;
	case 3:
		return DISPLAY_MOUSE_RIGHT;
	case 4:
		return DISPLAY_MOUSE_WHEEL_UP;
	case 5:
		return DISPLAY_MOUSE_WHEEL_DOWN;
	case 6:
		return DISPLAY_MOUSE_WHEEL_LEFT;
	case 7:
		return DISPLAY_MOUSE_WHEEL_RIGHT;
	case 8:
		return DISPLAY_MOUSE_BACK;
	case 9:
		return DISPLAY_MOUSE_FORWARD;
	default:
		log_warn("cdisplay", tag, NULL, "unknown X11 mouse button: %u", button);
		return DISPLAY_MOUSE_UNKNOWN;
	}
}

display_modifier_t x11_modifiers_from_state(const display_modifier_t map[static X_MODIFIER_COUNT], u32 state)
{
	display_modifier_t modifiers = DISPLAY_MOD_NONE;

	for (u8 i = 0; i < X_MODIFIER_COUNT; i++) {
		if (state & (1u << i)) {
			modifiers = (display_modifier_t)(modifiers | map[i]);
		}
	}

	if (state & X_MODIFIER_BUTTON1) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_MOUSE_LEFT);
	}
	if (state & X_MODIFIER_BUTTON2) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_MOUSE_MIDDLE);
	}
	if (state & X_MODIFIER_BUTTON3) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_MOUSE_RIGHT);
	}
	if (state & X_MODIFIER_BUTTON4) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_MOUSE_WHEEL_UP);
	}
	if (state & X_MODIFIER_BUTTON5) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_MOUSE_WHEEL_DOWN);
	}

	return modifiers;
}

void x11_monitor_set(display_monitor_t *monitor, u32 id, s32 x, s32 y, u32 width, u32 height, u32 physical_width, u32 physical_height,
		     int primary, void *native)
{
	mem_set(monitor, 0, sizeof(*monitor));
	monitor->id		 = id;
	monitor->x		 = x;
	monitor->y		 = y;
	monitor->width		 = width;
	monitor->height		 = height;
	monitor->physical_width	 = physical_width;
	monitor->physical_height = physical_height;
	monitor->primary	 = primary;
	monitor->native		 = native;
}
