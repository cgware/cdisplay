#include "display_driver.h"
#include "display_x11_priv.h"

#include "arr.h"
#include "log.h"
#include "mem.h"

#include <limits.h>

typedef struct x11_s {
	Display *(*OpenDisplay)(const char *);
	int (*CloseDisplay)(Display *);
	int (*DefaultScreen)(Display *);
	int (*ScreenCount)(Display *);
	Window (*RootWindow)(Display *, int);
	int (*DisplayWidth)(Display *, int);
	int (*DisplayHeight)(Display *, int);
	int (*DisplayWidthMM)(Display *, int);
	int (*DisplayHeightMM)(Display *, int);
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
	Status (*FetchName)(Display *, Window, char **);
	int (*Pending)(Display *);
	int (*NextEvent)(Display *, XEvent *);
	int (*Flush)(Display *);
	int (*Sync)(Display *, Bool);
	KeySym (*LookupKeysym)(XKeyEvent *, int);
	void (*DisplayKeycodes)(Display *, int *, int *);
	KeySym *(*GetKeyboardMapping)(Display *, KeyCode, int, int *);
	XModifierKeymap *(*GetModifierMapping)(Display *);
	int (*FreeModifiermap)(XModifierKeymap *);
	char *(*GetAtomName)(Display *, Atom);
	int (*GetWindowAttributes)(Display *, Window, XWindowAttributes *);
	int (*GetWindowProperty)(Display *, Window, Atom, long, long, Bool, Atom, Atom *, int *, unsigned long *, unsigned long *,
				 unsigned char **);
	XVisualInfo *(*GetVisualInfo)(Display *, long, XVisualInfo *, int *);
	XID (*AllocID)(Display *);
	int (*QueryExtension)(Display *, const char *, int *, int *, int *);
	int (*Free)(void *);
} x11_t;

typedef struct xrandr_s {
	XRRMonitorInfo *(*GetMonitors)(Display *, Window, Bool, int *);
	void (*FreeMonitors)(XRRMonitorInfo *);
} xrandr_t;

typedef struct display_x11_dynamic_s {
	proc_t *proc;
	void *lib;
	void *xrandr_lib;
	x11_t x11;
	xrandr_t xrandr;
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
	arr_t windows;
} display_x11_dynamic_t;

typedef struct window_x11_dynamic_s {
	Window id;
	Colormap colormap;
	int mapped;
} window_x11_dynamic_t;

typedef struct window_x11_dynamic_slot_s {
	int used;
	window_x11_dynamic_t window;
} window_x11_dynamic_slot_t;

static window_x11_dynamic_t *display_x11_dynamic_window_data(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return NULL;
	}

	display_x11_dynamic_t *dx11	= wnd->display->data;
	uint id				= (uint)(uintptr_t)wnd->data - 1;
	window_x11_dynamic_slot_t *slot = arr_get(&dx11->windows, id);
	if (slot == NULL || !slot->used) {
		return NULL;
	}

	return &slot->window;
}

static window_x11_dynamic_t *display_x11_dynamic_window_alloc(window_t *wnd)
{
	display_x11_dynamic_t *dx11	= wnd->display->data;
	window_x11_dynamic_slot_t *slot = NULL;
	uint id				= 0;

	for (; id < dx11->windows.cnt; ++id) {
		slot = arr_get(&dx11->windows, id);
		if (slot != NULL && !slot->used) {
			break;
		}
	}

	if (id == dx11->windows.cnt) {
		slot = arr_add(&dx11->windows, &id);
		if (slot == NULL) {
			return NULL;
		}
	}

	mem_set(slot, 0, sizeof(*slot));
	slot->used = 1;
	wnd->data  = (void *)(uintptr_t)(id + 1);

	return &slot->window;
}

static void display_x11_dynamic_window_release(window_t *wnd)
{
	display_x11_dynamic_t *dx11	= wnd->display->data;
	uint id				= (uint)(uintptr_t)wnd->data - 1;
	window_x11_dynamic_slot_t *slot = arr_get(&dx11->windows, id);
	if (slot != NULL) {
		mem_set(slot, 0, sizeof(*slot));
	}
	wnd->data = NULL;
}

enum {
	X_ALLOC_NONE = 0,
};

enum {
	X_PROP_MODE_REPLACE = 0,
};

enum {
	X_VISUAL_ID_MASK     = 0x1,
	X_VISUAL_SCREEN_MASK = 0x2,
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
	LOAD_X11(dx11, FetchName);
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
	LOAD_X11(dx11, GetWindowProperty);
	LOAD_X11(dx11, GetVisualInfo);
	if (load_symbol(dx11, (void **)&dx11->x11.AllocID, STRV("_XAllocID"))) {
		return 1;
	}
	LOAD_X11(dx11, QueryExtension);
	LOAD_X11(dx11, Free);

	proc_dlsym(dx11->proc, dx11->lib, STRV("XScreenCount"), (void **)&dx11->x11.ScreenCount);
	proc_dlsym(dx11->proc, dx11->lib, STRV("XDisplayWidth"), (void **)&dx11->x11.DisplayWidth);
	proc_dlsym(dx11->proc, dx11->lib, STRV("XDisplayHeight"), (void **)&dx11->x11.DisplayHeight);
	proc_dlsym(dx11->proc, dx11->lib, STRV("XDisplayWidthMM"), (void **)&dx11->x11.DisplayWidthMM);
	proc_dlsym(dx11->proc, dx11->lib, STRV("XDisplayHeightMM"), (void **)&dx11->x11.DisplayHeightMM);
	proc_dlsym(dx11->proc, dx11->lib, STRV("XGetAtomName"), (void **)&dx11->x11.GetAtomName);

	return 0;
}

#undef LOAD_X11

static void load_xrandr(display_x11_dynamic_t *dx11)
{
	if (proc_dlopen(dx11->proc, STRV("libXrandr.so.2"), &dx11->xrandr_lib) &&
	    proc_dlopen(dx11->proc, STRV("libXrandr.so"), &dx11->xrandr_lib)) {
		return;
	}

	if (proc_dlsym(dx11->proc, dx11->xrandr_lib, STRV("XRRGetMonitors"), (void **)&dx11->xrandr.GetMonitors) ||
	    proc_dlsym(dx11->proc, dx11->xrandr_lib, STRV("XRRFreeMonitors"), (void **)&dx11->xrandr.FreeMonitors)) {
		proc_dlclose(dx11->proc, dx11->xrandr_lib);
		dx11->xrandr_lib = NULL;
		dx11->xrandr	 = (xrandr_t){0};
	}
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
			display_key_t key = x11_key_from_keysym((u32)keysyms[i * keysyms_per_keycode + j]);
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
			dx11->modifiers[i] = (display_modifier_t)(dx11->modifiers[i] | x11_modifier_from_key(dx11->keys[keycode]));
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
	window_x11_dynamic_t *wx11  = display_x11_dynamic_window_data(wnd);

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
	window_x11_dynamic_t *wx11  = display_x11_dynamic_window_data(wnd);

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
	window_x11_dynamic_t *wx11  = display_x11_dynamic_window_data(wnd);
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
	window_x11_dynamic_t *wx11  = display_x11_dynamic_window_data(wnd);
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
	window_x11_dynamic_t *wx11  = display_x11_dynamic_window_data(wnd);

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
	window_x11_dynamic_t *wx11  = display_x11_dynamic_window_data(wnd);

	if (dx11->x11.ChangeProperty(
		    dx11->display, wx11->id, property, type, 32, X_PROP_MODE_REPLACE, (const unsigned char *)values, count) == 0) {
		log_error("cdisplay", "display_x11_dynamic", NULL, "failed to set window property");
		return 1;
	}

	return dx11->x11.Flush(dx11->display) == 0 ? 1 : 0;
}

static int set_wm_normal_hints(window_t *wnd, const window_config_t *config)
{
	long hints[X_SIZE_HINT_FIELD_COUNT] = {
		X_SIZE_HINT_US_POSITION | X_SIZE_HINT_US_SIZE | X_SIZE_HINT_P_POSITION | X_SIZE_HINT_P_SIZE,
		config->x,
		config->y,
		config->width,
		config->height,
	};

	return set_property_long(wnd, XA_WM_NORMAL_HINTS, XA_WM_SIZE_HINTS, hints, X_SIZE_HINT_FIELD_COUNT);
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
	window_x11_dynamic_t *wx11  = display_x11_dynamic_window_data(wnd);
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
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);

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
		event->modifiers = x11_modifiers_from_state(dx11->modifiers, xev.xkey.state);
		event->type	 = xev.type == X_KEY_PRESS ? DISPLAY_EVENT_KEY_DOWN : DISPLAY_EVENT_KEY_UP;
		event->key	 = x11_key_from_keysym((u32)dx11->x11.LookupKeysym(&xev.xkey, 0));
		return 0;
	}
	case X_BUTTON_PRESS:
	case X_BUTTON_RELEASE: {
		event->window	 = (u32)xev.xbutton.window;
		event->x	 = (u16)xev.xbutton.x;
		event->y	 = (u16)xev.xbutton.y;
		event->modifiers = x11_modifiers_from_state(dx11->modifiers, xev.xbutton.state);
		event->type	 = xev.type == X_BUTTON_PRESS ? DISPLAY_EVENT_MOUSE_DOWN : DISPLAY_EVENT_MOUSE_UP;
		event->button	 = x11_mouse_from_button(xev.xbutton.keycode, "display_x11_dynamic");
		return 0;
	}
	case X_MOTION_NOTIFY: {
		event->window	 = (u32)xev.xmotion.window;
		event->x	 = (u16)xev.xmotion.x;
		event->y	 = (u16)xev.xmotion.y;
		event->modifiers = x11_modifiers_from_state(dx11->modifiers, xev.xmotion.state);
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
	if (arr_init(&dx11->windows, 8, sizeof(window_x11_dynamic_slot_t), display->alloc) == NULL || load_x11(dx11)) {
		if (dx11->lib != NULL) {
			proc_dlclose(dx11->proc, dx11->lib);
		}
		arr_free(&dx11->windows);
		alloc_free(&display->alloc, display->data, sizeof(display_x11_dynamic_t));
		display->data = NULL;
		return 1;
	}
	load_xrandr(dx11);
	if (open_display(display) || init_keys(display) || init_modifiers(display) || init_atoms(display)) {
		if (dx11->display != NULL) {
			dx11->x11.CloseDisplay(dx11->display);
		}
		if (dx11->xrandr_lib != NULL) {
			proc_dlclose(dx11->proc, dx11->xrandr_lib);
		}
		if (dx11->lib != NULL) {
			proc_dlclose(dx11->proc, dx11->lib);
		}
		arr_free(&dx11->windows);
		alloc_free(&display->alloc, display->data, sizeof(display_x11_dynamic_t));
		display->data = NULL;
		return 1;
	}

	return 0;
}

static int display_x11_dynamic_available(display_driver_t *driver, proc_t *proc)
{
	(void)driver;
	return proc != NULL && proc_getenv(proc, STRV("DISPLAY")).data != NULL;
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
	if (dx11->xrandr_lib != NULL) {
		proc_dlclose(dx11->proc, dx11->xrandr_lib);
	}

	arr_free(&dx11->windows);
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

static void display_x11_dynamic_monitor_name(display_x11_dynamic_t *dx11, display_monitor_t *monitor, Atom atom)
{
	if (dx11->x11.GetAtomName == NULL) {
		return;
	}

	char *name = atom == X_NONE ? NULL : dx11->x11.GetAtomName(dx11->display, atom);
	size_t len = 0;

	if (name == NULL) {
		return;
	}
	while (len + 1 < sizeof(monitor->name) && name[len] != 0) {
		len++;
	}
	if (len > 0) {
		mem_copy(monitor->name, sizeof(monitor->name), name, len);
	}
	monitor->name[len] = 0;
	dx11->x11.Free(name);
}

static int display_x11_dynamic_monitors(display_t *display, arr_t *monitors)
{
	if (display == NULL || display->data == NULL || monitors == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = display->data;
	if (dx11->xrandr.GetMonitors != NULL) {
		int count		  = 0;
		XRRMonitorInfo *xmonitors = dx11->xrandr.GetMonitors(dx11->display, dx11->root, X_TRUE, &count);
		if (xmonitors != NULL) {
			if (arr_resize(monitors, count > 0 ? (u32)count : 0)) {
				dx11->xrandr.FreeMonitors(xmonitors);
				return 1;
			}
			monitors->cnt = count > 0 ? (u32)count : 0;
			for (int i = 0; i < count; ++i) {
				display_monitor_t *monitor = arr_get(monitors, (u32)i);
				x11_monitor_set(monitor,
						(u32)i,
						xmonitors[i].x,
						xmonitors[i].y,
						xmonitors[i].width > 0 ? (u32)xmonitors[i].width : 0,
						xmonitors[i].height > 0 ? (u32)xmonitors[i].height : 0,
						xmonitors[i].mwidth > 0 ? (u32)xmonitors[i].mwidth : 0,
						xmonitors[i].mheight > 0 ? (u32)xmonitors[i].mheight : 0,
						xmonitors[i].primary != 0,
						xmonitors[i].noutput > 0 ? (void *)(uintptr_t)xmonitors[i].outputs[0] : NULL);
				display_x11_dynamic_monitor_name(dx11, monitor, xmonitors[i].name);
			}
			dx11->xrandr.FreeMonitors(xmonitors);
			return 0;
		}
	}

	if (dx11->x11.ScreenCount == NULL || dx11->x11.DisplayWidth == NULL || dx11->x11.DisplayHeight == NULL ||
	    dx11->x11.DisplayWidthMM == NULL || dx11->x11.DisplayHeightMM == NULL) {
		monitors->cnt = 0;
		return 0;
	}

	int screens = dx11->x11.ScreenCount(dx11->display);
	if (arr_resize(monitors, screens > 0 ? (u32)screens : 0)) {
		return 1;
	}
	monitors->cnt = screens > 0 ? (u32)screens : 0;
	for (int i = 0; i < screens; ++i) {
		display_monitor_t *monitor = arr_get(monitors, (u32)i);
		x11_monitor_set(monitor,
				(u32)i,
				0,
				0,
				(u32)dx11->x11.DisplayWidth(dx11->display, i),
				(u32)dx11->x11.DisplayHeight(dx11->display, i),
				(u32)dx11->x11.DisplayWidthMM(dx11->display, i),
				(u32)dx11->x11.DisplayHeightMM(dx11->display, i),
				i == dx11->screen,
				(void *)(uintptr_t)dx11->x11.RootWindow(dx11->display, i));
	}

	return 0;
}

static int display_x11_dynamic_window_init(window_t *wnd, const window_config_t *config)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->display->alloc.alloc == NULL || config == NULL) {
		return 1;
	}

	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_alloc(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	if (create_window(wnd, config) || set_wm_normal_hints(wnd, config) || set_wm_protocols(wnd)) {
		display_x11_dynamic_t *dx11 = wnd->display->data;
		if (wx11->id != X_NONE) {
			dx11->x11.DestroyWindow(dx11->display, wx11->id);
		}
		free_colormap(wnd);
		display_x11_dynamic_window_release(wnd);
		return 1;
	}

	return 0;
}

static int display_x11_dynamic_window_free(window_t *wnd)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;

	if (wx11->id != X_NONE) {
		dx11->x11.DestroyWindow(dx11->display, wx11->id);
	}
	free_colormap(wnd);
	dx11->x11.Flush(dx11->display);

	display_x11_dynamic_window_release(wnd);
	return 0;
}

static u32 display_x11_dynamic_window_id(window_t *wnd)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL) {
		return 0;
	}

	return (u32)wx11->id;
}

static int display_x11_dynamic_window_native(window_t *wnd, window_native_t *native)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL || native == NULL) {
		return 1;
	}

	native->type   = DISPLAY_NATIVE_X11;
	native->window = (void *)(uintptr_t)wx11->id;
	return wx11->id == X_NONE;
}

static int display_x11_dynamic_window_set_title(window_t *wnd, strv_t title)
{
	if (display_x11_dynamic_window_data(wnd) == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	return set_property_text(wnd, dx11->wm_name, XA_STRING, title) ||
	       set_property_text(wnd, dx11->net_wm_name, dx11->utf8_string, title);
}

static int display_x11_dynamic_window_get_title(window_t *wnd, char *title, size_t size)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL || title == NULL || size == 0) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	char *name		    = NULL;
	if (dx11->x11.FetchName(dx11->display, wx11->id, &name) == 0 || name == NULL) {
		return 1;
	}

	size_t len = 0;
	while (name[len] != 0) {
		len++;
	}
	if (len >= size) {
		dx11->x11.Free(name);
		return 1;
	}

	mem_copy(title, size, name, len + 1);
	dx11->x11.Free(name);
	return 0;
}

static int display_x11_dynamic_window_set_position(window_t *wnd, u16 x, u16 y)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	return dx11->x11.MoveWindow(dx11->display, wx11->id, x, y) == 0 || dx11->x11.Flush(dx11->display) == 0;
}

static int display_x11_dynamic_window_get_position(window_t *wnd, u16 *x, u16 *y)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL || x == NULL || y == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	XWindowAttributes attrs	    = {0};
	if (dx11->x11.GetWindowAttributes(dx11->display, wx11->id, &attrs) == 0 || attrs.x < 0 || attrs.y < 0 || attrs.x > UINT16_MAX ||
	    attrs.y > UINT16_MAX) {
		return 1;
	}

	*x = (u16)attrs.x;
	*y = (u16)attrs.y;
	return 0;
}

static int display_x11_dynamic_window_set_size(window_t *wnd, u16 width, u16 height)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	return dx11->x11.ResizeWindow(dx11->display, wx11->id, width, height) == 0 || dx11->x11.Flush(dx11->display) == 0;
}

static int display_x11_dynamic_window_get_size(window_t *wnd, u16 *width, u16 *height)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL || width == NULL || height == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	XWindowAttributes attrs	    = {0};
	if (dx11->x11.GetWindowAttributes(dx11->display, wx11->id, &attrs) == 0 || attrs.width < 0 || attrs.height < 0 ||
	    attrs.width > UINT16_MAX || attrs.height > UINT16_MAX) {
		return 1;
	}

	*width	= (u16)attrs.width;
	*height = (u16)attrs.height;
	return 0;
}

static int display_x11_dynamic_window_set_borderless(window_t *wnd, int borderless)
{
	if (display_x11_dynamic_window_data(wnd) == NULL) {
		return 1;
	}

	return set_borderless(wnd, borderless);
}

static int display_x11_dynamic_window_get_borderless(window_t *wnd, int *borderless)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL || borderless == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	Atom actual_type	    = 0;
	int actual_format	    = 0;
	unsigned long items	    = 0;
	unsigned long after	    = 0;
	unsigned char *data	    = NULL;
	if (dx11->x11.GetWindowProperty(dx11->display,
					wx11->id,
					dx11->motif_wm_hints,
					0,
					MOTIF_WM_HINTS_FIELD_COUNT,
					X_FALSE,
					dx11->motif_wm_hints,
					&actual_type,
					&actual_format,
					&items,
					&after,
					&data) != X_SUCCESS ||
	    actual_type != dx11->motif_wm_hints || actual_format != 32 || items < MOTIF_WM_HINTS_FIELD_COUNT || data == NULL) {
		if (data != NULL) {
			dx11->x11.Free(data);
		}
		return 1;
	}

	long *hints = (long *)data;
	*borderless = (hints[0] & MOTIF_WM_HINTS_DECORATIONS_FLAG) != 0 && hints[2] == 0;
	dx11->x11.Free(data);
	return after == 0 ? 0 : 1;
}

static int display_x11_dynamic_window_set_fullscreen(window_t *wnd, int fullscreen)
{
	if (display_x11_dynamic_window_data(wnd) == NULL) {
		return 1;
	}

	return set_fullscreen(wnd, fullscreen);
}

static int display_x11_dynamic_window_get_fullscreen(window_t *wnd, int *fullscreen)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL || fullscreen == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	Atom actual_type	    = 0;
	int actual_format	    = 0;
	unsigned long items	    = 0;
	unsigned long after	    = 0;
	unsigned char *data	    = NULL;
	if (dx11->x11.GetWindowProperty(dx11->display,
					wx11->id,
					dx11->net_wm_state,
					0,
					32,
					X_FALSE,
					XA_ATOM,
					&actual_type,
					&actual_format,
					&items,
					&after,
					&data) != X_SUCCESS ||
	    actual_type != XA_ATOM || actual_format != 32) {
		if (data != NULL) {
			dx11->x11.Free(data);
		}
		return 1;
	}

	Atom *states = (Atom *)data;
	*fullscreen  = 0;
	for (unsigned long i = 0; i < items; i++) {
		if (states[i] == dx11->net_wm_state_fullscreen) {
			*fullscreen = 1;
			break;
		}
	}
	if (data != NULL) {
		dx11->x11.Free(data);
	}
	return after == 0 ? 0 : 1;
}

static int display_x11_dynamic_window_show(window_t *wnd)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
	if (dx11->x11.MapWindow(dx11->display, wx11->id) == 0 || dx11->x11.Flush(dx11->display) == 0) {
		return 1;
	}

	wx11->mapped = 1;
	return 0;
}

static int display_x11_dynamic_window_hide(window_t *wnd)
{
	window_x11_dynamic_t *wx11 = display_x11_dynamic_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	display_x11_dynamic_t *dx11 = wnd->display->data;
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
	.available	       = display_x11_dynamic_available,
	.init		       = display_x11_dynamic_init,
	.free		       = display_x11_dynamic_free,
	.poll_events	       = display_x11_dynamic_poll_events,
	.wait_events	       = display_x11_dynamic_wait_events,
	.native		       = display_x11_dynamic_native,
	.native_free	       = display_x11_dynamic_native_free,
	.monitors	       = display_x11_dynamic_monitors,
	.window_init	       = display_x11_dynamic_window_init,
	.window_free	       = display_x11_dynamic_window_free,
	.window_id	       = display_x11_dynamic_window_id,
	.window_native	       = display_x11_dynamic_window_native,
	.window_set_title      = display_x11_dynamic_window_set_title,
	.window_get_title      = display_x11_dynamic_window_get_title,
	.window_set_position   = display_x11_dynamic_window_set_position,
	.window_get_position   = display_x11_dynamic_window_get_position,
	.window_set_size       = display_x11_dynamic_window_set_size,
	.window_get_size       = display_x11_dynamic_window_get_size,
	.window_set_borderless = display_x11_dynamic_window_set_borderless,
	.window_get_borderless = display_x11_dynamic_window_get_borderless,
	.window_set_fullscreen = display_x11_dynamic_window_set_fullscreen,
	.window_get_fullscreen = display_x11_dynamic_window_get_fullscreen,
	.window_show	       = display_x11_dynamic_window_show,
	.window_hide	       = display_x11_dynamic_window_hide,
	.ext_init	       = display_x11_dynamic_ext_init,
	.alloc_id	       = display_x11_dynamic_alloc_id,
	.visual_depth	       = display_x11_dynamic_visual_depth,
};

DISPLAY_DRIVER(display_x11_dynamic, &display_x11_dynamic);
