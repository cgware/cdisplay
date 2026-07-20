#include "test.h"

#include "display_driver.h"
#include "fs.h"
#include "mem.h"
#include "proc.h"
#include "sock.h"

#include <stdarg.h>

typedef struct t_wl_display_s t_wl_display_t;
typedef struct t_wl_proxy_s t_wl_proxy_t;
typedef struct t_wl_registry_s t_wl_registry_t;
typedef struct t_wl_compositor_s t_wl_compositor_t;
typedef struct t_wl_surface_s t_wl_surface_t;
typedef struct t_xdg_wm_base_s t_xdg_wm_base_t;
typedef struct t_xdg_surface_s t_xdg_surface_t;
typedef struct t_xdg_toplevel_s t_xdg_toplevel_t;

typedef struct t_wl_interface_s {
	const char *name;
	int version;
	int method_count;
	const void *methods;
	int event_count;
	const void *events;
} t_wl_interface_t;

typedef struct t_wl_registry_listener_s {
	void (*global)(void *data, t_wl_registry_t *registry, u32 name, const char *interface, u32 version);
	void (*global_remove)(void *data, t_wl_registry_t *registry, u32 name);
} t_wl_registry_listener_t;

typedef struct t_xdg_wm_base_listener_s {
	void (*ping)(void *data, t_xdg_wm_base_t *xdg_wm_base, u32 serial);
} t_xdg_wm_base_listener_t;

typedef struct t_xdg_surface_listener_s {
	void (*configure)(void *data, t_xdg_surface_t *xdg_surface, u32 serial);
} t_xdg_surface_listener_t;

typedef struct t_xdg_toplevel_listener_s {
	void (*configure)(void *data, t_xdg_toplevel_t *xdg_toplevel, int width, int height, void *states);
	void (*close)(void *data, t_xdg_toplevel_t *xdg_toplevel);
} t_xdg_toplevel_listener_t;

typedef void (*t_wayland_symbol_t)(void);

enum {
	T_WL_DISPLAY_GET_REGISTRY	= 1,
	T_WL_REGISTRY_BIND		= 0,
	T_WL_SURFACE_DESTROY		= 0,
	T_WL_SURFACE_COMMIT		= 6,
	T_WL_COMPOSITOR_CREATE_SURFACE	= 0,
	T_XDG_WM_BASE_GET_XDG_SURFACE	= 2,
	T_XDG_WM_BASE_PONG		= 3,
	T_XDG_SURFACE_DESTROY		= 0,
	T_XDG_SURFACE_GET_TOPLEVEL	= 1,
	T_XDG_SURFACE_ACK_CONFIGURE	= 4,
	T_XDG_TOPLEVEL_DESTROY		= 0,
	T_XDG_TOPLEVEL_SET_TITLE	= 2,
	T_XDG_TOPLEVEL_SET_FULLSCREEN	= 11,
	T_XDG_TOPLEVEL_UNSET_FULLSCREEN = 12,
};

typedef struct t_wayland_state_s {
	int connect_calls;
	int disconnect_calls;
	int roundtrip_calls;
	int dispatch_calls;
	int dispatch_pending_calls;
	int flush_calls;
	int add_listener_calls;
	int constructor_calls;
	int get_registry_constructor_calls;
	int marshal_calls;
	int destroy_calls;
	int connect_null;
	int roundtrip_result;
	int dispatch_result;
	int dispatch_pending_result;
	int flush_result;
	int add_listener_result;
	int add_listener_fail_after;
	int constructor_null_after;
	int roundtrip_fail_after;
	t_wl_registry_listener_t *registry_listener;
	void *registry_listener_data;
	t_xdg_wm_base_listener_t *wm_base_listener;
	void *wm_base_listener_data;
	t_xdg_surface_listener_t *xdg_surface_listener;
	void *xdg_surface_listener_data;
	t_xdg_toplevel_listener_t *xdg_toplevel_listener;
	void *xdg_toplevel_listener_data;
	u32 registry_global_count;
	u32 last_bind_name;
	const char *last_bind_interface;
	u32 last_bind_version;
	u32 last_constructor_opcode;
	const char *last_constructor_interface;
	u32 last_marshal_opcode;
	u32 last_marshal_u32;
	int last_marshal_i0;
	int last_marshal_i1;
	const char *last_title;
	void *last_fullscreen_output;
} t_wayland_state_t;

static t_wayland_state_t t_wayland;
static t_wl_display_t *t_wayland_display	= (t_wl_display_t *)0x11u;
static t_wl_registry_t *t_wayland_registry	= (t_wl_registry_t *)0x22u;
static t_wl_compositor_t *t_wayland_compositor	= (t_wl_compositor_t *)0x33u;
static t_xdg_wm_base_t *t_wayland_wm_base	= (t_xdg_wm_base_t *)0x44u;
static t_wl_surface_t *t_wayland_surface	= (t_wl_surface_t *)0x55u;
static t_xdg_surface_t *t_wayland_xdg_surface	= (t_xdg_surface_t *)0x66u;
static t_xdg_toplevel_t *t_wayland_xdg_toplevel = (t_xdg_toplevel_t *)0x77u;

typedef struct t_window_wayland_dynamic_s {
	t_wl_surface_t *surface;
	t_xdg_surface_t *xdg_surface;
	t_xdg_toplevel_t *xdg_toplevel;
	u16 width;
	u16 height;
	u16 pending_width;
	u16 pending_height;
	int mapped;
} t_window_wayland_dynamic_t;

static void *t_wayland_null_alloc(alloc_t *alloc, size_t size)
{
	(void)alloc;
	(void)size;
	return NULL;
}

static void t_wayland_reset(void)
{
	t_wayland = (t_wayland_state_t){
		.roundtrip_result	 = 0,
		.dispatch_result	 = 1,
		.dispatch_pending_result = 0,
		.flush_result		 = 0,
	};
}

static display_driver_t *t_wayland_dynamic_driver(void)
{
	return display_driver_find(STRV("Wayland-dynamic"));
}

static void *t_wayland_symbol(t_wayland_symbol_t fn)
{
	union {
		t_wayland_symbol_t fn;
		void *ptr;
	} symbol = {.fn = fn};

	return symbol.ptr;
}

static t_wl_display_t *t_wl_display_connect(const char *name)
{
	(void)name;
	t_wayland.connect_calls++;
	return t_wayland.connect_null ? NULL : t_wayland_display;
}

static void t_wl_display_disconnect(t_wl_display_t *display)
{
	(void)display;
	t_wayland.disconnect_calls++;
}

static int t_wl_display_roundtrip(t_wl_display_t *display)
{
	(void)display;
	t_wayland.roundtrip_calls++;
	if (t_wayland.roundtrip_fail_after != 0 && t_wayland.roundtrip_calls >= t_wayland.roundtrip_fail_after) {
		return -1;
	}
	if (t_wayland.roundtrip_result < 0) {
		return t_wayland.roundtrip_result;
	}
	if (t_wayland.roundtrip_calls == 1 && t_wayland.registry_listener != NULL) {
		if (t_wayland.registry_global_count > 0) {
			t_wayland.registry_listener->global(t_wayland.registry_listener_data, t_wayland_registry, 7, "wl_compositor", 4);
		}
		if (t_wayland.registry_global_count > 1) {
			t_wayland.registry_listener->global(t_wayland.registry_listener_data, t_wayland_registry, 8, "xdg_wm_base", 3);
		}
	}
	return t_wayland.roundtrip_result;
}

static int t_wl_display_dispatch(t_wl_display_t *display)
{
	(void)display;
	t_wayland.dispatch_calls++;
	return t_wayland.dispatch_result;
}

static int t_wl_display_dispatch_pending(t_wl_display_t *display)
{
	(void)display;
	t_wayland.dispatch_pending_calls++;
	return t_wayland.dispatch_pending_result;
}

static int t_wl_display_flush(t_wl_display_t *display)
{
	(void)display;
	t_wayland.flush_calls++;
	return t_wayland.flush_result;
}

static int t_wl_proxy_add_listener(t_wl_proxy_t *proxy, void (**listener)(void), void *data)
{
	t_wayland.add_listener_calls++;
	if (proxy == (t_wl_proxy_t *)t_wayland_registry) {
		t_wayland.registry_listener	 = (t_wl_registry_listener_t *)listener;
		t_wayland.registry_listener_data = data;
	} else if (proxy == (t_wl_proxy_t *)t_wayland_wm_base) {
		t_wayland.wm_base_listener	= (t_xdg_wm_base_listener_t *)listener;
		t_wayland.wm_base_listener_data = data;
	} else if (proxy == (t_wl_proxy_t *)t_wayland_xdg_surface) {
		t_wayland.xdg_surface_listener	    = (t_xdg_surface_listener_t *)listener;
		t_wayland.xdg_surface_listener_data = data;
	} else if (proxy == (t_wl_proxy_t *)t_wayland_xdg_toplevel) {
		t_wayland.xdg_toplevel_listener	     = (t_xdg_toplevel_listener_t *)listener;
		t_wayland.xdg_toplevel_listener_data = data;
	}
	if (t_wayland.add_listener_fail_after != 0 && t_wayland.add_listener_calls >= t_wayland.add_listener_fail_after) {
		return 1;
	}
	return t_wayland.add_listener_result;
}

static t_wl_proxy_t *t_wl_proxy_marshal_constructor_versioned(t_wl_proxy_t *proxy, u32 opcode, const t_wl_interface_t *interface,
							      u32 version, ...)
{
	(void)version;
	t_wayland.constructor_calls++;
	t_wayland.last_constructor_opcode    = opcode;
	t_wayland.last_constructor_interface = interface->name;
	if (t_wayland.constructor_null_after != 0 && t_wayland.constructor_calls >= t_wayland.constructor_null_after) {
		return NULL;
	}

	va_list args;
	va_start(args, version);
	if (proxy == (t_wl_proxy_t *)t_wayland_display && opcode == T_WL_DISPLAY_GET_REGISTRY) {
		t_wayland.get_registry_constructor_calls++;
		va_end(args);
		return (t_wl_proxy_t *)t_wayland_registry;
	}
	if (proxy == (t_wl_proxy_t *)t_wayland_registry && opcode == T_WL_REGISTRY_BIND) {
		t_wayland.last_bind_name      = va_arg(args, u32);
		t_wayland.last_bind_interface = va_arg(args, const char *);
		t_wayland.last_bind_version   = va_arg(args, u32);
	}
	va_end(args);

	if (strv_eq(strv_cstr(interface->name), STRV("wl_compositor"))) {
		return (t_wl_proxy_t *)t_wayland_compositor;
	}
	if (strv_eq(strv_cstr(interface->name), STRV("xdg_wm_base"))) {
		return (t_wl_proxy_t *)t_wayland_wm_base;
	}
	if (strv_eq(strv_cstr(interface->name), STRV("wl_surface"))) {
		return (t_wl_proxy_t *)t_wayland_surface;
	}
	if (strv_eq(strv_cstr(interface->name), STRV("xdg_surface"))) {
		return (t_wl_proxy_t *)t_wayland_xdg_surface;
	}
	if (strv_eq(strv_cstr(interface->name), STRV("xdg_toplevel"))) {
		return (t_wl_proxy_t *)t_wayland_xdg_toplevel;
	}
	return NULL;
}

static void t_wl_proxy_marshal(t_wl_proxy_t *proxy, u32 opcode, ...)
{
	(void)proxy;
	t_wayland.marshal_calls++;
	t_wayland.last_marshal_opcode = opcode;

	va_list args;
	va_start(args, opcode);
	if (opcode == T_XDG_WM_BASE_PONG || opcode == T_XDG_SURFACE_ACK_CONFIGURE) {
		t_wayland.last_marshal_u32 = va_arg(args, u32);
	} else if (opcode == T_XDG_TOPLEVEL_SET_TITLE) {
		t_wayland.last_title = va_arg(args, const char *);
	} else if (opcode == T_XDG_TOPLEVEL_SET_FULLSCREEN) {
		t_wayland.last_fullscreen_output = va_arg(args, void *);
	}
	va_end(args);
}

static void t_wl_proxy_destroy(t_wl_proxy_t *proxy)
{
	(void)proxy;
	t_wayland.destroy_calls++;
}

static u32 t_wl_proxy_get_id(t_wl_proxy_t *proxy)
{
	return proxy == (t_wl_proxy_t *)t_wayland_surface ? 0x55u : 0;
}

static void t_wayland_set_symbols(proc_t *proc)
{
#define T_WAYLAND_SET(_name)                                                                                                               \
	proc_setdlsym(proc, STRV("libwayland-client.so.0"), STRV("wl_" #_name), t_wayland_symbol((t_wayland_symbol_t)t_wl_##_name))

	T_WAYLAND_SET(display_connect);
	T_WAYLAND_SET(display_disconnect);
	T_WAYLAND_SET(display_roundtrip);
	T_WAYLAND_SET(display_dispatch);
	T_WAYLAND_SET(display_dispatch_pending);
	T_WAYLAND_SET(display_flush);
	T_WAYLAND_SET(proxy_add_listener);
	T_WAYLAND_SET(proxy_marshal_constructor_versioned);
	T_WAYLAND_SET(proxy_marshal);
	T_WAYLAND_SET(proxy_destroy);
	T_WAYLAND_SET(proxy_get_id);

#undef T_WAYLAND_SET
}

static void t_wayland_env_init(fs_t *fs, proc_t *proc, sock_t *ss)
{
	fs_init(fs, 0, 1, ALLOC_STD);
	proc_init(proc, 256, 1, ALLOC_STD);
	sock_init(ss, 0, 1, ALLOC_STD);
	t_wayland.registry_global_count = 2;
	t_wayland_set_symbols(proc);
}

static void t_wayland_env_free(fs_t *fs, proc_t *proc, sock_t *ss)
{
	fs_free(fs);
	proc_free(proc);
	sock_free(ss);
}

static int t_wayland_open(display_t *display, window_t *window, fs_t *fs, proc_t *proc, sock_t *ss)
{
	display_driver_t *drv = t_wayland_dynamic_driver();
	if (drv == NULL || display_init(display, drv, fs, proc, ss, ALLOC_STD) == NULL) {
		return 1;
	}
	if (window != NULL && window_init(window, display, &(window_config_t){.width = 640, .height = 480}) == NULL) {
		display_free(display);
		return 1;
	}
	return 0;
}

static int t_wayland_event_calls;
static display_event_t t_wayland_event;

static void t_wayland_event_cb(display_t *display, const display_event_t *event, void *user)
{
	(void)display;
	(void)user;
	t_wayland_event_calls++;
	t_wayland_event = *event;
}

#define T_WAYLAND_DRV()                                                                                                                    \
	display_driver_t *drv = t_wayland_dynamic_driver();                                                                                \
	EXPECT_NOT_NULL(drv)

TEST(display_wayland_dynamic_driver_is_registered)
{
	START;

	EXPECT_NOT_NULL(t_wayland_dynamic_driver());

	END;
}

TEST(display_wayland_dynamic_driver_has_display_hooks)
{
	START;

	T_WAYLAND_DRV();
	EXPECT(drv->init != NULL);
	EXPECT(drv->free != NULL);
	EXPECT(drv->poll_events != NULL);
	EXPECT(drv->wait_events != NULL);
	EXPECT(drv->native != NULL);

	END;
}

TEST(display_wayland_dynamic_driver_has_window_hooks)
{
	START;

	T_WAYLAND_DRV();
	EXPECT(drv->window_init != NULL);
	EXPECT(drv->window_free != NULL);
	EXPECT(drv->window_id != NULL);
	EXPECT(drv->window_native != NULL);
	EXPECT(drv->window_set_title != NULL);
	EXPECT(drv->window_set_fullscreen != NULL);
	EXPECT(drv->window_show != NULL);
	EXPECT(drv->window_hide != NULL);

	END;
}

TEST(display_wayland_dynamic_driver_omits_position_hook)
{
	START;

	T_WAYLAND_DRV();
	EXPECT(drv->window_set_position == NULL);

	END;
}

TEST(display_wayland_dynamic_driver_omits_borderless_hook)
{
	START;

	T_WAYLAND_DRV();
	EXPECT(drv->window_set_borderless == NULL);

	END;
}

TEST(display_wayland_dynamic_driver_omits_size_hook)
{
	START;

	T_WAYLAND_DRV();
	EXPECT(drv->window_set_size == NULL);

	END;
}

TEST(display_wayland_dynamic_available_rejects_missing_runtime)
{
	START;

	proc_t proc = {0};
	proc_init(&proc, 256, 1, ALLOC_STD);
	T_WAYLAND_DRV();

	EXPECT_EQ(display_driver_available(drv, &proc), 0);

	proc_free(&proc);
	END;
}

TEST(display_wayland_dynamic_available_accepts_display)
{
	START;

	proc_t proc = {0};
	proc_init(&proc, 256, 1, ALLOC_STD);
	proc_setenv(&proc, STRV("XDG_RUNTIME_DIR"), STRV("/run/user/1000"), 1);
	proc_setenv(&proc, STRV("WAYLAND_DISPLAY"), STRV("wayland-0"), 1);
	T_WAYLAND_DRV();

	EXPECT_EQ(display_driver_available(drv, &proc), 1);

	proc_free(&proc);
	END;
}

TEST(display_wayland_dynamic_available_accepts_session)
{
	START;

	proc_t proc = {0};
	proc_init(&proc, 256, 1, ALLOC_STD);
	proc_setenv(&proc, STRV("XDG_RUNTIME_DIR"), STRV("/run/user/1000"), 1);
	proc_setenv(&proc, STRV("XDG_SESSION_TYPE"), STRV("wayland"), 1);
	T_WAYLAND_DRV();

	EXPECT_EQ(display_driver_available(drv, &proc), 1);

	proc_free(&proc);
	END;
}

TEST(display_wayland_dynamic_init_null_display)
{
	START;

	T_WAYLAND_DRV();
	EXPECT_EQ(drv->init(NULL), 1);

	END;
}

TEST(display_wayland_dynamic_init_rejects_alloc_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {
		.proc  = &proc,
		.alloc = {.alloc = t_wayland_null_alloc},
	};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_EQ(drv->init(&display), 1);

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_missing_symbol)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	fs_init(&fs, 0, 1, ALLOC_STD);
	proc_init(&proc, 256, 1, ALLOC_STD);
	sock_init(&ss, 0, 1, ALLOC_STD);
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_connects_display)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_NOT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));
	EXPECT_EQ(t_wayland.connect_calls, 1);

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_connect_failure)
{
	START;

	t_wayland_reset();
	t_wayland.connect_null = 1;
	fs_t fs		       = {0};
	proc_t proc	       = {0};
	sock_t ss	       = {0};
	display_t display      = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_gets_registry)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(t_wayland.get_registry_constructor_calls, 1);

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_binds_compositor)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_STR(t_wayland.last_bind_interface, "xdg_wm_base");

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_missing_globals)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland.registry_global_count = 1;
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_missing_symbol_after_open)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	fs_init(&fs, 0, 1, ALLOC_STD);
	proc_init(&proc, 256, 1, ALLOC_STD);
	sock_init(&ss, 0, 1, ALLOC_STD);
	proc_setdlsym(&proc,
		      STRV("libwayland-client.so.0"),
		      STRV("wl_display_connect"),
		      t_wayland_symbol((t_wayland_symbol_t)t_wl_display_connect));
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_missing_registry)
{
	START;

	t_wayland_reset();
	t_wayland.constructor_null_after = 1;
	fs_t fs				 = {0};
	proc_t proc			 = {0};
	sock_t ss			 = {0};
	display_t display		 = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_registry_listener_failure)
{
	START;

	t_wayland_reset();
	t_wayland.add_listener_fail_after = 1;
	fs_t fs				  = {0};
	proc_t proc			  = {0};
	sock_t ss			  = {0};
	display_t display		  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_globals_roundtrip_failure)
{
	START;

	t_wayland_reset();
	t_wayland.roundtrip_fail_after = 1;
	fs_t fs			       = {0};
	proc_t proc		       = {0};
	sock_t ss		       = {0};
	display_t display	       = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_compositor_bind_failure)
{
	START;

	t_wayland_reset();
	t_wayland.constructor_null_after = 2;
	fs_t fs				 = {0};
	proc_t proc			 = {0};
	sock_t ss			 = {0};
	display_t display		 = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_wm_base_bind_failure)
{
	START;

	t_wayland_reset();
	t_wayland.constructor_null_after = 3;
	fs_t fs				 = {0};
	proc_t proc			 = {0};
	sock_t ss			 = {0};
	display_t display		 = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_wm_base_listener_failure)
{
	START;

	t_wayland_reset();
	t_wayland.add_listener_fail_after = 2;
	fs_t fs				  = {0};
	proc_t proc			  = {0};
	sock_t ss			  = {0};
	display_t display		  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_init_rejects_binding_roundtrip_failure)
{
	START;

	t_wayland_reset();
	t_wayland.roundtrip_fail_after = 2;
	fs_t fs			       = {0};
	proc_t proc		       = {0};
	sock_t ss		       = {0};
	display_t display	       = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_free_disconnects_display)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();

	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	display_free(&display);

	EXPECT_EQ(t_wayland.disconnect_calls, 1);

	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_free_rejects_invalid_display)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->free(NULL), 1);
	EXPECT_EQ(drv->free(&(display_t){0}), 1);

	END;
}

TEST(display_wayland_dynamic_poll_events_rejects_invalid_display)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->poll_events(NULL), 1);
	EXPECT_EQ(drv->poll_events(&(display_t){0}), 1);

	END;
}

TEST(display_wayland_dynamic_wait_events_rejects_invalid_display)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->wait_events(NULL), 1);
	EXPECT_EQ(drv->wait_events(&(display_t){0}), 1);

	END;
}

TEST(display_wayland_dynamic_native_rejects_invalid_arguments)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->native(NULL, &(display_native_t){0}), 1);
	EXPECT_EQ(drv->native(&(display_t){0}, &(display_native_t){0}), 1);
	EXPECT_EQ(drv->native(&(display_t){.data = (void *)0x1}, NULL), 1);

	END;
}

TEST(display_wayland_dynamic_native_returns_type)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	display_native_t native = {0};
	display_native(&display, &native);

	EXPECT_EQ(native.type, DISPLAY_NATIVE_WAYLAND);

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_native_returns_display)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	display_native_t native = {0};
	display_native(&display, &native);

	EXPECT_EQ(native.display, t_wayland_display);

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_init_creates_surface)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);

	EXPECT_EQ(t_wayland_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_STR(t_wayland.last_constructor_interface, "xdg_toplevel");

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_init_rejects_surface_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	t_wayland.constructor_null_after = t_wayland.constructor_calls + 1;

	EXPECT_NULL(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}));

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_init_rejects_invalid_arguments)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->window_init(NULL, &(window_config_t){.width = 640, .height = 480}), 1);
	EXPECT_EQ(drv->window_init(&(window_t){0}, &(window_config_t){.width = 640, .height = 480}), 1);
	EXPECT_EQ(drv->window_init(&(window_t){.display = &(display_t){0}}, &(window_config_t){.width = 640, .height = 480}), 1);
	EXPECT_EQ(drv->window_init(&(window_t){.display = &(display_t){.data = (void *)0x1}},
				   &(window_config_t){.width = 640, .height = 480}),
		  1);
	EXPECT_EQ(drv->window_init(&(window_t){.display = &(display_t){.data = (void *)0x1, .alloc = ALLOC_STD}}, NULL), 1);

	END;
}

TEST(display_wayland_dynamic_window_init_rejects_alloc_failure)
{
	START;

	T_WAYLAND_DRV();
	display_t display = {
		.data  = (void *)0x1,
		.alloc = {.alloc = t_wayland_null_alloc},
	};
	window_t window = {
		.display = &display,
	};

	EXPECT_EQ(drv->window_init(&window, &(window_config_t){.width = 640, .height = 480}), 1);

	END;
}

TEST(display_wayland_dynamic_window_init_rejects_xdg_surface_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	t_wayland.constructor_null_after = t_wayland.constructor_calls + 2;

	EXPECT_NULL(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}));

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_init_rejects_xdg_surface_listener_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	t_wayland.add_listener_fail_after = t_wayland.add_listener_calls + 1;

	EXPECT_NULL(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}));

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_init_rejects_xdg_toplevel_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	t_wayland.constructor_null_after = t_wayland.constructor_calls + 3;

	EXPECT_NULL(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}));

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_init_rejects_xdg_toplevel_listener_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	t_wayland.add_listener_fail_after = t_wayland.add_listener_calls + 2;

	EXPECT_NULL(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}));

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_id_returns_surface_id)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);

	EXPECT_EQ(window_id(&window), 0x55u);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_free_rejects_invalid_window)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->window_free(NULL), 1);
	EXPECT_EQ(drv->window_free(&(window_t){0}), 1);
	EXPECT_EQ(drv->window_free(&(window_t){.display = &(display_t){0}}), 1);
	EXPECT_EQ(drv->window_free(&(window_t){.display = &(display_t){.data = (void *)0x1}}), 1);

	END;
}

TEST(display_wayland_dynamic_window_id_rejects_invalid_window)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->window_id(NULL), 0);
	EXPECT_EQ(drv->window_id(&(window_t){0}), 0);
	EXPECT_EQ(drv->window_id(&(window_t){.display = &(display_t){0}}), 0);
	EXPECT_EQ(drv->window_id(&(window_t){.display = &(display_t){.data = (void *)0x1}}), 0);

	END;
}

TEST(display_wayland_dynamic_window_id_rejects_missing_surface)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	((t_window_wayland_dynamic_t *)window.data)->surface = NULL;

	EXPECT_EQ(window_id(&window), 0);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_native_returns_type)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);

	window_native_t native = {0};
	window_native(&window, &native);

	EXPECT_EQ(native.type, DISPLAY_NATIVE_WAYLAND);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_native_returns_surface)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);

	window_native_t native = {0};
	window_native(&window, &native);

	EXPECT_EQ(native.window, t_wayland_surface);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_native_rejects_invalid_arguments)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->window_native(NULL, &(window_native_t){0}), 1);
	EXPECT_EQ(drv->window_native(&(window_t){0}, &(window_native_t){0}), 1);
	EXPECT_EQ(drv->window_native(&(window_t){.data = (void *)0x1}, NULL), 1);

	END;
}

TEST(display_wayland_dynamic_window_native_rejects_missing_surface)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	((t_window_wayland_dynamic_t *)window.data)->surface = NULL;

	window_native_t native = {0};
	EXPECT_EQ(window_native(&window, &native), 1);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_set_title_sends_title)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);

	window_set_title(&window, STRV("hello"));

	EXPECT_STR(t_wayland.last_title, "hello");

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_set_title_rejects_missing_toplevel)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	((t_window_wayland_dynamic_t *)window.data)->xdg_toplevel = NULL;

	EXPECT_EQ(window_set_title(&window, STRV("hello")), 1);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_set_title_rejects_invalid_arguments)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->window_set_title(NULL, STRV("hello")), 1);
	EXPECT_EQ(drv->window_set_title(&(window_t){0}, STRV("hello")), 1);
	EXPECT_EQ(drv->window_set_title(&(window_t){.display = &(display_t){0}}, STRV("hello")), 1);
	EXPECT_EQ(drv->window_set_title(&(window_t){.display = &(display_t){.data = (void *)0x1}}, STRV("hello")), 1);
	EXPECT_EQ(drv->window_set_title(&(window_t){.display = &(display_t){.data = (void *)0x1}, .data = (void *)0x2}, STRVN(NULL, 1)), 1);

	END;
}

TEST(display_wayland_dynamic_window_set_title_rejects_long_title)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	char title[256] = {0};

	EXPECT_EQ(window_set_title(&window, STRVN(title, sizeof(title))), 1);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_set_title_returns_flush_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	t_wayland.flush_result = -1;

	EXPECT_EQ(window_set_title(&window, STRV("hello")), 1);

	t_wayland.flush_result = 0;
	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_show_commits_surface)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);

	window_show(&window);

	EXPECT_EQ(t_wayland.last_marshal_opcode, T_WL_SURFACE_COMMIT);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_show_recreates_role)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	((t_window_wayland_dynamic_t *)window.data)->xdg_toplevel = NULL;
	((t_window_wayland_dynamic_t *)window.data)->xdg_surface  = NULL;

	EXPECT_EQ(window_show(&window), 0);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_show_returns_flush_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	t_wayland.flush_result = -1;

	EXPECT_EQ(window_show(&window), 1);

	t_wayland.flush_result = 0;
	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_show_rejects_invalid_window)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->window_show(NULL), 1);
	EXPECT_EQ(drv->window_show(&(window_t){0}), 1);
	EXPECT_EQ(drv->window_show(&(window_t){.display = &(display_t){0}}), 1);
	EXPECT_EQ(drv->window_show(&(window_t){.display = &(display_t){.data = (void *)0x1}}), 1);

	END;
}

TEST(display_wayland_dynamic_window_show_rejects_role_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	((t_window_wayland_dynamic_t *)window.data)->xdg_toplevel = NULL;
	((t_window_wayland_dynamic_t *)window.data)->xdg_surface  = NULL;
	t_wayland.constructor_null_after			  = t_wayland.constructor_calls + 1;

	EXPECT_EQ(window_show(&window), 1);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_hide_destroys_role)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);

	window_hide(&window);

	EXPECT_EQ(t_wayland.destroy_calls, 2);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_hide_returns_flush_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	t_wayland.flush_result = -1;

	EXPECT_EQ(window_hide(&window), 1);

	t_wayland.flush_result = 0;
	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_hide_rejects_invalid_window)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->window_hide(NULL), 1);
	EXPECT_EQ(drv->window_hide(&(window_t){0}), 1);
	EXPECT_EQ(drv->window_hide(&(window_t){.display = &(display_t){0}}), 1);
	EXPECT_EQ(drv->window_hide(&(window_t){.display = &(display_t){.data = (void *)0x1}}), 1);

	END;
}

TEST(display_wayland_dynamic_window_set_fullscreen_sends_set)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);

	window_set_fullscreen(&window, 1);

	EXPECT_EQ(t_wayland.last_marshal_opcode, T_XDG_TOPLEVEL_SET_FULLSCREEN);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_set_fullscreen_rejects_missing_toplevel)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	((t_window_wayland_dynamic_t *)window.data)->xdg_toplevel = NULL;

	EXPECT_EQ(window_set_fullscreen(&window, 1), 1);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_set_fullscreen_rejects_invalid_window)
{
	START;

	T_WAYLAND_DRV();

	EXPECT_EQ(drv->window_set_fullscreen(NULL, 1), 1);
	EXPECT_EQ(drv->window_set_fullscreen(&(window_t){0}, 1), 1);
	EXPECT_EQ(drv->window_set_fullscreen(&(window_t){.display = &(display_t){0}}, 1), 1);
	EXPECT_EQ(drv->window_set_fullscreen(&(window_t){.display = &(display_t){.data = (void *)0x1}}, 1), 1);

	END;
}

TEST(display_wayland_dynamic_window_set_fullscreen_returns_flush_failure)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	t_wayland.flush_result = -1;

	EXPECT_EQ(window_set_fullscreen(&window, 1), 1);

	t_wayland.flush_result = 0;
	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_window_set_fullscreen_sends_unset)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);

	window_set_fullscreen(&window, 0);

	EXPECT_EQ(t_wayland.last_marshal_opcode, T_XDG_TOPLEVEL_UNSET_FULLSCREEN);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_poll_events_dispatches_pending)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	display_poll_events(&display);

	EXPECT_EQ(t_wayland.dispatch_pending_calls, 1);

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_wait_events_dispatches)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	display_wait_events(&display);

	EXPECT_EQ(t_wayland.dispatch_calls, 1);

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_ping_sends_pong)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	t_wayland.wm_base_listener->ping(t_wayland.wm_base_listener_data, t_wayland_wm_base, 123);

	EXPECT_EQ(t_wayland.last_marshal_u32, 123);

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_global_remove_ignores_global)
{
	START;

	t_wayland_reset();
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};
	display_t display = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	T_WAYLAND_DRV();
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	t_wayland.registry_listener->global_remove(t_wayland.registry_listener_data, t_wayland_registry, 7);

	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_close_emits_event)
{
	START;

	t_wayland_reset();
	t_wayland_event_calls = 0;
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	display_set_event_callback(&display, t_wayland_event_cb, NULL);

	t_wayland.xdg_toplevel_listener->close(t_wayland.xdg_toplevel_listener_data, t_wayland_xdg_toplevel);

	EXPECT_EQ(t_wayland_event.type, DISPLAY_EVENT_CLOSE);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_configure_emits_resize)
{
	START;

	t_wayland_reset();
	t_wayland_event_calls = 0;
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	display_set_event_callback(&display, t_wayland_event_cb, NULL);

	t_wayland.xdg_toplevel_listener->configure(t_wayland.xdg_toplevel_listener_data, t_wayland_xdg_toplevel, 800, 600, NULL);
	t_wayland.xdg_surface_listener->configure(t_wayland.xdg_surface_listener_data, t_wayland_xdg_surface, 10);

	EXPECT_EQ(t_wayland_event.type, DISPLAY_EVENT_RESIZE);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_configure_ignores_empty_size)
{
	START;

	t_wayland_reset();
	t_wayland_event_calls = 0;
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	display_set_event_callback(&display, t_wayland_event_cb, NULL);

	t_wayland.xdg_surface_listener->configure(t_wayland.xdg_surface_listener_data, t_wayland_xdg_surface, 10);

	EXPECT_EQ(t_wayland_event_calls, 0);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_configure_sets_width)
{
	START;

	t_wayland_reset();
	t_wayland_event_calls = 0;
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	display_set_event_callback(&display, t_wayland_event_cb, NULL);

	t_wayland.xdg_toplevel_listener->configure(t_wayland.xdg_toplevel_listener_data, t_wayland_xdg_toplevel, 800, 600, NULL);
	t_wayland.xdg_surface_listener->configure(t_wayland.xdg_surface_listener_data, t_wayland_xdg_surface, 10);

	EXPECT_EQ(t_wayland_event.width, 800);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

TEST(display_wayland_dynamic_configure_sets_height)
{
	START;

	t_wayland_reset();
	t_wayland_event_calls = 0;
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	t_wayland_env_init(&fs, &proc, &ss);
	t_wayland_open(&display, &window, &fs, &proc, &ss);
	display_set_event_callback(&display, t_wayland_event_cb, NULL);

	t_wayland.xdg_toplevel_listener->configure(t_wayland.xdg_toplevel_listener_data, t_wayland_xdg_toplevel, 800, 600, NULL);
	t_wayland.xdg_surface_listener->configure(t_wayland.xdg_surface_listener_data, t_wayland_xdg_surface, 10);

	EXPECT_EQ(t_wayland_event.height, 600);

	window_free(&window);
	display_free(&display);
	t_wayland_env_free(&fs, &proc, &ss);
	END;
}

STEST(display_wayland_dynamic)
{
	SSTART;

	RUN(display_wayland_dynamic_driver_is_registered);
	RUN(display_wayland_dynamic_driver_has_display_hooks);
	RUN(display_wayland_dynamic_driver_has_window_hooks);
	RUN(display_wayland_dynamic_driver_omits_position_hook);
	RUN(display_wayland_dynamic_driver_omits_borderless_hook);
	RUN(display_wayland_dynamic_driver_omits_size_hook);
	RUN(display_wayland_dynamic_available_rejects_missing_runtime);
	RUN(display_wayland_dynamic_available_accepts_display);
	RUN(display_wayland_dynamic_available_accepts_session);
	RUN(display_wayland_dynamic_init_null_display);
	RUN(display_wayland_dynamic_init_rejects_alloc_failure);
	RUN(display_wayland_dynamic_init_missing_symbol);
	RUN(display_wayland_dynamic_init_connects_display);
	RUN(display_wayland_dynamic_init_rejects_connect_failure);
	RUN(display_wayland_dynamic_init_gets_registry);
	RUN(display_wayland_dynamic_init_binds_compositor);
	RUN(display_wayland_dynamic_init_rejects_missing_globals);
	RUN(display_wayland_dynamic_init_rejects_missing_symbol_after_open);
	RUN(display_wayland_dynamic_init_rejects_missing_registry);
	RUN(display_wayland_dynamic_init_rejects_registry_listener_failure);
	RUN(display_wayland_dynamic_init_rejects_globals_roundtrip_failure);
	RUN(display_wayland_dynamic_init_rejects_compositor_bind_failure);
	RUN(display_wayland_dynamic_init_rejects_wm_base_bind_failure);
	RUN(display_wayland_dynamic_init_rejects_wm_base_listener_failure);
	RUN(display_wayland_dynamic_init_rejects_binding_roundtrip_failure);
	RUN(display_wayland_dynamic_free_disconnects_display);
	RUN(display_wayland_dynamic_free_rejects_invalid_display);
	RUN(display_wayland_dynamic_poll_events_rejects_invalid_display);
	RUN(display_wayland_dynamic_wait_events_rejects_invalid_display);
	RUN(display_wayland_dynamic_native_rejects_invalid_arguments);
	RUN(display_wayland_dynamic_native_returns_type);
	RUN(display_wayland_dynamic_native_returns_display);
	RUN(display_wayland_dynamic_window_init_creates_surface);
	RUN(display_wayland_dynamic_window_init_rejects_surface_failure);
	RUN(display_wayland_dynamic_window_init_rejects_invalid_arguments);
	RUN(display_wayland_dynamic_window_init_rejects_alloc_failure);
	RUN(display_wayland_dynamic_window_init_rejects_xdg_surface_failure);
	RUN(display_wayland_dynamic_window_init_rejects_xdg_surface_listener_failure);
	RUN(display_wayland_dynamic_window_init_rejects_xdg_toplevel_failure);
	RUN(display_wayland_dynamic_window_init_rejects_xdg_toplevel_listener_failure);
	RUN(display_wayland_dynamic_window_id_returns_surface_id);
	RUN(display_wayland_dynamic_window_free_rejects_invalid_window);
	RUN(display_wayland_dynamic_window_id_rejects_invalid_window);
	RUN(display_wayland_dynamic_window_id_rejects_missing_surface);
	RUN(display_wayland_dynamic_window_native_returns_type);
	RUN(display_wayland_dynamic_window_native_returns_surface);
	RUN(display_wayland_dynamic_window_native_rejects_invalid_arguments);
	RUN(display_wayland_dynamic_window_native_rejects_missing_surface);
	RUN(display_wayland_dynamic_window_set_title_sends_title);
	RUN(display_wayland_dynamic_window_set_title_rejects_missing_toplevel);
	RUN(display_wayland_dynamic_window_set_title_rejects_invalid_arguments);
	RUN(display_wayland_dynamic_window_set_title_rejects_long_title);
	RUN(display_wayland_dynamic_window_set_title_returns_flush_failure);
	RUN(display_wayland_dynamic_window_show_commits_surface);
	RUN(display_wayland_dynamic_window_show_recreates_role);
	RUN(display_wayland_dynamic_window_show_returns_flush_failure);
	RUN(display_wayland_dynamic_window_show_rejects_invalid_window);
	RUN(display_wayland_dynamic_window_show_rejects_role_failure);
	RUN(display_wayland_dynamic_window_hide_destroys_role);
	RUN(display_wayland_dynamic_window_hide_returns_flush_failure);
	RUN(display_wayland_dynamic_window_hide_rejects_invalid_window);
	RUN(display_wayland_dynamic_window_set_fullscreen_sends_set);
	RUN(display_wayland_dynamic_window_set_fullscreen_sends_unset);
	RUN(display_wayland_dynamic_window_set_fullscreen_rejects_missing_toplevel);
	RUN(display_wayland_dynamic_window_set_fullscreen_rejects_invalid_window);
	RUN(display_wayland_dynamic_window_set_fullscreen_returns_flush_failure);
	RUN(display_wayland_dynamic_poll_events_dispatches_pending);
	RUN(display_wayland_dynamic_wait_events_dispatches);
	RUN(display_wayland_dynamic_ping_sends_pong);
	RUN(display_wayland_dynamic_global_remove_ignores_global);
	RUN(display_wayland_dynamic_close_emits_event);
	RUN(display_wayland_dynamic_configure_emits_resize);
	RUN(display_wayland_dynamic_configure_ignores_empty_size);
	RUN(display_wayland_dynamic_configure_sets_width);
	RUN(display_wayland_dynamic_configure_sets_height);

	SEND;
}
