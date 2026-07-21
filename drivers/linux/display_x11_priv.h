#ifndef DISPLAY_X11_PRIV_H
#define DISPLAY_X11_PRIV_H

#include "display_driver.h"

typedef void Display;
typedef unsigned long XID;
typedef unsigned long Window;
typedef unsigned long Atom;
typedef unsigned long Colormap;
typedef unsigned long VisualID;
typedef unsigned long Time;
typedef unsigned long KeySym;
typedef unsigned char KeyCode;
typedef XID RROutput;
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

typedef struct XRRMonitorInfo_s {
	Atom name;
	Bool primary;
	Bool automatic;
	int noutput;
	int x;
	int y;
	int width;
	int height;
	int mwidth;
	int mheight;
	RROutput *outputs;
} XRRMonitorInfo;

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

enum {
	X11_PAD_SIZE		    = 4,
	X11_CONNECTION_SETUP_SIZE   = 12,
	X11_SETUP_REPLY_HEADER_SIZE = 8,
	X11_SETUP_MIN_SIZE	    = 32,
	X11_FORMAT_SIZE		    = 8,
	X11_SCREEN_MIN_SIZE	    = 40,
	X11_REPLY_SIZE		    = 32,
	X11_EVENT_SIZE		    = 32,
	X11_EVENT_TYPE_MASK	    = 0x7f,
	X11_SOCKET_NONBLOCK	    = 04000,
	X11_EVENT_IGNORED	    = 2,
	X11_EVENT_NONE		    = 3,
};

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
	X_CW_BACK_PIXEL	  = 1u << 1,
	X_CW_BORDER_PIXEL = 1u << 3,
	X_CW_EVENT_MASK	  = 1u << 11,
	X_CW_COLORMAP	  = 1u << 13,
};

enum {
	X_EVENT_MASK_KEY_PRESS		   = 1u << 0,
	X_EVENT_MASK_KEY_RELEASE	   = 1u << 1,
	X_EVENT_MASK_BUTTON_PRESS	   = 1u << 2,
	X_EVENT_MASK_BUTTON_RELEASE	   = 1u << 3,
	X_EVENT_MASK_POINTER_MOTION	   = 1u << 6,
	X_EVENT_MASK_EXPOSURE		   = 1u << 15,
	X_EVENT_MASK_STRUCTURE		   = 1u << 17,
	X_EVENT_MASK_SUBSTRUCTURE_NOTIFY   = 1u << 19,
	X_EVENT_MASK_SUBSTRUCTURE_REDIRECT = 1u << 20,
	X_EVENT_MASK_FOCUS_CHANGE	   = 1u << 21,
};

enum {
	X_EVENT_KEY_PRESS	 = 2,
	X_EVENT_KEY_RELEASE	 = 3,
	X_EVENT_BUTTON_PRESS	 = 4,
	X_EVENT_BUTTON_RELEASE	 = 5,
	X_EVENT_MOTION_NOTIFY	 = 6,
	X_EVENT_FOCUS_IN	 = 9,
	X_EVENT_FOCUS_OUT	 = 10,
	X_EVENT_EXPOSE		 = 12,
	X_EVENT_DESTROY_NOTIFY	 = 17,
	X_EVENT_UNMAP_NOTIFY	 = 18,
	X_EVENT_MAP_NOTIFY	 = 19,
	X_EVENT_REPARENT_NOTIFY	 = 21,
	X_EVENT_CONFIGURE_NOTIFY = 22,
	X_EVENT_CLIENT_MESSAGE	 = 33,
	X_EVENT_MAPPING_NOTIFY	 = 34,
};

enum {
	X_KEY_PRESS	   = X_EVENT_KEY_PRESS,
	X_KEY_RELEASE	   = X_EVENT_KEY_RELEASE,
	X_BUTTON_PRESS	   = X_EVENT_BUTTON_PRESS,
	X_BUTTON_RELEASE   = X_EVENT_BUTTON_RELEASE,
	X_MOTION_NOTIFY	   = X_EVENT_MOTION_NOTIFY,
	X_FOCUS_IN	   = X_EVENT_FOCUS_IN,
	X_FOCUS_OUT	   = X_EVENT_FOCUS_OUT,
	X_EXPOSE	   = X_EVENT_EXPOSE,
	X_DESTROY_NOTIFY   = X_EVENT_DESTROY_NOTIFY,
	X_UNMAP_NOTIFY	   = X_EVENT_UNMAP_NOTIFY,
	X_MAP_NOTIFY	   = X_EVENT_MAP_NOTIFY,
	X_REPARENT_NOTIFY  = X_EVENT_REPARENT_NOTIFY,
	X_CONFIGURE_NOTIFY = X_EVENT_CONFIGURE_NOTIFY,
	X_CLIENT_MESSAGE   = X_EVENT_CLIENT_MESSAGE,
	X_MAPPING_NOTIFY   = X_EVENT_MAPPING_NOTIFY,
	X_EVENT_IGNORED	   = X11_EVENT_IGNORED,
};

enum {
	XA_ATOM		   = 4,
	XA_WM_NORMAL_HINTS = 40,
	XA_WM_SIZE_HINTS   = 41,
	XA_STRING	   = 31,
	X_SUCCESS	   = 0,
};

enum {
	MOTIF_WM_HINTS_FIELD_COUNT	= 5,
	MOTIF_WM_HINTS_DECORATIONS_FLAG = 1u << 1,
	MOTIF_WM_DECOR_ALL		= 1,
};

enum {
	X_SIZE_HINT_US_POSITION = 1 << 0,
	X_SIZE_HINT_US_SIZE	= 1 << 1,
	X_SIZE_HINT_P_POSITION	= 1 << 2,
	X_SIZE_HINT_P_SIZE	= 1 << 3,
	X_SIZE_HINT_FIELD_COUNT = 18,
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

size_t x11_pad4(size_t length);
s32 x11_s16(u16 value);
display_key_t x11_key_from_keysym(u32 keysym);
display_modifier_t x11_modifier_from_key(display_key_t key);
display_mouse_t x11_mouse_from_button(u32 button, const char *tag);
display_modifier_t x11_modifiers_from_state(const display_modifier_t modifiers[static X_MODIFIER_COUNT], u32 state);
void x11_monitor_set(display_monitor_t *monitor, u32 id, s32 x, s32 y, u32 width, u32 height, u32 physical_width, u32 physical_height,
		     int primary, void *native);

#endif
