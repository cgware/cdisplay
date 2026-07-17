#include "display_driver.h"

#include "log.h"
#include "mem.h"

#include <limits.h>

typedef void Display;
typedef unsigned long XID;
typedef unsigned long Window;
typedef unsigned long Atom;
typedef unsigned long Colormap;
typedef unsigned long VisualID;
typedef unsigned long Time;
typedef unsigned long KeySym;
typedef unsigned char KeyCode;
typedef int Bool;
typedef int Status;

typedef struct Visual_s {
	void *ext_data;
	VisualID visualid;
	int class;
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int bits_per_rgb;
	int map_entries;
} Visual;

typedef struct XSetWindowAttributes_s {
	unsigned long background_pixmap;
	unsigned long background_pixel;
	unsigned long border_pixmap;
	unsigned long border_pixel;
	int bit_gravity;
	int win_gravity;
	int backing_store;
	unsigned long backing_planes;
	unsigned long backing_pixel;
	Bool save_under;
	long event_mask;
	long do_not_propagate_mask;
	Bool override_redirect;
	Colormap colormap;
	unsigned long cursor;
} XSetWindowAttributes;

typedef struct XWindowAttributes_s {
	int x;
	int y;
	int width;
	int height;
	int border_width;
	int depth;
	Visual *visual;
	Window root;
	int class;
	int bit_gravity;
	int win_gravity;
	int backing_store;
	unsigned long backing_planes;
	unsigned long backing_pixel;
	Bool save_under;
	Colormap colormap;
	Bool map_installed;
	int map_state;
	long all_event_masks;
	long your_event_mask;
	long do_not_propagate_mask;
	Bool override_redirect;
	void *screen;
} XWindowAttributes;

typedef struct XVisualInfo_s {
	Visual *visual;
	VisualID visualid;
	int screen;
	int depth;
	int class;
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int colormap_size;
	int bits_per_rgb;
} XVisualInfo;

typedef struct XModifierKeymap_s {
	int max_keypermod;
	KeyCode *modifiermap;
} XModifierKeymap;

typedef struct XAnyEvent_s {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
} XAnyEvent;

typedef struct XKeyEvent_s {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	Window root;
	Window subwindow;
	Time time;
	int x;
	int y;
	int x_root;
	int y_root;
	unsigned int state;
	unsigned int keycode;
	Bool same_screen;
} XKeyEvent;

typedef XKeyEvent XButtonEvent;
typedef XKeyEvent XMotionEvent;

typedef struct XFocusChangeEvent_s {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	int mode;
	int detail;
} XFocusChangeEvent;

typedef struct XDestroyWindowEvent_s {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window event;
	Window window;
} XDestroyWindowEvent;

typedef struct XConfigureEvent_s {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window event;
	Window window;
	int x;
	int y;
	int width;
	int height;
	int border_width;
	Window above;
	Bool override_redirect;
} XConfigureEvent;

typedef union XClientMessageData_u {
	char b[20];
	short s[10];
	long l[5];
} XClientMessageData;

typedef struct XClientMessageEvent_s {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	Atom message_type;
	int format;
	XClientMessageData data;
} XClientMessageEvent;

typedef union XEvent_u {
	int type;
	XAnyEvent xany;
	XKeyEvent xkey;
	XButtonEvent xbutton;
	XMotionEvent xmotion;
	XFocusChangeEvent xfocus;
	XDestroyWindowEvent xdestroywindow;
	XConfigureEvent xconfigure;
	XClientMessageEvent xclient;
	long pad[24];
} XEvent;

typedef struct x11_s {
	Display *(*OpenDisplay)(const char *);
	int (*CloseDisplay)(Display *);
	int (*DefaultScreen)(Display *);
	Window (*RootWindow)(Display *, int);
	unsigned long (*WhitePixel)(Display *, int);
	unsigned long (*BlackPixel)(Display *, int);
	Visual *(*DefaultVisual)(Display *, int);
	Atom (*InternAtom)(Display *, const char *, Bool);
	Window (*CreateWindow)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *,
			       unsigned long, XSetWindowAttributes *);
	int (*DestroyWindow)(Display *, Window);
	Colormap (*CreateColormap)(Display *, Window, Visual *, int);
	int (*FreeColormap)(Display *, Colormap);
	int (*ChangeProperty)(Display *, Window, Atom, Atom, int, int, const unsigned char *, int);
	Status (*SendEvent)(Display *, Window, Bool, long, XEvent *);
	Status (*SetWMProtocols)(Display *, Window, Atom *, int);
	int (*MapWindow)(Display *, Window);
	int (*UnmapWindow)(Display *, Window);
	int (*MoveWindow)(Display *, Window, int, int);
	int (*ResizeWindow)(Display *, Window, unsigned int, unsigned int);
	int (*Pending)(Display *);
	int (*NextEvent)(Display *, XEvent *);
	int (*Flush)(Display *);
	int (*Sync)(Display *, Bool);
	KeySym (*LookupKeysym)(XKeyEvent *, int);
	void (*DisplayKeycodes)(Display *, int *, int *);
	KeySym *(*GetKeyboardMapping)(Display *, KeyCode, int, int *);
	XModifierKeymap *(*GetModifierMapping)(Display *);
	int (*FreeModifiermap)(XModifierKeymap *);
	int (*GetWindowAttributes)(Display *, Window, XWindowAttributes *);
	XVisualInfo *(*GetVisualInfo)(Display *, long, XVisualInfo *, int *);
	XID (*AllocID)(Display *);
	int (*QueryExtension)(Display *, const char *, int *, int *, int *);
	int (*Free)(void *);
} x11_t;

typedef struct display_x11_dynamic_s {
	proc_t *proc;
	void *lib;
	x11_t x11;
	Display *display;
	int screen;
	Window root;
	unsigned long white_pixel;
	unsigned long black_pixel;
	Visual *default_visual;
	Atom wm_protocols;
	Atom wm_delete_window;
	Atom wm_name;
	Atom net_wm_name;
	Atom utf8_string;
	Atom motif_wm_hints;
	Atom net_wm_state;
	Atom net_wm_state_fullscreen;
	display_key_t keys[256];
	display_modifier_t modifiers[8];
} display_x11_dynamic_t;

typedef struct window_x11_dynamic_s {
	Window id;
	Colormap colormap;
	int mapped;
} window_x11_dynamic_t;

enum {
	X_FALSE = 0,
	X_TRUE	= 1,
	X_NONE	= 0,
};

enum {
	X_COPY_FROM_PARENT = 0,
	X_INPUT_OUTPUT	   = 1,
};

enum {
	X_ALLOC_NONE = 0,
};

enum {
	X_CW_BACK_PIXEL	  = 1L << 1,
	X_CW_BORDER_PIXEL = 1L << 3,
	X_CW_EVENT_MASK	  = 1L << 11,
	X_CW_COLORMAP	  = 1L << 13,
};

enum {
	X_EVENT_MASK_KEY_PRESS		   = 1L << 0,
	X_EVENT_MASK_KEY_RELEASE	   = 1L << 1,
	X_EVENT_MASK_BUTTON_PRESS	   = 1L << 2,
	X_EVENT_MASK_BUTTON_RELEASE	   = 1L << 3,
	X_EVENT_MASK_POINTER_MOTION	   = 1L << 6,
	X_EVENT_MASK_EXPOSURE		   = 1L << 15,
	X_EVENT_MASK_STRUCTURE		   = 1L << 17,
	X_EVENT_MASK_SUBSTRUCTURE_NOTIFY   = 1L << 19,
	X_EVENT_MASK_SUBSTRUCTURE_REDIRECT = 1L << 20,
	X_EVENT_MASK_FOCUS_CHANGE	   = 1L << 21,
};

enum {
	X_KEY_PRESS	   = 2,
	X_KEY_RELEASE	   = 3,
	X_BUTTON_PRESS	   = 4,
	X_BUTTON_RELEASE   = 5,
	X_MOTION_NOTIFY	   = 6,
	X_FOCUS_IN	   = 9,
	X_FOCUS_OUT	   = 10,
	X_EXPOSE	   = 12,
	X_DESTROY_NOTIFY   = 17,
	X_UNMAP_NOTIFY	   = 18,
	X_MAP_NOTIFY	   = 19,
	X_REPARENT_NOTIFY  = 21,
	X_CONFIGURE_NOTIFY = 22,
	X_CLIENT_MESSAGE   = 33,
	X_MAPPING_NOTIFY   = 34,
	X_EVENT_IGNORED	   = 2,
};

enum {
	XA_ATOM	  = 4,
	XA_STRING = 31,
};

enum {
	X_PROP_MODE_REPLACE = 0,
};

enum {
	X_VISUAL_ID_MASK     = 0x1,
	X_VISUAL_SCREEN_MASK = 0x2,
};

enum {
	MOTIF_WM_HINTS_FIELD_COUNT	= 5,
	MOTIF_WM_HINTS_DECORATIONS_FLAG = 1L << 1,
	MOTIF_WM_DECOR_ALL		= 1,
};

enum {
	NET_WM_STATE_REMOVE = 0,
	NET_WM_STATE_ADD    = 1,
};

enum {
	X_MODIFIER_COUNT   = 8,
	X_MODIFIER_BUTTON1 = 1u << 8,
	X_MODIFIER_BUTTON2 = 1u << 9,
	X_MODIFIER_BUTTON3 = 1u << 10,
	X_MODIFIER_BUTTON4 = 1u << 11,
	X_MODIFIER_BUTTON5 = 1u << 12,
};

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

static int load_symbol(display_x11_dynamic_t *dx11, void **sym, strv_t name)
{
	if (proc_dlsym(dx11->proc, dx11->lib, name, sym)) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to load X11 symbol: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

#define LOAD_X11(_dx11, _name)                                                                                                             \
	do {                                                                                                                               \
		if (load_symbol((_dx11), (void **)&(_dx11)->x11._name, STRV("X" #_name))) {                                                \
			return 1;                                                                                                          \
		}                                                                                                                          \
	} while (0)

static int load_x11(display_x11_dynamic_t *dx11)
{
	if (proc_dlopen(dx11->proc, STRV("libX11.so.6"), &dx11->lib) && proc_dlopen(dx11->proc, STRV("libX11.so"), &dx11->lib)) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to load libX11.so");
		return 1;
	}

	LOAD_X11(dx11, OpenDisplay);
	LOAD_X11(dx11, CloseDisplay);
	LOAD_X11(dx11, DefaultScreen);
	LOAD_X11(dx11, RootWindow);
	LOAD_X11(dx11, WhitePixel);
	LOAD_X11(dx11, BlackPixel);
	LOAD_X11(dx11, DefaultVisual);
	LOAD_X11(dx11, InternAtom);
	LOAD_X11(dx11, CreateWindow);
	LOAD_X11(dx11, DestroyWindow);
	LOAD_X11(dx11, CreateColormap);
	LOAD_X11(dx11, FreeColormap);
	LOAD_X11(dx11, ChangeProperty);
	LOAD_X11(dx11, SendEvent);
	LOAD_X11(dx11, SetWMProtocols);
	LOAD_X11(dx11, MapWindow);
	LOAD_X11(dx11, UnmapWindow);
	LOAD_X11(dx11, MoveWindow);
	LOAD_X11(dx11, ResizeWindow);
	LOAD_X11(dx11, Pending);
	LOAD_X11(dx11, NextEvent);
	LOAD_X11(dx11, Flush);
	LOAD_X11(dx11, Sync);
	LOAD_X11(dx11, LookupKeysym);
	LOAD_X11(dx11, DisplayKeycodes);
	LOAD_X11(dx11, GetKeyboardMapping);
	LOAD_X11(dx11, GetModifierMapping);
	LOAD_X11(dx11, FreeModifiermap);
	LOAD_X11(dx11, GetWindowAttributes);
	LOAD_X11(dx11, GetVisualInfo);
	if (load_symbol(dx11, (void **)&dx11->x11.AllocID, STRV("_XAllocID"))) {
		return 1;
	}
	LOAD_X11(dx11, QueryExtension);
	LOAD_X11(dx11, Free);

	return 0;
}

#undef LOAD_X11

static display_key_t key_from_keysym(KeySym keysym)
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

static display_modifier_t modifier_from_key(display_key_t key)
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

static display_mouse_t mouse_from_button(unsigned int button)
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
		log_warn("cdisplay", "display_x11_dynamic", NULL, "unknown X11 mouse button: %u", button);
		return DISPLAY_MOUSE_UNKNOWN;
	}
}

static display_modifier_t modifiers_from_state(display_x11_dynamic_t *dx11, unsigned int state)
{
	display_modifier_t modifiers = DISPLAY_MOD_NONE;

	for (u8 i = 0; i < X_MODIFIER_COUNT; i++) {
		if (state & (1u << i)) {
			modifiers = (display_modifier_t)(modifiers | dx11->modifiers[i]);
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

static int init_atoms(display_t *display)
{
	display_x11_dynamic_t *dx11 = display->data;

	dx11->wm_protocols	      = dx11->x11.InternAtom(dx11->display, "WM_PROTOCOLS", X_FALSE);
	dx11->wm_delete_window	      = dx11->x11.InternAtom(dx11->display, "WM_DELETE_WINDOW", X_FALSE);
	dx11->wm_name		      = dx11->x11.InternAtom(dx11->display, "WM_NAME", X_FALSE);
	dx11->net_wm_name	      = dx11->x11.InternAtom(dx11->display, "_NET_WM_NAME", X_FALSE);
	dx11->utf8_string	      = dx11->x11.InternAtom(dx11->display, "UTF8_STRING", X_FALSE);
	dx11->motif_wm_hints	      = dx11->x11.InternAtom(dx11->display, "_MOTIF_WM_HINTS", X_FALSE);
	dx11->net_wm_state	      = dx11->x11.InternAtom(dx11->display, "_NET_WM_STATE", X_FALSE);
	dx11->net_wm_state_fullscreen = dx11->x11.InternAtom(dx11->display, "_NET_WM_STATE_FULLSCREEN", X_FALSE);

	if (dx11->wm_protocols == X_NONE || dx11->wm_delete_window == X_NONE || dx11->wm_name == X_NONE || dx11->net_wm_name == X_NONE ||
	    dx11->utf8_string == X_NONE || dx11->motif_wm_hints == X_NONE || dx11->net_wm_state == X_NONE ||
	    dx11->net_wm_state_fullscreen == X_NONE) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to intern X11 atoms");
		return 1;
	}

	return 0;
}

static int init_keys(display_t *display)
{
	display_x11_dynamic_t *dx11 = display->data;
	int min_keycode		    = 0;
	int max_keycode		    = 0;
	int keysyms_per_keycode	    = 0;

	dx11->x11.DisplayKeycodes(dx11->display, &min_keycode, &max_keycode);
	if (min_keycode < 0 || min_keycode > max_keycode || max_keycode > 255) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "invalid keycode range");
		return 1;
	}

	int keycode_count = max_keycode - min_keycode + 1;
	KeySym *keysyms	  = dx11->x11.GetKeyboardMapping(dx11->display, (KeyCode)min_keycode, keycode_count, &keysyms_per_keycode);
	if (keysyms == NULL || keysyms_per_keycode <= 0) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to get keyboard mapping");
		if (keysyms != NULL) {
			dx11->x11.Free(keysyms);
		}
		return 1;
	}

	for (int i = 0; i < keycode_count; i++) {
		for (int j = 0; j < keysyms_per_keycode; j++) {
			display_key_t key = key_from_keysym(keysyms[i * keysyms_per_keycode + j]);
			if (key != DISPLAY_KEY_UNKNOWN) {
				dx11->keys[min_keycode + i] = key;
				break;
			}
		}
	}

	dx11->x11.Free(keysyms);
	return 0;
}

static int init_modifiers(display_t *display)
{
	display_x11_dynamic_t *dx11 = display->data;

	XModifierKeymap *map = dx11->x11.GetModifierMapping(dx11->display);
	if (map == NULL) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to get modifier mapping");
		return 1;
	}

	for (int i = 0; i < X_MODIFIER_COUNT; i++) {
		for (int j = 0; j < map->max_keypermod; j++) {
			KeyCode keycode = map->modifiermap[i * map->max_keypermod + j];
			if (keycode == 0) {
				continue;
			}
			dx11->modifiers[i] = (display_modifier_t)(dx11->modifiers[i] | modifier_from_key(dx11->keys[keycode]));
		}
	}

	dx11->x11.FreeModifiermap(map);
	return 0;
}

static int open_display(display_t *display)
{
	display_x11_dynamic_t *dx11 = display->data;
	strv_t name		    = proc_getenv(display->proc, STRV("DISPLAY"));

	if (name.data == NULL || name.len >= 256) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to get display name");
		return 1;
	}

	char display_name[256] = {0};
	mem_copy(display_name, sizeof(display_name), name.data, name.len);

	dx11->display = dx11->x11.OpenDisplay(display_name);
	if (dx11->display == NULL) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to open display");
		return 1;
	}

	dx11->screen	     = dx11->x11.DefaultScreen(dx11->display);
	dx11->root	     = dx11->x11.RootWindow(dx11->display, dx11->screen);
	dx11->white_pixel    = dx11->x11.WhitePixel(dx11->display, dx11->screen);
	dx11->black_pixel    = dx11->x11.BlackPixel(dx11->display, dx11->screen);
	dx11->default_visual = dx11->x11.DefaultVisual(dx11->display, dx11->screen);

	return 0;
}

static Visual *visual_from_id(display_x11_dynamic_t *dx11, VisualID id, int *depth)
{
	XVisualInfo template = {
		.visualid = id,
		.screen	  = dx11->screen,
	};
	int count = 0;

	XVisualInfo *info = dx11->x11.GetVisualInfo(dx11->display, X_VISUAL_ID_MASK | X_VISUAL_SCREEN_MASK, &template, &count);
	if (info == NULL || count == 0) {
		if (info != NULL) {
			dx11->x11.Free(info);
		}
		return NULL;
	}

	Visual *visual = info[0].visual;
	if (depth != NULL) {
		*depth = info[0].depth;
	}
	dx11->x11.Free(info);
	return visual;
}

static int create_colormap(window_t *wnd, Visual *visual)
{
	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;

	wx11->colormap = dx11->x11.CreateColormap(dx11->display, dx11->root, visual, X_ALLOC_NONE);
	if (wx11->colormap == X_NONE) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to create colormap");
		return 1;
	}

	return 0;
}

static int free_colormap(window_t *wnd)
{
	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;

	if (wx11->colormap == X_NONE) {
		return 0;
	}

	int ret	       = dx11->x11.FreeColormap(dx11->display, wx11->colormap) == 0 ? 1 : 0;
	wx11->colormap = X_NONE;
	if (ret) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to free colormap");
	}
	return ret;
}

static int create_window(window_t *wnd, const window_config_t *config)
{
	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;
	Visual *visual		    = NULL;
	int depth		    = X_COPY_FROM_PARENT;

	if (config->visual != 0) {
		visual = visual_from_id(dx11, config->visual, &depth);
		if (visual == NULL) {
			log_error("cdisplay", "display_x11_dynamic", NULL, "unknown X11 visual: %u", config->visual);
			return 1;
		}
		if (config->depth != 0) {
			depth = config->depth;
		}
		if (create_colormap(wnd, visual)) {
			return 1;
		}
	}

	XSetWindowAttributes attrs = {
		.background_pixel = dx11->white_pixel,
		.border_pixel	  = dx11->black_pixel,
		.event_mask = X_EVENT_MASK_KEY_PRESS | X_EVENT_MASK_KEY_RELEASE | X_EVENT_MASK_BUTTON_PRESS | X_EVENT_MASK_BUTTON_RELEASE |
			      X_EVENT_MASK_POINTER_MOTION | X_EVENT_MASK_EXPOSURE | X_EVENT_MASK_STRUCTURE | X_EVENT_MASK_FOCUS_CHANGE,
		.colormap = wx11->colormap,
	};
	unsigned long value_mask = X_CW_BORDER_PIXEL | X_CW_EVENT_MASK;
	if (config->background != WINDOW_BACKGROUND_NONE) {
		value_mask |= X_CW_BACK_PIXEL;
	}
	if (wx11->colormap != X_NONE) {
		value_mask |= X_CW_COLORMAP;
	}

	wx11->id = dx11->x11.CreateWindow(dx11->display,
					  dx11->root,
					  config->x,
					  config->y,
					  config->width,
					  config->height,
					  1,
					  depth,
					  X_INPUT_OUTPUT,
					  visual,
					  value_mask,
					  &attrs);
	if (wx11->id == X_NONE) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to create window");
		free_colormap(wnd);
		return 1;
	}

	return dx11->x11.Flush(dx11->display) == 0 ? 1 : 0;
}

static int set_wm_protocols(window_t *wnd)
{
	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;
	Atom protocols[]	    = {dx11->wm_delete_window};

	if (dx11->x11.SetWMProtocols(dx11->display, wx11->id, protocols, 1) == 0) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to set WM protocols");
		return 1;
	}

	return dx11->x11.Flush(dx11->display) == 0 ? 1 : 0;
}

static int set_property_text(window_t *wnd, Atom property, Atom type, strv_t text)
{
	if (text.data == NULL && text.len > 0) {
		return 1;
	}
	if (text.len > (size_t)INT_MAX) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "property text is too long");
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;

	if (dx11->x11.ChangeProperty(
		    dx11->display, wx11->id, property, type, 8, X_PROP_MODE_REPLACE, (const unsigned char *)text.data, (int)text.len) ==
	    0) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to set window text property");
		return 1;
	}

	return dx11->x11.Flush(dx11->display) == 0 ? 1 : 0;
}

static int set_property_long(window_t *wnd, Atom property, Atom type, const long *values, int count)
{
	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;

	if (dx11->x11.ChangeProperty(
		    dx11->display, wx11->id, property, type, 32, X_PROP_MODE_REPLACE, (const unsigned char *)values, count) == 0) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to set window property");
		return 1;
	}

	return dx11->x11.Flush(dx11->display) == 0 ? 1 : 0;
}

static int set_borderless(window_t *wnd, int borderless)
{
	display_x11_dynamic_t *dx11	       = wnd->display->data;
	long hints[MOTIF_WM_HINTS_FIELD_COUNT] = {
		MOTIF_WM_HINTS_DECORATIONS_FLAG,
		0,
		borderless ? 0 : MOTIF_WM_DECOR_ALL,
		0,
		0,
	};

	return set_property_long(wnd, dx11->motif_wm_hints, dx11->motif_wm_hints, hints, MOTIF_WM_HINTS_FIELD_COUNT);
}

static int set_fullscreen_property(window_t *wnd, int fullscreen)
{
	display_x11_dynamic_t *dx11 = wnd->display->data;
	long state[]		    = {(long)dx11->net_wm_state_fullscreen};

	return set_property_long(wnd, dx11->net_wm_state, XA_ATOM, state, fullscreen ? 1 : 0);
}

static int send_fullscreen_message(window_t *wnd, int fullscreen)
{
	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;
	XEvent event		    = {0};

	event.xclient.type	   = X_CLIENT_MESSAGE;
	event.xclient.window	   = wx11->id;
	event.xclient.message_type = dx11->net_wm_state;
	event.xclient.format	   = 32;
	event.xclient.data.l[0]	   = fullscreen ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE;
	event.xclient.data.l[1]	   = (long)dx11->net_wm_state_fullscreen;
	event.xclient.data.l[2]	   = 0;
	event.xclient.data.l[3]	   = 1;
	event.xclient.data.l[4]	   = 0;

	if (dx11->x11.SendEvent(
		    dx11->display, dx11->root, X_FALSE, X_EVENT_MASK_SUBSTRUCTURE_REDIRECT | X_EVENT_MASK_SUBSTRUCTURE_NOTIFY, &event) ==
	    0) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to set fullscreen state");
		return 1;
	}

	return dx11->x11.Flush(dx11->display) == 0 ? 1 : 0;
}

static int set_fullscreen(window_t *wnd, int fullscreen)
{
	window_x11_dynamic_t *wx11 = wnd->data;

	if (wx11->mapped) {
		return send_fullscreen_message(wnd, fullscreen);
	}

	return set_fullscreen_property(wnd, fullscreen);
}

static int read_x11_event(display_t *display, display_event_t *event)
{
	display_x11_dynamic_t *dx11 = display->data;
	XEvent xev		    = {0};

	*event = (display_event_t){0};
	dx11->x11.NextEvent(dx11->display, &xev);

	switch (xev.type) {
	case X_KEY_PRESS:
	case X_KEY_RELEASE: {
		event->window	 = (u32)xev.xkey.window;
		event->x	 = (u16)xev.xkey.x;
		event->y	 = (u16)xev.xkey.y;
		event->modifiers = modifiers_from_state(dx11, xev.xkey.state);
		event->type	 = xev.type == X_KEY_PRESS ? DISPLAY_EVENT_KEY_DOWN : DISPLAY_EVENT_KEY_UP;
		event->key	 = key_from_keysym(dx11->x11.LookupKeysym(&xev.xkey, 0));
		return 0;
	}
	case X_BUTTON_PRESS:
	case X_BUTTON_RELEASE: {
		event->window	 = (u32)xev.xbutton.window;
		event->x	 = (u16)xev.xbutton.x;
		event->y	 = (u16)xev.xbutton.y;
		event->modifiers = modifiers_from_state(dx11, xev.xbutton.state);
		event->type	 = xev.type == X_BUTTON_PRESS ? DISPLAY_EVENT_MOUSE_DOWN : DISPLAY_EVENT_MOUSE_UP;
		event->button	 = mouse_from_button(xev.xbutton.keycode);
		return 0;
	}
	case X_MOTION_NOTIFY: {
		event->window	 = (u32)xev.xmotion.window;
		event->x	 = (u16)xev.xmotion.x;
		event->y	 = (u16)xev.xmotion.y;
		event->modifiers = modifiers_from_state(dx11, xev.xmotion.state);
		event->type	 = DISPLAY_EVENT_MOUSE_MOVE;
		return 0;
	}
	case X_FOCUS_IN:
	case X_FOCUS_OUT: {
		event->window = (u32)xev.xfocus.window;
		event->type   = xev.type == X_FOCUS_IN ? DISPLAY_EVENT_FOCUS_GAINED : DISPLAY_EVENT_FOCUS_LOST;
		return 0;
	}
	case X_DESTROY_NOTIFY: {
		event->window = (u32)xev.xdestroywindow.window;
		event->type   = DISPLAY_EVENT_CLOSE;
		return 0;
	}
	case X_CONFIGURE_NOTIFY: {
		event->window = (u32)xev.xconfigure.window;
		event->x      = (u16)xev.xconfigure.x;
		event->y      = (u16)xev.xconfigure.y;
		event->width  = (u16)xev.xconfigure.width;
		event->height = (u16)xev.xconfigure.height;
		event->type   = DISPLAY_EVENT_RESIZE;
		return 0;
	}
	case X_CLIENT_MESSAGE: {
		if (xev.xclient.format == 32 && xev.xclient.message_type == dx11->wm_protocols &&
		    (Atom)xev.xclient.data.l[0] == dx11->wm_delete_window) {
			event->window = (u32)xev.xclient.window;
			event->type   = DISPLAY_EVENT_CLOSE;
			return 0;
		}
		return X_EVENT_IGNORED;
	}
	case X_EXPOSE:
	case X_UNMAP_NOTIFY:
	case X_MAP_NOTIFY:
	case X_REPARENT_NOTIFY:
	case X_MAPPING_NOTIFY:
		return X_EVENT_IGNORED;
	default:
		log_error("cdisplay", "display_x11_dynamic", NULL, "unsupported X11 event: %u", xev.type);
		return 1;
	}
}

static int display_x11_dynamic_init(display_t *display)
{
	if (display == NULL || display->alloc.alloc == NULL) {
		return 1;
	}

	log_info("cdisplay", "display_x11_dynamic", NULL, "Initializing X11...");

	display->data = alloc_alloc(&display->alloc, sizeof(display_x11_dynamic_t));
	if (display->data == NULL) {
		return 1;
	}
	mem_set(display->data, 0, sizeof(display_x11_dynamic_t));

	display_x11_dynamic_t *dx11 = display->data;
	dx11->proc		    = display->proc;
	if (load_x11(dx11) || open_display(display) || init_keys(display) || init_modifiers(display) || init_atoms(display)) {
		if (dx11->display != NULL) {
			dx11->x11.CloseDisplay(dx11->display);
		}
		if (dx11->lib != NULL) {
			proc_dlclose(dx11->proc, dx11->lib);
		}
		alloc_free(&display->alloc, display->data, sizeof(display_x11_dynamic_t));
		display->data = NULL;
		return 1;
	}

	return 0;
}

static int display_x11_dynamic_free(display_t *display)
{
	if (display == NULL || display->data == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = display->data;

	log_info("cdisplay", "display_x11_dynamic", NULL, "Freeing X11...");

	if (dx11->display != NULL) {
		dx11->x11.CloseDisplay(dx11->display);
	}
	if (dx11->lib != NULL) {
		proc_dlclose(dx11->proc, dx11->lib);
	}

	alloc_free(&display->alloc, display->data, sizeof(display_x11_dynamic_t));
	return 0;
}

static int display_x11_dynamic_poll_events(display_t *display)
{
	if (display == NULL || display->data == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = display->data;
	if (dx11->x11.Pending(dx11->display) == 0) {
		return 0;
	}

	int ret;
	display_event_t event = {0};
	do {
		ret = read_x11_event(display, &event);
	} while (ret == X_EVENT_IGNORED && dx11->x11.Pending(dx11->display) != 0);

	if (ret == X_EVENT_IGNORED) {
		return 0;
	}
	if (ret == 0) {
		display_emit_event(display, &event);
	}

	return ret;
}

static int display_x11_dynamic_wait_events(display_t *display)
{
	if (display == NULL || display->data == NULL) {
		return 1;
	}

	int ret;
	display_event_t event = {0};
	do {
		ret = read_x11_event(display, &event);
	} while (ret == X_EVENT_IGNORED);

	if (ret == 0) {
		display_emit_event(display, &event);
	}

	return ret;
}

static int display_x11_dynamic_native(display_t *display, display_native_t *native)
{
	if (display == NULL || display->data == NULL || native == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = display->data;
	native->type		    = DISPLAY_NATIVE_X11;
	native->display		    = dx11->display;
	native->screen		    = dx11->screen;
	return native->display == NULL;
}

static int display_x11_dynamic_native_free(display_t *display, void *data)
{
	if (display == NULL || display->data == NULL || data == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = display->data;
	return dx11->x11.Free(data);
}

static int display_x11_dynamic_window_init(window_t *wnd, const window_config_t *config)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->display->alloc.alloc == NULL || config == NULL) {
		return 1;
	}

	wnd->data = alloc_alloc(&wnd->display->alloc, sizeof(window_x11_dynamic_t));
	if (wnd->data == NULL) {
		return 1;
	}
	mem_set(wnd->data, 0, sizeof(window_x11_dynamic_t));

	if (create_window(wnd, config) || set_wm_protocols(wnd)) {
		display_x11_dynamic_t *dx11 = wnd->display->data;
		window_x11_dynamic_t *wx11  = wnd->data;
		if (wx11->id != X_NONE) {
			dx11->x11.DestroyWindow(dx11->display, wx11->id);
		}
		free_colormap(wnd);
		alloc_free(&wnd->display->alloc, wnd->data, sizeof(window_x11_dynamic_t));
		wnd->data = NULL;
		return 1;
	}

	return 0;
}

static int display_x11_dynamic_window_free(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;

	if (wx11->id != X_NONE) {
		dx11->x11.DestroyWindow(dx11->display, wx11->id);
	}
	free_colormap(wnd);
	dx11->x11.Flush(dx11->display);

	alloc_free(&wnd->display->alloc, wnd->data, sizeof(window_x11_dynamic_t));
	return 0;
}

static u32 display_x11_dynamic_window_id(window_t *wnd)
{
	if (wnd == NULL || wnd->data == NULL) {
		return 0;
	}

	window_x11_dynamic_t *wx11 = wnd->data;
	return (u32)wx11->id;
}

static int display_x11_dynamic_window_native(window_t *wnd, window_native_t *native)
{
	if (wnd == NULL || wnd->data == NULL || native == NULL) {
		return 1;
	}

	window_x11_dynamic_t *wx11 = wnd->data;
	native->type		   = DISPLAY_NATIVE_X11;
	native->window		   = (void *)(uintptr_t)wx11->id;
	return wx11->id == X_NONE;
}

static int display_x11_dynamic_window_set_title(window_t *wnd, strv_t title)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	return set_property_text(wnd, dx11->wm_name, XA_STRING, title) ||
	       set_property_text(wnd, dx11->net_wm_name, dx11->utf8_string, title);
}

static int display_x11_dynamic_window_set_position(window_t *wnd, u16 x, u16 y)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;
	return dx11->x11.MoveWindow(dx11->display, wx11->id, x, y) == 0 || dx11->x11.Flush(dx11->display) == 0;
}

static int display_x11_dynamic_window_set_size(window_t *wnd, u16 width, u16 height)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;
	return dx11->x11.ResizeWindow(dx11->display, wx11->id, width, height) == 0 || dx11->x11.Flush(dx11->display) == 0;
}

static int display_x11_dynamic_window_set_borderless(window_t *wnd, int borderless)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	return set_borderless(wnd, borderless);
}

static int display_x11_dynamic_window_set_fullscreen(window_t *wnd, int fullscreen)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	return set_fullscreen(wnd, fullscreen);
}

static int display_x11_dynamic_window_show(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;
	if (dx11->x11.MapWindow(dx11->display, wx11->id) == 0 || dx11->x11.Flush(dx11->display) == 0) {
		return 1;
	}

	wx11->mapped = 1;
	return 0;
}

static int display_x11_dynamic_window_hide(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	window_x11_dynamic_t *wx11  = wnd->data;
	if (dx11->x11.UnmapWindow(dx11->display, wx11->id) == 0 || dx11->x11.Flush(dx11->display) == 0) {
		return 1;
	}

	wx11->mapped = 0;
	return 0;
}

static int display_x11_dynamic_ext_init(display_ext_t *ext, strv_t name)
{
	if (ext == NULL || ext->display == NULL || ext->display->data == NULL || name.data == NULL || name.len > INT_MAX) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = ext->display->data;
	char buf[256]		    = {0};
	if (name.len >= sizeof(buf)) {
		return 1;
	}
	mem_copy(buf, sizeof(buf), name.data, name.len);

	int opcode;
	int first_event;
	int first_error;
	if (dx11->x11.QueryExtension(dx11->display, buf, &opcode, &first_event, &first_error) == 0) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "display ext is unavailable: %.*s", name.len, name.data);
		return 1;
	}

	ext->opcode	 = (u8)opcode;
	ext->first_event = (u8)first_event;
	ext->first_error = (u8)first_error;
	return 0;
}

static int display_x11_dynamic_alloc_id(display_t *display, u32 *id)
{
	if (display == NULL || display->data == NULL || id == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = display->data;
	*id			    = (u32)dx11->x11.AllocID(dx11->display);
	return *id == 0;
}

static int display_x11_dynamic_visual_depth(display_t *display, u32 visual, u8 *depth)
{
	if (display == NULL || display->data == NULL || depth == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = display->data;
	int xdepth		    = 0;
	if (visual_from_id(dx11, visual, &xdepth) == NULL || xdepth < 0 || xdepth > UINT8_MAX) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "unknown X11 visual: %u", visual);
		return 1;
	}

	*depth = (u8)xdepth;
	return 0;
}

static display_driver_t display_x11_dynamic = {
	.name		       = "X11-dynamic",
	.init		       = display_x11_dynamic_init,
	.free		       = display_x11_dynamic_free,
	.poll_events	       = display_x11_dynamic_poll_events,
	.wait_events	       = display_x11_dynamic_wait_events,
	.native		       = display_x11_dynamic_native,
	.native_free	       = display_x11_dynamic_native_free,
	.window_init	       = display_x11_dynamic_window_init,
	.window_free	       = display_x11_dynamic_window_free,
	.window_id	       = display_x11_dynamic_window_id,
	.window_native	       = display_x11_dynamic_window_native,
	.window_set_title      = display_x11_dynamic_window_set_title,
	.window_set_position   = display_x11_dynamic_window_set_position,
	.window_set_size       = display_x11_dynamic_window_set_size,
	.window_set_borderless = display_x11_dynamic_window_set_borderless,
	.window_set_fullscreen = display_x11_dynamic_window_set_fullscreen,
	.window_show	       = display_x11_dynamic_window_show,
	.window_hide	       = display_x11_dynamic_window_hide,
	.ext_init	       = display_x11_dynamic_ext_init,
	.alloc_id	       = display_x11_dynamic_alloc_id,
	.visual_depth	       = display_x11_dynamic_visual_depth,
};

DISPLAY_DRIVER(display_x11_dynamic, &display_x11_dynamic);
