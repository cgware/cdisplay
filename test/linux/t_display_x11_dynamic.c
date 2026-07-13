#include "test.h"

#include "display_driver.h"
#include "fs.h"
#include "mem.h"
#include "proc.h"
#include "sock.h"

#include <limits.h>

typedef unsigned long t_x11_window_t;
typedef unsigned long t_x11_atom_t;
typedef unsigned long t_x11_keysym_t;
typedef unsigned char t_x11_keycode_t;
typedef void (*t_x11_symbol_t)(void);

typedef struct t_visual_s {
	void *ext_data;
	unsigned long visualid;
	int class;
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int bits_per_rgb;
	int map_entries;
} t_visual_t;

typedef struct t_visual_info_s {
	t_visual_t *visual;
	unsigned long visualid;
	int screen;
	int depth;
	int class;
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int colormap_size;
	int bits_per_rgb;
} t_visual_info_t;

typedef struct t_x11_key_event_s {
	int type;
	unsigned long serial;
	int send_event;
	void *display;
	t_x11_window_t window;
	t_x11_window_t root;
	t_x11_window_t subwindow;
	unsigned long time;
	int x;
	int y;
	int x_root;
	int y_root;
	unsigned int state;
	unsigned int keycode;
	int same_screen;
} t_x11_key_event_t;

typedef t_x11_key_event_t t_x11_button_event_t;
typedef t_x11_key_event_t t_x11_motion_event_t;

typedef struct t_x11_focus_event_s {
	int type;
	unsigned long serial;
	int send_event;
	void *display;
	t_x11_window_t window;
	int mode;
	int detail;
} t_x11_focus_event_t;

typedef struct t_x11_destroy_event_s {
	int type;
	unsigned long serial;
	int send_event;
	void *display;
	t_x11_window_t event;
	t_x11_window_t window;
} t_x11_destroy_event_t;

typedef struct t_x11_configure_event_s {
	int type;
	unsigned long serial;
	int send_event;
	void *display;
	t_x11_window_t event;
	t_x11_window_t window;
	int x;
	int y;
	int width;
	int height;
	int border_width;
	t_x11_window_t above;
	int override_redirect;
} t_x11_configure_event_t;

typedef union t_x11_client_message_data_u {
	char b[20];
	short s[10];
	long l[5];
} t_x11_client_message_data_t;

typedef struct t_x11_client_message_event_s {
	int type;
	unsigned long serial;
	int send_event;
	void *display;
	t_x11_window_t window;
	t_x11_atom_t message_type;
	int format;
	t_x11_client_message_data_t data;
} t_x11_client_message_event_t;

typedef union t_x11_event_u {
	int type;
	t_x11_key_event_t xkey;
	t_x11_button_event_t xbutton;
	t_x11_motion_event_t xmotion;
	t_x11_focus_event_t xfocus;
	t_x11_destroy_event_t xdestroywindow;
	t_x11_configure_event_t xconfigure;
	t_x11_client_message_event_t xclient;
	long pad[24];
} t_x11_event_t;

enum {
	T_X11_KEY_PRESS	  = 2,
	T_X11_KEY_RELEASE = 3,
	T_X11_BUTTON_PRESS = 4,
	T_X11_BUTTON_RELEASE = 5,
	T_X11_MOTION_NOTIFY = 6,
	T_X11_FOCUS_IN	  = 9,
	T_X11_FOCUS_OUT	  = 10,
	T_X11_EXPOSE	  = 12,
	T_X11_DESTROY_NOTIFY = 17,
	T_X11_CONFIGURE_NOTIFY = 22,
	T_X11_CLIENT_MESSAGE = 33,
};

typedef struct t_x11_modifier_keymap_s {
	int max_keypermod;
	t_x11_keycode_t *modifiermap;
} t_x11_modifier_keymap_t;

typedef struct t_x11_state_s {
	int open_display_calls;
	int close_display_calls;
	int free_calls;
	int destroy_window_calls;
	int create_colormap_calls;
	int free_colormap_calls;
	int change_property_calls;
	int send_event_calls;
	int set_wm_protocols_calls;
	int map_window_calls;
	int unmap_window_calls;
	int move_window_calls;
	int resize_window_calls;
	int query_extension_calls;
	int alloc_id_calls;
	int get_visual_info_calls;
	const char *display_name;
	void *display_result;
	int min_keycode;
	int max_keycode;
	int keysyms_per_keycode;
	int keyboard_mapping_null;
	int modifier_mapping_null;
	int intern_atom_zero_after;
	int intern_atom_calls;
	t_x11_keysym_t lookup_keysym;
	t_x11_keysym_t keysyms[256];
	t_x11_keycode_t modifiermap[16];
	t_x11_modifier_keymap_t modifiers;
	t_visual_t visual;
	t_visual_info_t visual_info;
	t_visual_info_t *visual_info_result;
	int visual_info_count;
	unsigned long create_colormap_result;
	int free_colormap_result;
	t_x11_window_t create_window_result;
	int set_wm_protocols_result;
	int change_property_result;
	int send_event_result;
	int map_window_result;
	int unmap_window_result;
	int move_window_result;
	int resize_window_result;
	int flush_result;
	int query_extension_result;
	unsigned long alloc_id_result;
	t_x11_event_t events[16];
	int event_count;
	int event_index;
	int pending[16];
	int pending_count;
	int pending_index;
} t_x11_state_t;

static t_x11_state_t t_x11;

static void t_x11_reset(void)
{
	t_x11 = (t_x11_state_t){
		.display_result	       = (void *)0x11,
		.min_keycode	       = 8,
		.max_keycode	       = 8,
		.keysyms_per_keycode   = 1,
		.lookup_keysym	       = 'a',
		.keysyms	       = {'a'},
		.modifiers	       = {.max_keypermod = 0, .modifiermap = t_x11.modifiermap},
		.visual		       = {.visualid = 0x21},
		.visual_info	       = {.visual = &t_x11.visual, .visualid = 0x21, .depth = 24},
		.visual_info_result    = NULL,
		.create_colormap_result = 0x55,
		.free_colormap_result  = 1,
		.create_window_result  = 0x44,
		.set_wm_protocols_result = 1,
		.change_property_result = 1,
		.send_event_result     = 1,
		.map_window_result     = 1,
		.unmap_window_result   = 1,
		.move_window_result    = 1,
		.resize_window_result  = 1,
		.flush_result	       = 1,
		.query_extension_result = 1,
		.alloc_id_result       = 0x66,
	};
}

static display_driver_t *t_x11_dynamic_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != DISPLAY_DRIVER_TYPE) {
			continue;
		}

		display_driver_t *drv = i->data;
		if (strv_eq(strv_cstr(drv->name), STRV("X11-dynamic"))) {
			return drv;
		}
	}

	return NULL;
}

static void *t_x11_symbol(t_x11_symbol_t fn)
{
	union {
		t_x11_symbol_t fn;
		void *ptr;
	} symbol = {.fn = fn};

	return symbol.ptr;
}

static void *t_XOpenDisplay(const char *name)
{
	t_x11.open_display_calls++;
	t_x11.display_name = name;
	return t_x11.display_result;
}

static int t_XCloseDisplay(void *display)
{
	(void)display;
	t_x11.close_display_calls++;
	return 0;
}

static int t_XDefaultScreen(void *display)
{
	(void)display;
	return 0;
}

static t_x11_window_t t_XRootWindow(void *display, int screen)
{
	(void)display;
	(void)screen;
	return 0x22;
}

static unsigned long t_XWhitePixel(void *display, int screen)
{
	(void)display;
	(void)screen;
	return 0xffffff;
}

static unsigned long t_XBlackPixel(void *display, int screen)
{
	(void)display;
	(void)screen;
	return 0;
}

static void *t_XDefaultVisual(void *display, int screen)
{
	(void)display;
	(void)screen;
	return (void *)0x33;
}

static t_x11_atom_t t_XInternAtom(void *display, const char *name, int only_if_exists)
{
	(void)display;
	(void)name;
	(void)only_if_exists;
	t_x11.intern_atom_calls++;
	if (t_x11.intern_atom_zero_after != 0 && t_x11.intern_atom_calls >= t_x11.intern_atom_zero_after) {
		return 0;
	}
	return (t_x11_atom_t)(100 + t_x11.intern_atom_calls);
}

static t_x11_window_t t_XCreateWindow(void *display, t_x11_window_t parent, int x, int y, unsigned int width, unsigned int height,
				      unsigned int border_width, int depth, unsigned int class, void *visual, unsigned long value_mask,
				      void *attrs)
{
	(void)display;
	(void)parent;
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	(void)border_width;
	(void)depth;
	(void)class;
	(void)visual;
	(void)value_mask;
	(void)attrs;
	return t_x11.create_window_result;
}

static int t_XDestroyWindow(void *display, t_x11_window_t window)
{
	(void)display;
	(void)window;
	t_x11.destroy_window_calls++;
	return 1;
}

static unsigned long t_XCreateColormap(void *display, t_x11_window_t window, void *visual, int alloc)
{
	(void)display;
	(void)window;
	(void)visual;
	(void)alloc;
	t_x11.create_colormap_calls++;
	return t_x11.create_colormap_result;
}

static int t_XFreeColormap(void *display, unsigned long colormap)
{
	(void)display;
	(void)colormap;
	t_x11.free_colormap_calls++;
	return t_x11.free_colormap_result;
}

static int t_XChangeProperty(void *display, t_x11_window_t window, t_x11_atom_t property, t_x11_atom_t type, int format, int mode,
			     const unsigned char *data, int count)
{
	(void)display;
	(void)window;
	(void)property;
	(void)type;
	(void)format;
	(void)mode;
	(void)data;
	(void)count;
	t_x11.change_property_calls++;
	return t_x11.change_property_result;
}

static int t_XSendEvent(void *display, t_x11_window_t window, int propagate, long event_mask, void *event)
{
	(void)display;
	(void)window;
	(void)propagate;
	(void)event_mask;
	(void)event;
	t_x11.send_event_calls++;
	return t_x11.send_event_result;
}

static int t_XSetWMProtocols(void *display, t_x11_window_t window, t_x11_atom_t *protocols, int count)
{
	(void)display;
	(void)window;
	(void)protocols;
	(void)count;
	t_x11.set_wm_protocols_calls++;
	return t_x11.set_wm_protocols_result;
}

static int t_XMapWindow(void *display, t_x11_window_t window)
{
	(void)display;
	(void)window;
	t_x11.map_window_calls++;
	return t_x11.map_window_result;
}

static int t_XUnmapWindow(void *display, t_x11_window_t window)
{
	(void)display;
	(void)window;
	t_x11.unmap_window_calls++;
	return t_x11.unmap_window_result;
}

static int t_XMoveWindow(void *display, t_x11_window_t window, int x, int y)
{
	(void)display;
	(void)window;
	(void)x;
	(void)y;
	t_x11.move_window_calls++;
	return t_x11.move_window_result;
}

static int t_XResizeWindow(void *display, t_x11_window_t window, unsigned int width, unsigned int height)
{
	(void)display;
	(void)window;
	(void)width;
	(void)height;
	t_x11.resize_window_calls++;
	return t_x11.resize_window_result;
}

static int t_XPending(void *display)
{
	(void)display;
	if (t_x11.pending_index < t_x11.pending_count) {
		return t_x11.pending[t_x11.pending_index++];
	}
	return t_x11.event_index < t_x11.event_count;
}

static int t_XNextEvent(void *display, void *event)
{
	(void)display;
	if (t_x11.event_index < t_x11.event_count) {
		*(t_x11_event_t *)event = t_x11.events[t_x11.event_index++];
	}
	return 0;
}

static int t_XFlush(void *display)
{
	(void)display;
	return t_x11.flush_result;
}

static int t_XSync(void *display, int discard)
{
	(void)display;
	(void)discard;
	return 1;
}

static t_x11_keysym_t t_XLookupKeysym(void *event, int index)
{
	(void)event;
	(void)index;
	return t_x11.lookup_keysym;
}

static void t_XDisplayKeycodes(void *display, int *min_keycode, int *max_keycode)
{
	(void)display;
	*min_keycode = t_x11.min_keycode;
	*max_keycode = t_x11.max_keycode;
}

static t_x11_keysym_t *t_XGetKeyboardMapping(void *display, t_x11_keycode_t first_keycode, int keycode_count, int *keysyms_per_keycode)
{
	(void)display;
	(void)first_keycode;
	(void)keycode_count;
	*keysyms_per_keycode = t_x11.keysyms_per_keycode;
	if (t_x11.keyboard_mapping_null) {
		return NULL;
	}
	return t_x11.keysyms;
}

static t_x11_modifier_keymap_t *t_XGetModifierMapping(void *display)
{
	(void)display;
	if (t_x11.modifier_mapping_null) {
		return NULL;
	}
	return &t_x11.modifiers;
}

static int t_XFreeModifiermap(t_x11_modifier_keymap_t *map)
{
	(void)map;
	return 1;
}

static int t_XGetWindowAttributes(void *display, t_x11_window_t window, void *attrs)
{
	(void)display;
	(void)window;
	(void)attrs;
	return 1;
}

static void *t_XGetVisualInfo(void *display, long mask, void *template, int *count)
{
	(void)display;
	(void)mask;
	(void)template;
	t_x11.get_visual_info_calls++;
	*count = t_x11.visual_info_count;
	return t_x11.visual_info_result;
}

static unsigned long t_XAllocID(void *display)
{
	(void)display;
	t_x11.alloc_id_calls++;
	return t_x11.alloc_id_result;
}

static int t_XQueryExtension(void *display, const char *name, int *opcode, int *first_event, int *first_error)
{
	(void)display;
	(void)name;
	t_x11.query_extension_calls++;
	*opcode	     = 1;
	*first_event = 2;
	*first_error = 3;
	return t_x11.query_extension_result;
}

static int t_XFree(void *data)
{
	(void)data;
	t_x11.free_calls++;
	return 1;
}

static void t_x11_dynamic_set_symbols(proc_t *proc, int include_alloc_id, int include_free)
{
#define T_X11_SET(_name) proc_setdlsym(proc, STRV("libX11.so.6"), STRV("X" #_name), t_x11_symbol((t_x11_symbol_t)t_X##_name))

	T_X11_SET(OpenDisplay);
	T_X11_SET(CloseDisplay);
	T_X11_SET(DefaultScreen);
	T_X11_SET(RootWindow);
	T_X11_SET(WhitePixel);
	T_X11_SET(BlackPixel);
	T_X11_SET(DefaultVisual);
	T_X11_SET(InternAtom);
	T_X11_SET(CreateWindow);
	T_X11_SET(DestroyWindow);
	T_X11_SET(CreateColormap);
	T_X11_SET(FreeColormap);
	T_X11_SET(ChangeProperty);
	T_X11_SET(SendEvent);
	T_X11_SET(SetWMProtocols);
	T_X11_SET(MapWindow);
	T_X11_SET(UnmapWindow);
	T_X11_SET(MoveWindow);
	T_X11_SET(ResizeWindow);
	T_X11_SET(Pending);
	T_X11_SET(NextEvent);
	T_X11_SET(Flush);
	T_X11_SET(Sync);
	T_X11_SET(LookupKeysym);
	T_X11_SET(DisplayKeycodes);
	T_X11_SET(GetKeyboardMapping);
	T_X11_SET(GetModifierMapping);
	T_X11_SET(FreeModifiermap);
	T_X11_SET(GetWindowAttributes);
	T_X11_SET(GetVisualInfo);
	if (include_alloc_id) {
		proc_setdlsym(proc, STRV("libX11.so.6"), STRV("_XAllocID"), t_x11_symbol((t_x11_symbol_t)t_XAllocID));
	}
	T_X11_SET(QueryExtension);
	if (include_free) {
		T_X11_SET(Free);
	}

#undef T_X11_SET
}

static void t_x11_dynamic_env_init(fs_t *fs, proc_t *proc, sock_t *ss)
{
	fs_init(fs, 0, 1, ALLOC_STD);
	proc_init(proc, 256, 1, ALLOC_STD);
	sock_init(ss, 0, 1, ALLOC_STD);
	proc_setenv(proc, STRV("DISPLAY"), STRV(":0"), 1);
	t_x11_dynamic_set_symbols(proc, 1, 1);
}

static void t_x11_dynamic_env_free(fs_t *fs, proc_t *proc, sock_t *ss)
{
	fs_free(fs);
	proc_free(proc);
	sock_free(ss);
}

typedef struct t_alloc_s {
	int fail_alloc;
} t_alloc_t;

static void *t_alloc_alloc(alloc_t *alloc, size_t size)
{
	t_alloc_t *state = alloc->priv;
	if (state->fail_alloc) {
		return NULL;
	}
	return mem_alloc(size);
}

static int t_alloc_realloc(alloc_t *alloc, void **ptr, size_t *old_size, size_t new_size)
{
	(void)alloc;
	void *data = mem_realloc(*ptr, new_size, *old_size);
	if (data == NULL) {
		return 1;
	}
	*ptr	  = data;
	*old_size = new_size;
	return 0;
}

static void t_alloc_free(alloc_t *alloc, void *ptr, size_t size)
{
	(void)alloc;
	mem_free(ptr, size);
}

static alloc_t t_alloc(t_alloc_t *state)
{
	return (alloc_t){
		.alloc	 = t_alloc_alloc,
		.realloc = t_alloc_realloc,
		.free	 = t_alloc_free,
		.priv	 = state,
	};
}

static int t_x11_event_calls;
static display_event_t t_x11_event;

static void t_x11_event_cb(display_t *display, const display_event_t *event, void *user)
{
	(void)display;
	(void)user;
	t_x11_event_calls++;
	t_x11_event = *event;
}

static int t_x11_open(display_t *display, window_t *window, fs_t *fs, proc_t *proc, sock_t *ss)
{
	display_driver_t *drv = t_x11_dynamic_driver();
	if (drv == NULL) {
		return 1;
	}
	if (display_init(display, drv, fs, proc, ss, ALLOC_STD) == NULL) {
		return 1;
	}
	display_set_event_callback(display, t_x11_event_cb, NULL);
	if (window != NULL && window_init(window, display, &(window_config_t){.x = 10, .y = 20, .width = 640, .height = 480}) == NULL) {
		display_free(display);
		return 1;
	}
	return 0;
}

static void t_x11_push_event(t_x11_event_t event)
{
	t_x11.events[t_x11.event_count++] = event;
}

static void t_x11_push_pending(int pending)
{
	t_x11.pending[t_x11.pending_count++] = pending;
}

#define T_X11_DYNAMIC_DRV()                  \
	display_driver_t *drv = t_x11_dynamic_driver(); \
	EXPECT_NE(drv, NULL)

TEST(display_x11_dynamic_driver_is_registered)
{
	START;

	EXPECT_NE(t_x11_dynamic_driver(), NULL);

	END;
}

TEST(display_x11_dynamic_driver_has_display_hooks)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_NE(drv->init, NULL);
	EXPECT_NE(drv->free, NULL);
	EXPECT_NE(drv->poll_events, NULL);
	EXPECT_NE(drv->wait_events, NULL);
	EXPECT_NE(drv->native, NULL);
	EXPECT_NE(drv->native_free, NULL);

	END;
}

TEST(display_x11_dynamic_driver_has_window_hooks)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_NE(drv->window_init, NULL);
	EXPECT_NE(drv->window_free, NULL);
	EXPECT_NE(drv->window_id, NULL);
	EXPECT_NE(drv->window_native, NULL);
	EXPECT_NE(drv->window_set_title, NULL);
	EXPECT_NE(drv->window_set_position, NULL);
	EXPECT_NE(drv->window_set_size, NULL);
	EXPECT_NE(drv->window_set_borderless, NULL);
	EXPECT_NE(drv->window_set_fullscreen, NULL);
	EXPECT_NE(drv->window_show, NULL);
	EXPECT_NE(drv->window_hide, NULL);

	END;
}

TEST(display_x11_dynamic_driver_has_ext_init)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_NE(drv->ext_init, NULL);

	END;
}

TEST(display_x11_dynamic_driver_has_alloc_id)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_NE(drv->alloc_id, NULL);

	END;
}

TEST(display_x11_dynamic_driver_has_visual_depth)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_NE(drv->visual_depth, NULL);

	END;
}

TEST(display_x11_dynamic_init_null_display)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->init(NULL), 1);

	END;
}

TEST(display_x11_dynamic_init_null_alloc)
{
	START;

	T_X11_DYNAMIC_DRV();
	display_t display = {0};
	EXPECT_EQ(drv->init(&display), 1);

	END;
}

TEST(display_x11_dynamic_init_uses_virtual_proc)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_NE(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	EXPECT_EQ(t_x11.open_display_calls, 1);
	EXPECT_STR(t_x11.display_name, ":0");

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_free_closes_virtual_display)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	display_free(&display);

	EXPECT_EQ(t_x11.close_display_calls, 1);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_rejects_missing_virtual_symbol)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	fs_init(&fs, 0, 1, ALLOC_STD);
	proc_init(&proc, 256, 1, ALLOC_STD);
	sock_init(&ss, 0, 1, ALLOC_STD);
	proc_setenv(&proc, STRV("DISPLAY"), STRV(":0"), 1);
	t_x11_dynamic_set_symbols(&proc, 1, 0);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_rejects_missing_alloc_id_symbol)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	fs_init(&fs, 0, 1, ALLOC_STD);
	proc_init(&proc, 256, 1, ALLOC_STD);
	sock_init(&ss, 0, 1, ALLOC_STD);
	proc_setenv(&proc, STRV("DISPLAY"), STRV(":0"), 1);
	t_x11_dynamic_set_symbols(&proc, 0, 1);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_free_null_display)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->free(NULL), 1);

	END;
}

TEST(display_x11_dynamic_poll_events_null_display)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->poll_events(NULL), 1);

	END;
}

TEST(display_x11_dynamic_wait_events_null_display)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->wait_events(NULL), 1);

	END;
}

TEST(display_x11_dynamic_native_null_display)
{
	START;

	T_X11_DYNAMIC_DRV();
	display_native_t native = {0};

	EXPECT_EQ(drv->native(NULL, &native), 1);

	END;
}

TEST(display_x11_dynamic_native_null_native)
{
	START;

	T_X11_DYNAMIC_DRV();
	display_t display = {.data = (void *)0x1234};

	EXPECT_EQ(drv->native(&display, NULL), 1);

	END;
}

TEST(display_x11_dynamic_native_returns_display)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	display_native_t native = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(drv->native(&display, &native), 0);
	EXPECT_EQ(native.type, DISPLAY_NATIVE_X11);
	EXPECT_EQ(native.display, (void *)0x11);
	EXPECT_EQ(native.screen, 0);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_native_free_null_display)
{
	START;

	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(drv->native_free(NULL, (void *)0x1234), 1);

	END;
}

TEST(display_x11_dynamic_native_free_null_data)
{
	START;

	T_X11_DYNAMIC_DRV();
	display_t display = {.data = (void *)0x1234};

	EXPECT_EQ(drv->native_free(&display, NULL), 1);

	END;
}

TEST(display_x11_dynamic_native_free_calls_x11)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	t_x11.free_calls = 0;

	EXPECT_EQ(drv->native_free(&display, (void *)0x1234), 1);
	EXPECT_EQ(t_x11.free_calls, 1);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_init_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_init(NULL, &(window_config_t){.width = 640, .height = 480}), 1);

	END;
}

TEST(display_x11_dynamic_window_free_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_free(NULL), 1);

	END;
}

TEST(display_x11_dynamic_window_id_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_id(NULL), 0);

	END;
}

TEST(display_x11_dynamic_window_native_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	window_native_t native = {0};

	EXPECT_EQ(drv->window_native(NULL, &native), 1);

	END;
}

TEST(display_x11_dynamic_window_native_null_native)
{
	START;

	T_X11_DYNAMIC_DRV();
	window_t window = {.data = (void *)0x1234};

	EXPECT_EQ(drv->window_native(&window, NULL), 1);

	END;
}

TEST(display_x11_dynamic_window_set_title_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_set_title(NULL, STRV("title")), 1);

	END;
}

TEST(display_x11_dynamic_window_set_position_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_set_position(NULL, 11, 22), 1);

	END;
}

TEST(display_x11_dynamic_window_set_size_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_set_size(NULL, 333, 444), 1);

	END;
}

TEST(display_x11_dynamic_window_set_borderless_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_set_borderless(NULL, 1), 1);

	END;
}

TEST(display_x11_dynamic_window_set_fullscreen_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_set_fullscreen(NULL, 1), 1);

	END;
}

TEST(display_x11_dynamic_window_show_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_show(NULL), 1);

	END;
}

TEST(display_x11_dynamic_window_hide_null_window)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->window_hide(NULL), 1);

	END;
}

TEST(display_x11_dynamic_ext_init_null_ext)
{
	START;

	T_X11_DYNAMIC_DRV();
	EXPECT_EQ(drv->ext_init(NULL, STRV("RANDR")), 1);

	END;
}

TEST(display_x11_dynamic_alloc_id_null_display)
{
	START;

	T_X11_DYNAMIC_DRV();
	u32 id = 0;
	EXPECT_EQ(drv->alloc_id(NULL, &id), 1);

	END;
}

TEST(display_x11_dynamic_visual_depth_null_display)
{
	START;

	T_X11_DYNAMIC_DRV();
	u8 depth = 0;
	EXPECT_EQ(drv->visual_depth(NULL, 1, &depth), 1);

	END;
}

TEST(display_x11_dynamic_init_rejects_missing_library)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	fs_init(&fs, 0, 1, ALLOC_STD);
	proc_init(&proc, 256, 1, ALLOC_STD);
	sock_init(&ss, 0, 1, ALLOC_STD);
	proc_setenv(&proc, STRV("DISPLAY"), STRV(":0"), 1);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_alloc_failure)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_alloc_t state = {.fail_alloc = 1};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, t_alloc(&state)), NULL);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_missing_display)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	proc_unsetenv(&proc, STRV("DISPLAY"));
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_open_display_failure)
{
	START;

	t_x11_reset();
	t_x11.display_result = NULL;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_invalid_keycode_range)
{
	START;

	t_x11_reset();
	t_x11.min_keycode = 9;
	t_x11.max_keycode = 8;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	EXPECT_EQ(t_x11.close_display_calls, 1);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_keyboard_mapping_missing)
{
	START;

	t_x11_reset();
	t_x11.keyboard_mapping_null = 1;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_keyboard_mapping_invalid)
{
	START;

	t_x11_reset();
	t_x11.keysyms_per_keycode = 0;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	EXPECT_EQ(t_x11.free_calls, 1);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_modifier_mapping_missing)
{
	START;

	t_x11_reset();
	t_x11.modifier_mapping_null = 1;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_atom_failure)
{
	START;

	t_x11_reset();
	t_x11.intern_atom_zero_after = 1;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_maps_keysyms)
{
	START;

	t_x11_reset();
	t_x11_keysym_t keysyms[] = {
		'A', '0', 0xffb0, 0xffbe, 0x0060, 0x002d, 0x003d, 0x005b, 0x005d, 0x005c, 0x003b, 0x0027, 0x002c,
		0x002e, 0x002f, ' ', 0xff0d, 0xff09, 0xff08, 0xff1b, 0xffe5, 0xff7f, 0xff14, 0xff13, 0xff61,
		0xff63, 0xffff, 0xff50, 0xff57, 0xff55, 0xff56, 0xff52, 0xff54, 0xff51, 0xff53, 0xffe1, 0xffe2,
		0xffe3, 0xffe4, 0xffe9, 0xffea, 0xffeb, 0xffec, 0xff67, 0xff9e, 0xff9c, 0xff99, 0xff9b, 0xff96,
		0xff9d, 0xff98, 0xff95, 0xff97, 0xff9a, 0xff9f, 0xffae, 0xffaf, 0xffaa, 0xffad, 0xffab, 0xff8d, 0x1234,
	};
	mem_copy(t_x11.keysyms, sizeof(t_x11.keysyms), keysyms, sizeof(keysyms));
	t_x11.max_keycode = (int)(t_x11.min_keycode + sizeof(keysyms) / sizeof(keysyms[0]) - 1);
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_NE(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_init_maps_modifiers)
{
	START;

	t_x11_reset();
	t_x11_keysym_t keysyms[] = {0xffe1, 0xffe5, 0xffe3, 0xffe9, 0xff7f, 0xffeb, 0x1234};
	mem_copy(t_x11.keysyms, sizeof(t_x11.keysyms), keysyms, sizeof(keysyms));
	t_x11.max_keycode = 14;
	t_x11.modifiers.max_keypermod = 2;
	t_x11.modifiermap[0]  = 8;
	t_x11.modifiermap[2]  = 9;
	t_x11.modifiermap[4]  = 10;
	t_x11.modifiermap[6]  = 11;
	t_x11.modifiermap[8]  = 12;
	t_x11.modifiermap[10] = 13;
	t_x11.modifiermap[12] = 14;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();

	EXPECT_NE(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_init_success)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);

	EXPECT_EQ(t_x11_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_id(&window), 0x44);

	window_free(&window);
	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_native_returns_window)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	window_native_t native = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);

	EXPECT_EQ(t_x11_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_native(&window, &native), 0);
	EXPECT_EQ(native.type, DISPLAY_NATIVE_X11);
	EXPECT_EQ(native.window, (void *)(uintptr_t)0x44);

	window_free(&window);
	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_init_alloc_failure)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_alloc_t state   = {.fail_alloc = 1};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	display.alloc = t_alloc(&state);

	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}), NULL);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_init_custom_visual)
{
	START;

	t_x11_reset();
	t_x11.visual_info_result = &t_x11.visual_info;
	t_x11.visual_info_count  = 1;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_NE(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480, .visual = 0x21, .depth = 32}), NULL);
	EXPECT_EQ(t_x11.create_colormap_calls, 1);

	window_free(&window);
	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_init_unknown_visual)
{
	START;

	t_x11_reset();
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480, .visual = 0x21}), NULL);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_init_empty_visual_result)
{
	START;

	t_x11_reset();
	t_x11.visual_info_result = &t_x11.visual_info;
	t_x11.visual_info_count  = 0;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	int free_calls = t_x11.free_calls;

	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480, .visual = 0x21}), NULL);
	EXPECT_EQ(t_x11.free_calls, free_calls + 1);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_init_colormap_failure)
{
	START;

	t_x11_reset();
	t_x11.visual_info_result = &t_x11.visual_info;
	t_x11.visual_info_count  = 1;
	t_x11.create_colormap_result = 0;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480, .visual = 0x21}), NULL);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_init_create_failure)
{
	START;

	t_x11_reset();
	t_x11.create_window_result = 0;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}), NULL);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_init_wm_protocol_failure)
{
	START;

	t_x11_reset();
	t_x11.set_wm_protocols_result = 0;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}), NULL);
	EXPECT_EQ(t_x11.destroy_window_calls, 1);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_free_custom_visual)
{
	START;

	t_x11_reset();
	t_x11.visual_info_result = &t_x11.visual_info;
	t_x11.visual_info_count  = 1;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	window_init(&window, &display, &(window_config_t){.width = 640, .height = 480, .visual = 0x21});

	window_free(&window);
	EXPECT_EQ(t_x11.free_colormap_calls, 1);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_free_colormap_failure)
{
	START;

	t_x11_reset();
	t_x11.visual_info_result = &t_x11.visual_info;
	t_x11.visual_info_count  = 1;
	t_x11.free_colormap_result = 0;
	fs_t fs	      = {0};
	proc_t proc   = {0};
	sock_t ss     = {0};
	display_t display = {0};
	window_t window   = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	window_init(&window, &display, &(window_config_t){.width = 640, .height = 480, .visual = 0x21});

	window_free(&window);

	display_free(&display);
	t_x11_dynamic_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_dynamic_window_set_title)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_set_title(&window, STRV("title")), 0);
	EXPECT_EQ(t_x11.change_property_calls, 2);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_set_title_invalid_text)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_set_title(&window, STRVN(NULL, 1)), 1);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_set_title_too_long)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_set_title(&window, (strv_t){.data = "", .len = (size_t)INT_MAX + 1}), 1);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_set_title_change_failure)
{
	START;

	t_x11_reset();
	t_x11.change_property_result = 0;
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_set_title(&window, STRV("title")), 1);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_geometry)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_set_position(&window, 11, 22), 0);
	EXPECT_EQ(window_set_size(&window, 333, 444), 0);
	EXPECT_EQ(t_x11.move_window_calls, 1);
	EXPECT_EQ(t_x11.resize_window_calls, 1);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_set_borderless)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_set_borderless(&window, 1), 0);
	EXPECT_EQ(window_set_borderless(&window, 0), 0);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_set_borderless_property_failure)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);
	t_x11.change_property_result = 0;

	EXPECT_EQ(window_set_borderless(&window, 1), 1);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_set_fullscreen_unmapped)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_set_fullscreen(&window, 1), 0);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_set_fullscreen_mapped)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);
	window_show(&window);

	EXPECT_EQ(window_set_fullscreen(&window, 0), 0);
	EXPECT_EQ(t_x11.send_event_calls, 1);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_set_fullscreen_send_failure)
{
	START;

	t_x11_reset();
	t_x11.send_event_result = 0;
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);
	window_show(&window);

	EXPECT_EQ(window_set_fullscreen(&window, 1), 1);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_visibility)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_show(&window), 0);
	EXPECT_EQ(window_hide(&window), 0);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_show_failure)
{
	START;

	t_x11_reset();
	t_x11.map_window_result = 0;
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_show(&window), 1);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_window_hide_failure)
{
	START;

	t_x11_reset();
	t_x11.unmap_window_result = 0;
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; window_t window = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_hide(&window), 1);

	window_free(&window); display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_ext_init_success)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; display_ext_t ext = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	ext.display = &display;

	EXPECT_EQ(drv->ext_init(&ext, STRV("RANDR")), 0);
	EXPECT_EQ(ext.opcode, 1);
	EXPECT_EQ(ext.first_event, 2);
	EXPECT_EQ(ext.first_error, 3);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_ext_init_name_too_long)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; display_ext_t ext = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	ext.display = &display;

	EXPECT_EQ(drv->ext_init(&ext, (strv_t){.data = "x", .len = 256}), 1);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_ext_init_unavailable)
{
	START;

	t_x11_reset();
	t_x11.query_extension_result = 0;
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0}; display_ext_t ext = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	ext.display = &display;

	EXPECT_EQ(drv->ext_init(&ext, STRV("RANDR")), 1);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_alloc_id_success)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	u32 id = 0;
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(drv->alloc_id(&display, &id), 0);
	EXPECT_EQ(id, 0x66);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_alloc_id_failure)
{
	START;

	t_x11_reset();
	t_x11.alloc_id_result = 0;
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	u32 id = 1;
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(drv->alloc_id(&display, &id), 1);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_visual_depth_success)
{
	START;

	t_x11_reset();
	t_x11.visual_info_result = &t_x11.visual_info;
	t_x11.visual_info_count  = 1;
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	u8 depth = 0;
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(drv->visual_depth(&display, 0x21, &depth), 0);
	EXPECT_EQ(depth, 24);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_visual_depth_unknown)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	u8 depth = 0;
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	T_X11_DYNAMIC_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(drv->visual_depth(&display, 0x21, &depth), 1);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_poll_events_none)
{
	START;

	t_x11_reset();
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss);
	t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_x11_event_calls, 0);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_poll_events_resize)
{
	START;

	t_x11_reset(); t_x11_event_calls = 0;
	t_x11_push_event((t_x11_event_t){.xconfigure = {.type = T_X11_CONFIGURE_NOTIFY, .window = 0x44, .x = 1, .y = 2, .width = 3, .height = 4}});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_RESIZE);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_poll_events_ignored)
{
	START;

	t_x11_reset();
	t_x11_push_event((t_x11_event_t){.type = T_X11_EXPOSE});
	t_x11_push_pending(1);
	t_x11_push_pending(0);
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_poll_events(&display), 0);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_key)
{
	START;

	t_x11_reset(); t_x11_event_calls = 0;
	t_x11.lookup_keysym = 'Z';
	t_x11_push_event((t_x11_event_t){.xkey = {.type = T_X11_KEY_PRESS, .window = 0x44, .x = 11, .y = 22, .state = 0x1f00}});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_Z);
	EXPECT_EQ(t_x11_event.modifiers, DISPLAY_MOD_MOUSE_LEFT | DISPLAY_MOD_MOUSE_MIDDLE | DISPLAY_MOD_MOUSE_RIGHT | DISPLAY_MOD_MOUSE_WHEEL_UP | DISPLAY_MOD_MOUSE_WHEEL_DOWN);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_key_modifier_maps_state)
{
	START;

	t_x11_reset();
	t_x11_keysym_t keysyms[] = {'a', 0xffe1};
	mem_copy(t_x11.keysyms, sizeof(t_x11.keysyms), keysyms, sizeof(keysyms));
	t_x11.max_keycode = 9;
	t_x11.modifiers.max_keypermod = 1;
	t_x11.modifiermap[0] = 9;
	t_x11.lookup_keysym = 'a';
	t_x11_push_event((t_x11_event_t){.xkey = {.type = T_X11_KEY_PRESS, .window = 0x44, .state = 1}});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.modifiers, DISPLAY_MOD_SHIFT);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_button)
{
	START;

	t_x11_reset();
	t_x11_push_event((t_x11_event_t){.xbutton = {.type = T_X11_BUTTON_PRESS, .window = 0x44, .keycode = 9}});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_FORWARD);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_mouse_buttons)
{
	START;

	t_x11_reset();
	display_mouse_t buttons[] = {
		DISPLAY_MOUSE_LEFT,
		DISPLAY_MOUSE_MIDDLE,
		DISPLAY_MOUSE_RIGHT,
		DISPLAY_MOUSE_WHEEL_UP,
		DISPLAY_MOUSE_WHEEL_DOWN,
		DISPLAY_MOUSE_WHEEL_LEFT,
		DISPLAY_MOUSE_WHEEL_RIGHT,
		DISPLAY_MOUSE_BACK,
		DISPLAY_MOUSE_FORWARD,
		DISPLAY_MOUSE_UNKNOWN,
	};
	for (u32 i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
		t_x11_push_event((t_x11_event_t){.xbutton = {.type = T_X11_BUTTON_PRESS, .window = 0x44, .keycode = i + 1}});
	}
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	for (u32 i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
		EXPECT_EQ(display_wait_events(&display), 0);
		EXPECT_EQ(t_x11_event.button, buttons[i]);
	}

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_motion)
{
	START;

	t_x11_reset();
	t_x11_push_event((t_x11_event_t){.xmotion = {.type = T_X11_MOTION_NOTIFY, .window = 0x44, .x = 3, .y = 4}});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_MOVE);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_focus)
{
	START;

	t_x11_reset();
	t_x11_push_event((t_x11_event_t){.xfocus = {.type = T_X11_FOCUS_OUT, .window = 0x44}});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_FOCUS_LOST);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_close)
{
	START;

	t_x11_reset();
	t_x11_push_event((t_x11_event_t){.xdestroywindow = {.type = T_X11_DESTROY_NOTIFY, .window = 0x44}});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_CLOSE);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_client_close)
{
	START;

	t_x11_reset();
	t_x11_push_event((t_x11_event_t){.xclient = {.type = T_X11_CLIENT_MESSAGE, .window = 0x44, .message_type = 101, .format = 32, .data.l = {102}}});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_CLOSE);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_client_ignored)
{
	START;

	t_x11_reset();
	t_x11_push_event((t_x11_event_t){.xclient = {.type = T_X11_CLIENT_MESSAGE, .window = 0x44, .message_type = 101, .format = 8}});
	t_x11_push_event((t_x11_event_t){.xdestroywindow = {.type = T_X11_DESTROY_NOTIFY, .window = 0x44}});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_CLOSE);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_x11_dynamic_wait_events_unknown)
{
	START;

	t_x11_reset();
	t_x11_push_event((t_x11_event_t){.type = 99});
	fs_t fs = {0}; proc_t proc = {0}; sock_t ss = {0}; display_t display = {0};
	t_x11_dynamic_env_init(&fs, &proc, &ss); t_x11_open(&display, NULL, &fs, &proc, &ss);

	EXPECT_EQ(display_wait_events(&display), 1);

	display_free(&display); t_x11_dynamic_env_free(&fs, &proc, &ss);
	END;
}

STEST(display_x11_dynamic)
{
	SSTART;

	RUN(display_x11_dynamic_driver_is_registered);
	RUN(display_x11_dynamic_driver_has_display_hooks);
	RUN(display_x11_dynamic_driver_has_window_hooks);
	RUN(display_x11_dynamic_driver_has_ext_init);
	RUN(display_x11_dynamic_driver_has_alloc_id);
	RUN(display_x11_dynamic_driver_has_visual_depth);
	RUN(display_x11_dynamic_init_null_display);
	RUN(display_x11_dynamic_init_null_alloc);
	RUN(display_x11_dynamic_init_uses_virtual_proc);
	RUN(display_x11_dynamic_free_closes_virtual_display);
	RUN(display_x11_dynamic_init_rejects_missing_virtual_symbol);
	RUN(display_x11_dynamic_init_rejects_missing_alloc_id_symbol);
	RUN(display_x11_dynamic_init_rejects_missing_library);
	RUN(display_x11_dynamic_init_alloc_failure);
	RUN(display_x11_dynamic_init_missing_display);
	RUN(display_x11_dynamic_init_open_display_failure);
	RUN(display_x11_dynamic_init_invalid_keycode_range);
	RUN(display_x11_dynamic_init_keyboard_mapping_missing);
	RUN(display_x11_dynamic_init_keyboard_mapping_invalid);
	RUN(display_x11_dynamic_init_modifier_mapping_missing);
	RUN(display_x11_dynamic_init_atom_failure);
	RUN(display_x11_dynamic_init_maps_keysyms);
	RUN(display_x11_dynamic_init_maps_modifiers);
	RUN(display_x11_dynamic_free_null_display);
	RUN(display_x11_dynamic_poll_events_null_display);
	RUN(display_x11_dynamic_wait_events_null_display);
	RUN(display_x11_dynamic_native_null_display);
	RUN(display_x11_dynamic_native_null_native);
	RUN(display_x11_dynamic_native_returns_display);
	RUN(display_x11_dynamic_native_free_null_display);
	RUN(display_x11_dynamic_native_free_null_data);
	RUN(display_x11_dynamic_native_free_calls_x11);
	RUN(display_x11_dynamic_window_init_null_window);
	RUN(display_x11_dynamic_window_free_null_window);
	RUN(display_x11_dynamic_window_id_null_window);
	RUN(display_x11_dynamic_window_native_null_window);
	RUN(display_x11_dynamic_window_native_null_native);
	RUN(display_x11_dynamic_window_set_title_null_window);
	RUN(display_x11_dynamic_window_set_position_null_window);
	RUN(display_x11_dynamic_window_set_size_null_window);
	RUN(display_x11_dynamic_window_set_borderless_null_window);
	RUN(display_x11_dynamic_window_set_fullscreen_null_window);
	RUN(display_x11_dynamic_window_show_null_window);
	RUN(display_x11_dynamic_window_hide_null_window);
	RUN(display_x11_dynamic_ext_init_null_ext);
	RUN(display_x11_dynamic_alloc_id_null_display);
	RUN(display_x11_dynamic_visual_depth_null_display);
	RUN(display_x11_dynamic_window_init_success);
	RUN(display_x11_dynamic_window_native_returns_window);
	RUN(display_x11_dynamic_window_init_alloc_failure);
	RUN(display_x11_dynamic_window_init_custom_visual);
	RUN(display_x11_dynamic_window_init_unknown_visual);
	RUN(display_x11_dynamic_window_init_empty_visual_result);
	RUN(display_x11_dynamic_window_init_colormap_failure);
	RUN(display_x11_dynamic_window_init_create_failure);
	RUN(display_x11_dynamic_window_init_wm_protocol_failure);
	RUN(display_x11_dynamic_window_free_custom_visual);
	RUN(display_x11_dynamic_window_free_colormap_failure);
	RUN(display_x11_dynamic_window_set_title);
	RUN(display_x11_dynamic_window_set_title_invalid_text);
	RUN(display_x11_dynamic_window_set_title_too_long);
	RUN(display_x11_dynamic_window_set_title_change_failure);
	RUN(display_x11_dynamic_window_geometry);
	RUN(display_x11_dynamic_window_set_borderless);
	RUN(display_x11_dynamic_window_set_borderless_property_failure);
	RUN(display_x11_dynamic_window_set_fullscreen_unmapped);
	RUN(display_x11_dynamic_window_set_fullscreen_mapped);
	RUN(display_x11_dynamic_window_set_fullscreen_send_failure);
	RUN(display_x11_dynamic_window_visibility);
	RUN(display_x11_dynamic_window_show_failure);
	RUN(display_x11_dynamic_window_hide_failure);
	RUN(display_x11_dynamic_ext_init_success);
	RUN(display_x11_dynamic_ext_init_name_too_long);
	RUN(display_x11_dynamic_ext_init_unavailable);
	RUN(display_x11_dynamic_alloc_id_success);
	RUN(display_x11_dynamic_alloc_id_failure);
	RUN(display_x11_dynamic_visual_depth_success);
	RUN(display_x11_dynamic_visual_depth_unknown);
	RUN(display_x11_dynamic_poll_events_none);
	RUN(display_x11_dynamic_poll_events_resize);
	RUN(display_x11_dynamic_poll_events_ignored);
	RUN(display_x11_dynamic_wait_events_key);
	RUN(display_x11_dynamic_wait_events_key_modifier_maps_state);
	RUN(display_x11_dynamic_wait_events_button);
	RUN(display_x11_dynamic_wait_events_mouse_buttons);
	RUN(display_x11_dynamic_wait_events_motion);
	RUN(display_x11_dynamic_wait_events_focus);
	RUN(display_x11_dynamic_wait_events_close);
	RUN(display_x11_dynamic_wait_events_client_close);
	RUN(display_x11_dynamic_wait_events_client_ignored);
	RUN(display_x11_dynamic_wait_events_unknown);

	SEND;
}

#undef T_X11_DYNAMIC_DRV
