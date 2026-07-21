#include "display_driver.h"

#include "arr.h"
#include "log.h"
#include "mem.h"

#include <limits.h>

typedef struct wl_display wl_display;
typedef struct wl_proxy wl_proxy;
typedef struct wl_registry wl_registry;
typedef struct wl_compositor wl_compositor;
typedef struct wl_surface wl_surface;
typedef struct wl_output wl_output;
typedef struct xdg_wm_base xdg_wm_base;
typedef struct xdg_surface xdg_surface;
typedef struct xdg_toplevel xdg_toplevel;

typedef struct wl_message_s {
	const char *name;
	const char *signature;
	const struct wl_interface_s **types;
} wl_message;

typedef struct wl_interface_s {
	const char *name;
	int version;
	int method_count;
	const wl_message *methods;
	int event_count;
	const wl_message *events;
} wl_interface;

typedef struct wl_registry_listener_s {
	void (*global)(void *data, wl_registry *registry, u32 name, const char *interface, u32 version);
	void (*global_remove)(void *data, wl_registry *registry, u32 name);
} wl_registry_listener;

typedef struct xdg_wm_base_listener_s {
	void (*ping)(void *data, xdg_wm_base *xdg_wm_base, u32 serial);
} xdg_wm_base_listener;

typedef struct wl_output_listener_s {
	void (*geometry)(void *data, wl_output *output, s32 x, s32 y, s32 physical_width, s32 physical_height, s32 subpixel,
			 const char *make, const char *model, s32 transform);
	void (*mode)(void *data, wl_output *output, u32 flags, s32 width, s32 height, s32 refresh);
	void (*done)(void *data, wl_output *output);
	void (*scale)(void *data, wl_output *output, s32 factor);
} wl_output_listener;

typedef struct xdg_surface_listener_s {
	void (*configure)(void *data, xdg_surface *xdg_surface, u32 serial);
} xdg_surface_listener;

typedef struct xdg_toplevel_listener_s {
	void (*configure)(void *data, xdg_toplevel *xdg_toplevel, int width, int height, void *states);
	void (*close)(void *data, xdg_toplevel *xdg_toplevel);
} xdg_toplevel_listener;

typedef struct wayland_s {
	wl_display *(*display_connect)(const char *);
	void (*display_disconnect)(wl_display *);
	int (*display_roundtrip)(wl_display *);
	int (*display_dispatch)(wl_display *);
	int (*display_dispatch_pending)(wl_display *);
	int (*display_flush)(wl_display *);
	int (*proxy_add_listener)(wl_proxy *, void (**)(void), void *);
	wl_proxy *(*proxy_marshal_constructor_versioned)(wl_proxy *, u32, const wl_interface *, u32, ...);
	void (*proxy_marshal)(wl_proxy *, u32, ...);
	void (*proxy_destroy)(wl_proxy *);
	u32 (*proxy_get_id)(wl_proxy *);
} wayland_t;

typedef struct display_wayland_dynamic_s {
	proc_t *proc;
	void *lib;
	wayland_t wl;
	wl_display *display;
	wl_registry *registry;
	wl_compositor *compositor;
	xdg_wm_base *wm_base;
	u32 compositor_name;
	u32 compositor_version;
	u32 wm_base_name;
	u32 wm_base_version;
	arr_t outputs;
} display_wayland_dynamic_t;

typedef struct output_wayland_dynamic_s {
	wl_output *output;
	u32 name;
	u32 version;
	display_monitor_t monitor;
	int used;
} output_wayland_dynamic_t;

typedef struct window_wayland_dynamic_s {
	wl_surface *surface;
	xdg_surface *xdg_surface;
	xdg_toplevel *xdg_toplevel;
	char title[256];
	u16 width;
	u16 height;
	u16 pending_width;
	u16 pending_height;
	int fullscreen;
	int mapped;
} window_wayland_dynamic_t;

static const wl_interface wl_compositor_interface;
static const wl_interface wl_registry_interface;
static const wl_interface wl_surface_interface;
static const wl_interface wl_output_interface;
static const wl_interface xdg_wm_base_interface;
static const wl_interface xdg_surface_interface;
static const wl_interface xdg_toplevel_interface;

static const wl_interface *wl_registry_bind_types[] = {
	NULL,
	NULL,
	NULL,
};

static const wl_message wl_registry_methods[] = {
	{"bind", "usun", wl_registry_bind_types},
};

static const wl_interface *wl_registry_global_types[] = {
	NULL,
	NULL,
	NULL,
};

static const wl_interface *wl_registry_global_remove_types[] = {
	NULL,
};

static const wl_message wl_registry_events[] = {
	{"global", "usu", wl_registry_global_types},
	{"global_remove", "u", wl_registry_global_remove_types},
};

static const wl_interface wl_registry_interface = {
	.name	      = "wl_registry",
	.version      = 1,
	.method_count = sizeof(wl_registry_methods) / sizeof(wl_registry_methods[0]),
	.methods      = wl_registry_methods,
	.event_count  = sizeof(wl_registry_events) / sizeof(wl_registry_events[0]),
	.events	      = wl_registry_events,
};

static const wl_interface *wl_compositor_create_surface_types[] = {
	&wl_surface_interface,
};

static const wl_interface *wl_compositor_create_region_types[] = {
	NULL,
};

static const wl_message wl_compositor_methods[] = {
	{"create_surface", "n", wl_compositor_create_surface_types},
	{"create_region", "n", wl_compositor_create_region_types},
};

static const wl_interface wl_compositor_interface = {
	.name	      = "wl_compositor",
	.version      = 1,
	.method_count = sizeof(wl_compositor_methods) / sizeof(wl_compositor_methods[0]),
	.methods      = wl_compositor_methods,
};

static const wl_interface *wl_surface_frame_types[] = {
	NULL,
};

static const wl_message wl_surface_methods[] = {
	{"destroy", "", NULL},
	{"attach", "?oii", NULL},
	{"damage", "iiii", NULL},
	{"frame", "n", wl_surface_frame_types},
	{"set_opaque_region", "?o", NULL},
	{"set_input_region", "?o", NULL},
	{"commit", "", NULL},
};

static const wl_interface wl_surface_interface = {
	.name	      = "wl_surface",
	.version      = 1,
	.method_count = sizeof(wl_surface_methods) / sizeof(wl_surface_methods[0]),
	.methods      = wl_surface_methods,
};

static const wl_message wl_output_events[] = {
	{"geometry", "iiiiissi", NULL},
	{"mode", "uiii", NULL},
	{"done", "", NULL},
	{"scale", "i", NULL},
};

static const wl_interface wl_output_interface = {
	.name	     = "wl_output",
	.version     = 2,
	.event_count = sizeof(wl_output_events) / sizeof(wl_output_events[0]),
	.events	     = wl_output_events,
};

static const wl_interface *xdg_wm_base_create_positioner_types[] = {
	NULL,
};

static const wl_interface *xdg_wm_base_get_xdg_surface_types[] = {
	&xdg_surface_interface,
	&wl_surface_interface,
};

static const wl_interface *xdg_wm_base_pong_types[] = {
	NULL,
};

static const wl_message xdg_wm_base_methods[] = {
	{"destroy", "", NULL},
	{"create_positioner", "n", xdg_wm_base_create_positioner_types},
	{"get_xdg_surface", "no", xdg_wm_base_get_xdg_surface_types},
	{"pong", "u", xdg_wm_base_pong_types},
};

static const wl_interface *xdg_wm_base_ping_types[] = {
	NULL,
};

static const wl_message xdg_wm_base_events[] = {
	{"ping", "u", xdg_wm_base_ping_types},
};

static const wl_interface xdg_wm_base_interface = {
	.name	      = "xdg_wm_base",
	.version      = 1,
	.method_count = sizeof(xdg_wm_base_methods) / sizeof(xdg_wm_base_methods[0]),
	.methods      = xdg_wm_base_methods,
	.event_count  = sizeof(xdg_wm_base_events) / sizeof(xdg_wm_base_events[0]),
	.events	      = xdg_wm_base_events,
};

static const wl_interface *xdg_surface_get_toplevel_types[] = {
	&xdg_toplevel_interface,
};

static const wl_interface *xdg_surface_ack_configure_types[] = {
	NULL,
};

static const wl_message xdg_surface_methods[] = {
	{"destroy", "", NULL},
	{"get_toplevel", "n", xdg_surface_get_toplevel_types},
	{"get_popup", "n?oo", NULL},
	{"set_window_geometry", "iiii", NULL},
	{"ack_configure", "u", xdg_surface_ack_configure_types},
};

static const wl_interface *xdg_surface_configure_types[] = {
	NULL,
};

static const wl_message xdg_surface_events[] = {
	{"configure", "u", xdg_surface_configure_types},
};

static const wl_interface xdg_surface_interface = {
	.name	      = "xdg_surface",
	.version      = 1,
	.method_count = sizeof(xdg_surface_methods) / sizeof(xdg_surface_methods[0]),
	.methods      = xdg_surface_methods,
	.event_count  = sizeof(xdg_surface_events) / sizeof(xdg_surface_events[0]),
	.events	      = xdg_surface_events,
};

static const wl_interface *xdg_toplevel_set_parent_types[] = {
	&xdg_toplevel_interface,
};

static const wl_interface *xdg_toplevel_set_title_types[] = {
	NULL,
};

static const wl_interface *xdg_toplevel_set_app_id_types[] = {
	NULL,
};

static const wl_interface *xdg_toplevel_show_window_menu_types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
};

static const wl_interface *xdg_toplevel_move_types[] = {
	NULL,
	NULL,
};

static const wl_interface *xdg_toplevel_resize_types[] = {
	NULL,
	NULL,
	NULL,
};

static const wl_interface *xdg_toplevel_set_max_size_types[] = {
	NULL,
	NULL,
};

static const wl_interface *xdg_toplevel_set_min_size_types[] = {
	NULL,
	NULL,
};

static const wl_interface *xdg_toplevel_set_fullscreen_types[] = {
	&wl_output_interface,
};

static const wl_interface *xdg_toplevel_configure_types[] = {
	NULL,
	NULL,
	NULL,
};

static const wl_message xdg_toplevel_methods[] = {
	{"destroy", "", NULL},
	{"set_parent", "?o", xdg_toplevel_set_parent_types},
	{"set_title", "s", xdg_toplevel_set_title_types},
	{"set_app_id", "s", xdg_toplevel_set_app_id_types},
	{"show_window_menu", "ouii", xdg_toplevel_show_window_menu_types},
	{"move", "ou", xdg_toplevel_move_types},
	{"resize", "ouu", xdg_toplevel_resize_types},
	{"set_max_size", "ii", xdg_toplevel_set_max_size_types},
	{"set_min_size", "ii", xdg_toplevel_set_min_size_types},
	{"set_maximized", "", NULL},
	{"unset_maximized", "", NULL},
	{"set_fullscreen", "?o", xdg_toplevel_set_fullscreen_types},
	{"unset_fullscreen", "", NULL},
	{"set_minimized", "", NULL},
};

static const wl_message xdg_toplevel_events[] = {
	{"configure", "iia", xdg_toplevel_configure_types},
	{"close", "", NULL},
};

static const wl_interface xdg_toplevel_interface = {
	.name	      = "xdg_toplevel",
	.version      = 1,
	.method_count = sizeof(xdg_toplevel_methods) / sizeof(xdg_toplevel_methods[0]),
	.methods      = xdg_toplevel_methods,
	.event_count  = sizeof(xdg_toplevel_events) / sizeof(xdg_toplevel_events[0]),
	.events	      = xdg_toplevel_events,
};

enum {
	WL_DISPLAY_GET_REGISTRY	      = 1,
	WL_REGISTRY_BIND	      = 0,
	WL_SURFACE_DESTROY	      = 0,
	WL_SURFACE_COMMIT	      = 6,
	WL_COMPOSITOR_CREATE_SURFACE  = 0,
	XDG_WM_BASE_GET_XDG_SURFACE   = 2,
	XDG_WM_BASE_PONG	      = 3,
	XDG_SURFACE_DESTROY	      = 0,
	XDG_SURFACE_GET_TOPLEVEL      = 1,
	XDG_SURFACE_ACK_CONFIGURE     = 4,
	XDG_TOPLEVEL_DESTROY	      = 0,
	XDG_TOPLEVEL_SET_TITLE	      = 2,
	XDG_TOPLEVEL_SET_FULLSCREEN   = 11,
	XDG_TOPLEVEL_UNSET_FULLSCREEN = 12,
};

static u32 display_wayland_dynamic_window_id(window_t *wnd);

static int display_wayland_dynamic_available(display_driver_t *driver, proc_t *proc)
{
	(void)driver;
	if (proc == NULL || proc_getenv(proc, STRV("XDG_RUNTIME_DIR")).data == NULL) {
		return 0;
	}
	if (proc_getenv(proc, STRV("WAYLAND_DISPLAY")).data != NULL) {
		return 1;
	}

	return strv_eq(proc_getenv(proc, STRV("XDG_SESSION_TYPE")), STRV("wayland"));
}

static int load_symbol(display_wayland_dynamic_t *dwl, void **sym, strv_t name)
{
	if (proc_dlsym(dwl->proc, dwl->lib, name, sym)) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to load Wayland symbol: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

#define LOAD_WAYLAND(_dwl, _name)                                                                                                          \
	do {                                                                                                                               \
		if (load_symbol((_dwl), (void **)&(_dwl)->wl._name, STRV("wl_" #_name))) {                                                 \
			return 1;                                                                                                          \
		}                                                                                                                          \
	} while (0)

static int load_wayland(display_wayland_dynamic_t *dwl)
{
	if (proc_dlopen(dwl->proc, STRV("libwayland-client.so.0"), &dwl->lib) &&
	    proc_dlopen(dwl->proc, STRV("libwayland-client.so"), &dwl->lib)) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to load libwayland-client.so");
		return 1;
	}

	LOAD_WAYLAND(dwl, display_connect);
	LOAD_WAYLAND(dwl, display_disconnect);
	LOAD_WAYLAND(dwl, display_roundtrip);
	LOAD_WAYLAND(dwl, display_dispatch);
	LOAD_WAYLAND(dwl, display_dispatch_pending);
	LOAD_WAYLAND(dwl, display_flush);
	LOAD_WAYLAND(dwl, proxy_add_listener);
	LOAD_WAYLAND(dwl, proxy_marshal_constructor_versioned);
	LOAD_WAYLAND(dwl, proxy_marshal);
	LOAD_WAYLAND(dwl, proxy_destroy);
	LOAD_WAYLAND(dwl, proxy_get_id);

	return 0;
}

#undef LOAD_WAYLAND

static u32 bind_version(u32 compositor_version, u32 supported_version)
{
	return compositor_version < supported_version ? compositor_version : supported_version;
}

static int str_eq(const char *l, const char *r)
{
	return l != NULL && r != NULL && strv_eq(strv_cstr(l), strv_cstr(r));
}

static void wayland_output_name(display_monitor_t *monitor, const char *make, const char *model)
{
	size_t len = 0;

	if (make != NULL) {
		while (len + 1 < sizeof(monitor->name) && make[len] != 0) {
			monitor->name[len] = make[len];
			len++;
		}
	}
	if (model != NULL && model[0] != 0) {
		if (len + 1 < sizeof(monitor->name) && len > 0) {
			monitor->name[len++] = ' ';
		}
		for (size_t i = 0; len + 1 < sizeof(monitor->name) && model[i] != 0; ++i) {
			monitor->name[len++] = model[i];
		}
	}
	monitor->name[len] = 0;
}

static void wayland_output_geometry(void *data, wl_output *output, s32 x, s32 y, s32 physical_width, s32 physical_height, s32 subpixel,
				    const char *make, const char *model, s32 transform)
{
	(void)output;
	(void)subpixel;
	(void)transform;
	output_wayland_dynamic_t *out = data;

	out->monitor.x = x;
	out->monitor.y = y;
	if (physical_width > 0) {
		out->monitor.physical_width = (u32)physical_width;
	}
	if (physical_height > 0) {
		out->monitor.physical_height = (u32)physical_height;
	}
	wayland_output_name(&out->monitor, make, model);
}

static void wayland_output_mode(void *data, wl_output *output, u32 flags, s32 width, s32 height, s32 refresh)
{
	(void)output;
	output_wayland_dynamic_t *out = data;

	if ((flags & 1u) == 0) {
		return;
	}
	if (width > 0) {
		out->monitor.width = (u32)width;
	}
	if (height > 0) {
		out->monitor.height = (u32)height;
	}
	if (refresh > 0) {
		out->monitor.refresh_rate = (u32)((refresh + 500) / 1000);
	}
}

static void wayland_output_done(void *data, wl_output *output)
{
	(void)data;
	(void)output;
}

static void wayland_output_scale(void *data, wl_output *output, s32 factor)
{
	(void)output;
	output_wayland_dynamic_t *out = data;
	if (factor > 0) {
		out->monitor.scale = (u32)factor * 100;
	}
}

static const wl_output_listener wayland_output_listener = {
	.geometry = wayland_output_geometry,
	.mode	  = wayland_output_mode,
	.done	  = wayland_output_done,
	.scale	  = wayland_output_scale,
};

static void wayland_registry_global(void *data, wl_registry *registry, u32 name, const char *interface, u32 version)
{
	display_wayland_dynamic_t *dwl = data;
	if (str_eq(interface, wl_output_interface.name)) {
		output_wayland_dynamic_t *out = arr_add(&dwl->outputs, NULL);
		if (out == NULL) {
			log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to allocate Wayland output");
			return;
		}
		mem_set(out, 0, sizeof(*out));
		out->name	     = name;
		out->version	     = version;
		out->used	     = 1;
		out->monitor.id	     = (u32)dwl->outputs.cnt - 1;
		out->monitor.scale   = 100;
		out->monitor.primary = out->monitor.id == 0;
		out->output = (wl_output *)dwl->wl.proxy_marshal_constructor_versioned((wl_proxy *)registry,
										       WL_REGISTRY_BIND,
										       &wl_output_interface,
										       bind_version(version, wl_output_interface.version),
										       name,
										       wl_output_interface.name,
										       bind_version(version, wl_output_interface.version),
										       NULL);
		if (out->output == NULL ||
		    dwl->wl.proxy_add_listener((wl_proxy *)out->output, (void (**)(void))&wayland_output_listener, out)) {
			log_error("cdisplay",
				  "display_wayland_dynamic",
				  NULL,
				  "failed to bind Wayland output: name=%u version=%u",
				  name,
				  version);
			if (out->output != NULL) {
				dwl->wl.proxy_destroy((wl_proxy *)out->output);
				out->output = NULL;
			}
			out->used = 0;
			return;
		}
		out->monitor.native = out->output;
		log_info("cdisplay",
			 "display_wayland_dynamic",
			 NULL,
			 "found Wayland global: interface=%s name=%u version=%u",
			 interface,
			 name,
			 version);
		return;
	}
	if (str_eq(interface, wl_compositor_interface.name)) {
		dwl->compositor_name	= name;
		dwl->compositor_version = version;
		log_info("cdisplay",
			 "display_wayland_dynamic",
			 NULL,
			 "found Wayland global: interface=%s name=%u version=%u",
			 interface,
			 name,
			 version);
		return;
	}
	if (str_eq(interface, xdg_wm_base_interface.name)) {
		dwl->wm_base_name    = name;
		dwl->wm_base_version = version;
		log_info("cdisplay",
			 "display_wayland_dynamic",
			 NULL,
			 "found Wayland global: interface=%s name=%u version=%u",
			 interface,
			 name,
			 version);
	}
}

static void wayland_registry_global_remove(void *data, wl_registry *registry, u32 name)
{
	(void)registry;
	display_wayland_dynamic_t *dwl = data;
	for (u32 i = 0; i < dwl->outputs.cnt; ++i) {
		output_wayland_dynamic_t *out = arr_get(&dwl->outputs, i);
		if (out != NULL && out->used && out->name == name) {
			if (out->output != NULL) {
				dwl->wl.proxy_destroy((wl_proxy *)out->output);
			}
			out->used = 0;
			return;
		}
	}
}

static const wl_registry_listener wayland_registry_listener = {
	.global	       = wayland_registry_global,
	.global_remove = wayland_registry_global_remove,
};

static void wayland_wm_base_ping(void *data, xdg_wm_base *wm_base, u32 serial)
{
	display_wayland_dynamic_t *dwl = data;
	dwl->wl.proxy_marshal((wl_proxy *)wm_base, XDG_WM_BASE_PONG, serial);
}

static const xdg_wm_base_listener wayland_wm_base_listener = {
	.ping = wayland_wm_base_ping,
};

static void wayland_xdg_surface_configure(void *data, xdg_surface *xdg_surface, u32 serial)
{
	(void)xdg_surface;
	window_t *wnd			   = data;
	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;

	dwl->wl.proxy_marshal((wl_proxy *)wwayland->xdg_surface, XDG_SURFACE_ACK_CONFIGURE, serial);
	if (wwayland->pending_width == 0 || wwayland->pending_height == 0) {
		return;
	}

	wwayland->width	 = wwayland->pending_width;
	wwayland->height = wwayland->pending_height;
	display_emit_event(wnd->display,
			   &(display_event_t){
				   .type   = DISPLAY_EVENT_RESIZE,
				   .window = display_wayland_dynamic_window_id(wnd),
				   .width  = wwayland->width,
				   .height = wwayland->height,
			   });
}

static const xdg_surface_listener wayland_xdg_surface_listener = {
	.configure = wayland_xdg_surface_configure,
};

static void wayland_xdg_toplevel_configure(void *data, xdg_toplevel *xdg_toplevel, int width, int height, void *states)
{
	(void)xdg_toplevel;
	(void)states;
	window_t *wnd			   = data;
	window_wayland_dynamic_t *wwayland = wnd->data;

	if (width > 0 && width <= UINT16_MAX) {
		wwayland->pending_width = (u16)width;
	}
	if (height > 0 && height <= UINT16_MAX) {
		wwayland->pending_height = (u16)height;
	}
}

static void wayland_xdg_toplevel_close(void *data, xdg_toplevel *xdg_toplevel)
{
	(void)xdg_toplevel;
	window_t *wnd = data;
	display_emit_event(wnd->display,
			   &(display_event_t){
				   .type   = DISPLAY_EVENT_CLOSE,
				   .window = display_wayland_dynamic_window_id(wnd),
			   });
}

static const xdg_toplevel_listener wayland_xdg_toplevel_listener = {
	.configure = wayland_xdg_toplevel_configure,
	.close	   = wayland_xdg_toplevel_close,
};

static wl_proxy *wayland_bind(display_wayland_dynamic_t *dwl, u32 name, const wl_interface *interface, u32 version)
{
	return dwl->wl.proxy_marshal_constructor_versioned(
		(wl_proxy *)dwl->registry, WL_REGISTRY_BIND, interface, version, name, interface->name, version, NULL);
}

static int bind_globals(display_wayland_dynamic_t *dwl)
{
	dwl->registry = (wl_registry *)dwl->wl.proxy_marshal_constructor_versioned(
		(wl_proxy *)dwl->display, WL_DISPLAY_GET_REGISTRY, &wl_registry_interface, wl_registry_interface.version);
	if (dwl->registry == NULL) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to get Wayland registry");
		return 1;
	}
	if (dwl->wl.proxy_add_listener((wl_proxy *)dwl->registry, (void (**)(void))&wayland_registry_listener, dwl)) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to listen to Wayland registry");
		return 1;
	}
	if (dwl->wl.display_roundtrip(dwl->display) < 0) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to receive Wayland globals");
		return 1;
	}

	if (dwl->compositor_name == 0 || dwl->wm_base_name == 0) {
		log_error("cdisplay",
			  "display_wayland_dynamic",
			  NULL,
			  "required Wayland globals are unavailable: wl_compositor=%u xdg_wm_base=%u",
			  dwl->compositor_name,
			  dwl->wm_base_name);
		return 1;
	}

	dwl->compositor = (wl_compositor *)wayland_bind(dwl,
							dwl->compositor_name,
							&wl_compositor_interface,
							bind_version(dwl->compositor_version, wl_compositor_interface.version));
	if (dwl->compositor == NULL) {
		log_error("cdisplay",
			  "display_wayland_dynamic",
			  NULL,
			  "failed to bind Wayland global: interface=%s name=%u version=%u",
			  wl_compositor_interface.name,
			  dwl->compositor_name,
			  bind_version(dwl->compositor_version, wl_compositor_interface.version));
		return 1;
	}

	dwl->wm_base = (xdg_wm_base *)wayland_bind(
		dwl, dwl->wm_base_name, &xdg_wm_base_interface, bind_version(dwl->wm_base_version, xdg_wm_base_interface.version));
	if (dwl->wm_base == NULL) {
		log_error("cdisplay",
			  "display_wayland_dynamic",
			  NULL,
			  "failed to bind Wayland global: interface=%s name=%u version=%u",
			  xdg_wm_base_interface.name,
			  dwl->wm_base_name,
			  bind_version(dwl->wm_base_version, xdg_wm_base_interface.version));
		return 1;
	}
	if (dwl->wl.proxy_add_listener((wl_proxy *)dwl->wm_base, (void (**)(void))&wayland_wm_base_listener, dwl)) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to listen to xdg_wm_base");
		return 1;
	}

	if (dwl->wl.display_roundtrip(dwl->display) < 0) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to finish Wayland global binding");
		return 1;
	}

	return 0;
}

static int display_wayland_dynamic_init(display_t *display)
{
	if (display == NULL || display->alloc.alloc == NULL) {
		return 1;
	}

	log_info("cdisplay", "display_wayland_dynamic", NULL, "Initializing Wayland...");

	display->data = alloc_alloc(&display->alloc, sizeof(display_wayland_dynamic_t));
	if (display->data == NULL) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to allocate display data");
		return 1;
	}
	mem_set(display->data, 0, sizeof(display_wayland_dynamic_t));

	display_wayland_dynamic_t *dwl = display->data;
	dwl->proc		       = display->proc;
	if (arr_init(&dwl->outputs, 4, sizeof(output_wayland_dynamic_t), display->alloc) == NULL || load_wayland(dwl)) {
		if (dwl->lib != NULL) {
			proc_dlclose(dwl->proc, dwl->lib);
		}
		arr_free(&dwl->outputs);
		alloc_free(&display->alloc, display->data, sizeof(display_wayland_dynamic_t));
		display->data = NULL;
		return 1;
	}

	dwl->display = dwl->wl.display_connect(NULL);
	if (dwl->display == NULL) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to connect to Wayland display");
		proc_dlclose(dwl->proc, dwl->lib);
		arr_free(&dwl->outputs);
		alloc_free(&display->alloc, display->data, sizeof(display_wayland_dynamic_t));
		display->data = NULL;
		return 1;
	}

	if (bind_globals(dwl)) {
		for (u32 i = 0; i < dwl->outputs.cnt; ++i) {
			output_wayland_dynamic_t *out = arr_get(&dwl->outputs, i);
			if (out != NULL && out->used && out->output != NULL) {
				dwl->wl.proxy_destroy((wl_proxy *)out->output);
			}
		}
		if (dwl->wm_base != NULL) {
			dwl->wl.proxy_destroy((wl_proxy *)dwl->wm_base);
		}
		if (dwl->compositor != NULL) {
			dwl->wl.proxy_destroy((wl_proxy *)dwl->compositor);
		}
		if (dwl->registry != NULL) {
			dwl->wl.proxy_destroy((wl_proxy *)dwl->registry);
		}
		dwl->wl.display_disconnect(dwl->display);
		proc_dlclose(dwl->proc, dwl->lib);
		arr_free(&dwl->outputs);
		alloc_free(&display->alloc, display->data, sizeof(display_wayland_dynamic_t));
		display->data = NULL;
		return 1;
	}

	return 0;
}

static int display_wayland_dynamic_free(display_t *display)
{
	if (display == NULL || display->data == NULL) {
		return 1;
	}

	display_wayland_dynamic_t *dwl = display->data;

	log_info("cdisplay", "display_wayland_dynamic", NULL, "Freeing Wayland...");

	for (u32 i = 0; i < dwl->outputs.cnt; ++i) {
		output_wayland_dynamic_t *out = arr_get(&dwl->outputs, i);
		if (out != NULL && out->used && out->output != NULL) {
			dwl->wl.proxy_destroy((wl_proxy *)out->output);
		}
	}
	if (dwl->wm_base != NULL) {
		dwl->wl.proxy_destroy((wl_proxy *)dwl->wm_base);
	}
	if (dwl->compositor != NULL) {
		dwl->wl.proxy_destroy((wl_proxy *)dwl->compositor);
	}
	if (dwl->registry != NULL) {
		dwl->wl.proxy_destroy((wl_proxy *)dwl->registry);
	}
	if (dwl->display != NULL) {
		dwl->wl.display_disconnect(dwl->display);
	}
	if (dwl->lib != NULL) {
		proc_dlclose(dwl->proc, dwl->lib);
	}

	arr_free(&dwl->outputs);
	alloc_free(&display->alloc, display->data, sizeof(display_wayland_dynamic_t));
	return 0;
}

static int display_wayland_dynamic_poll_events(display_t *display)
{
	if (display == NULL || display->data == NULL) {
		return 1;
	}

	display_wayland_dynamic_t *dwl = display->data;
	return dwl->wl.display_dispatch_pending(dwl->display) < 0 || dwl->wl.display_flush(dwl->display) < 0;
}

static int display_wayland_dynamic_wait_events(display_t *display)
{
	if (display == NULL || display->data == NULL) {
		return 1;
	}

	display_wayland_dynamic_t *dwl = display->data;
	return dwl->wl.display_dispatch(dwl->display) < 0;
}

static int display_wayland_dynamic_native(display_t *display, display_native_t *native)
{
	if (display == NULL || display->data == NULL || native == NULL) {
		return 1;
	}

	display_wayland_dynamic_t *dwl = display->data;
	native->type		       = DISPLAY_NATIVE_WAYLAND;
	native->display		       = dwl->display;
	native->screen		       = 0;
	return native->display == NULL;
}

static u32 display_wayland_dynamic_monitor_count(display_t *display)
{
	display_wayland_dynamic_t *dwl = display->data;
	u32 count		       = 0;
	for (u32 i = 0; i < dwl->outputs.cnt; ++i) {
		output_wayland_dynamic_t *out = arr_get(&dwl->outputs, i);
		if (out != NULL && out->used) {
			count++;
		}
	}

	return count;
}

static int display_wayland_dynamic_monitors(display_t *display, arr_t *monitors)
{
	if (display == NULL || display->data == NULL || monitors == NULL) {
		return 1;
	}

	display_wayland_dynamic_t *dwl = display->data;
	u32 count		       = display_wayland_dynamic_monitor_count(display);
	if (arr_resize(monitors, count)) {
		return 1;
	}
	monitors->cnt = count;

	count = 0;
	for (u32 i = 0; i < dwl->outputs.cnt; ++i) {
		output_wayland_dynamic_t *out = arr_get(&dwl->outputs, i);
		if (out != NULL && out->used) {
			display_monitor_t *monitor = arr_get(monitors, count++);
			*monitor		   = out->monitor;
		}
	}

	return 0;
}

static int create_xdg_toplevel(window_t *wnd)
{
	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;

	wwayland->xdg_surface = (xdg_surface *)dwl->wl.proxy_marshal_constructor_versioned((wl_proxy *)dwl->wm_base,
											   XDG_WM_BASE_GET_XDG_SURFACE,
											   &xdg_surface_interface,
											   xdg_surface_interface.version,
											   wwayland->surface);
	if (wwayland->xdg_surface == NULL) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to create xdg_surface");
		return 1;
	}
	if (dwl->wl.proxy_add_listener((wl_proxy *)wwayland->xdg_surface, (void (**)(void))&wayland_xdg_surface_listener, wnd)) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to listen to xdg_surface");
		return 1;
	}

	wwayland->xdg_toplevel = (xdg_toplevel *)dwl->wl.proxy_marshal_constructor_versioned(
		(wl_proxy *)wwayland->xdg_surface, XDG_SURFACE_GET_TOPLEVEL, &xdg_toplevel_interface, xdg_toplevel_interface.version);
	if (wwayland->xdg_toplevel == NULL) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to create xdg_toplevel");
		return 1;
	}
	if (dwl->wl.proxy_add_listener((wl_proxy *)wwayland->xdg_toplevel, (void (**)(void))&wayland_xdg_toplevel_listener, wnd)) {
		log_error("cdisplay", "display_wayland_dynamic", NULL, "failed to listen to xdg_toplevel");
		return 1;
	}

	return 0;
}

static void destroy_xdg_toplevel(window_t *wnd)
{
	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;

	if (wwayland->xdg_toplevel != NULL) {
		dwl->wl.proxy_marshal((wl_proxy *)wwayland->xdg_toplevel, XDG_TOPLEVEL_DESTROY);
		dwl->wl.proxy_destroy((wl_proxy *)wwayland->xdg_toplevel);
		wwayland->xdg_toplevel = NULL;
	}
	if (wwayland->xdg_surface != NULL) {
		dwl->wl.proxy_marshal((wl_proxy *)wwayland->xdg_surface, XDG_SURFACE_DESTROY);
		dwl->wl.proxy_destroy((wl_proxy *)wwayland->xdg_surface);
		wwayland->xdg_surface = NULL;
	}
}

static int display_wayland_dynamic_window_init(window_t *wnd, const window_config_t *config)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->display->alloc.alloc == NULL || config == NULL) {
		return 1;
	}

	wnd->data = alloc_alloc(&wnd->display->alloc, sizeof(window_wayland_dynamic_t));
	if (wnd->data == NULL) {
		return 1;
	}
	mem_set(wnd->data, 0, sizeof(window_wayland_dynamic_t));

	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;
	wwayland->width			   = config->width;
	wwayland->height		   = config->height;
	wwayland->surface		   = (wl_surface *)dwl->wl.proxy_marshal_constructor_versioned(
		(wl_proxy *)dwl->compositor, WL_COMPOSITOR_CREATE_SURFACE, &wl_surface_interface, wl_surface_interface.version);
	if (wwayland->surface == NULL || create_xdg_toplevel(wnd)) {
		destroy_xdg_toplevel(wnd);
		if (wwayland->surface != NULL) {
			dwl->wl.proxy_marshal((wl_proxy *)wwayland->surface, WL_SURFACE_DESTROY);
			dwl->wl.proxy_destroy((wl_proxy *)wwayland->surface);
		}
		alloc_free(&wnd->display->alloc, wnd->data, sizeof(window_wayland_dynamic_t));
		wnd->data = NULL;
		return 1;
	}

	return 0;
}

static int display_wayland_dynamic_window_free(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;

	destroy_xdg_toplevel(wnd);
	if (wwayland->surface != NULL) {
		dwl->wl.proxy_marshal((wl_proxy *)wwayland->surface, WL_SURFACE_DESTROY);
		dwl->wl.proxy_destroy((wl_proxy *)wwayland->surface);
	}
	dwl->wl.display_flush(dwl->display);

	alloc_free(&wnd->display->alloc, wnd->data, sizeof(window_wayland_dynamic_t));
	return 0;
}

static u32 display_wayland_dynamic_window_id(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 0;
	}

	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;
	if (wwayland->surface == NULL) {
		return 0;
	}
	return dwl->wl.proxy_get_id((wl_proxy *)wwayland->surface);
}

static int display_wayland_dynamic_window_native(window_t *wnd, window_native_t *native)
{
	if (wnd == NULL || wnd->data == NULL || native == NULL) {
		return 1;
	}

	window_wayland_dynamic_t *wwayland = wnd->data;
	native->type			   = DISPLAY_NATIVE_WAYLAND;
	native->window			   = wwayland->surface;
	return native->window == NULL;
}

static int display_wayland_dynamic_window_set_title(window_t *wnd, strv_t title)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL || title.len > INT_MAX ||
	    (title.data == NULL && title.len > 0)) {
		return 1;
	}

	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;
	if (wwayland->xdg_toplevel == NULL) {
		return 1;
	}

	char buf[256] = {0};
	if (title.len >= sizeof(buf)) {
		return 1;
	}
	mem_copy(buf, sizeof(buf), title.data, title.len);

	dwl->wl.proxy_marshal((wl_proxy *)wwayland->xdg_toplevel, XDG_TOPLEVEL_SET_TITLE, buf);
	if (dwl->wl.display_flush(dwl->display) < 0) {
		return 1;
	}

	if (title.len > 0) {
		mem_copy(wwayland->title, sizeof(wwayland->title), title.data, title.len);
	}
	wwayland->title[title.len] = 0;
	return 0;
}

static int display_wayland_dynamic_window_get_title(window_t *wnd, char *title, size_t size)
{
	if (wnd == NULL || wnd->data == NULL || title == NULL || size == 0) {
		return 1;
	}

	window_wayland_dynamic_t *wwayland = wnd->data;
	size_t len			   = 0;
	while (len < sizeof(wwayland->title) && wwayland->title[len] != 0) {
		len++;
	}
	if (len >= size) {
		return 1;
	}

	mem_copy(title, size, wwayland->title, len + 1);
	return 0;
}

static int display_wayland_dynamic_window_set_fullscreen(window_t *wnd, int fullscreen)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;
	if (wwayland->xdg_toplevel == NULL) {
		return 1;
	}

	if (fullscreen) {
		dwl->wl.proxy_marshal((wl_proxy *)wwayland->xdg_toplevel, XDG_TOPLEVEL_SET_FULLSCREEN, NULL);
	} else {
		dwl->wl.proxy_marshal((wl_proxy *)wwayland->xdg_toplevel, XDG_TOPLEVEL_UNSET_FULLSCREEN);
	}
	if (dwl->wl.display_flush(dwl->display) < 0) {
		return 1;
	}

	wwayland->fullscreen = fullscreen != 0;
	return 0;
}

static int display_wayland_dynamic_window_get_fullscreen(window_t *wnd, int *fullscreen)
{
	if (wnd == NULL || wnd->data == NULL || fullscreen == NULL) {
		return 1;
	}

	window_wayland_dynamic_t *wwayland = wnd->data;
	*fullscreen			   = wwayland->fullscreen;
	return 0;
}

static int display_wayland_dynamic_window_show(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;
	if (wwayland->xdg_toplevel == NULL && create_xdg_toplevel(wnd)) {
		return 1;
	}

	dwl->wl.proxy_marshal((wl_proxy *)wwayland->surface, WL_SURFACE_COMMIT);
	if (dwl->wl.display_flush(dwl->display) < 0) {
		return 1;
	}

	wwayland->mapped = 1;
	return 0;
}

static int display_wayland_dynamic_window_hide(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_wayland_dynamic_t *dwl	   = wnd->display->data;
	window_wayland_dynamic_t *wwayland = wnd->data;

	destroy_xdg_toplevel(wnd);
	dwl->wl.proxy_marshal((wl_proxy *)wwayland->surface, WL_SURFACE_COMMIT);
	if (dwl->wl.display_flush(dwl->display) < 0) {
		return 1;
	}

	wwayland->mapped = 0;
	return 0;
}

static display_driver_t display_wayland_dynamic = {
	.name		       = "Wayland-dynamic",
	.available	       = display_wayland_dynamic_available,
	.init		       = display_wayland_dynamic_init,
	.free		       = display_wayland_dynamic_free,
	.poll_events	       = display_wayland_dynamic_poll_events,
	.wait_events	       = display_wayland_dynamic_wait_events,
	.native		       = display_wayland_dynamic_native,
	.monitors	       = display_wayland_dynamic_monitors,
	.window_init	       = display_wayland_dynamic_window_init,
	.window_free	       = display_wayland_dynamic_window_free,
	.window_id	       = display_wayland_dynamic_window_id,
	.window_native	       = display_wayland_dynamic_window_native,
	.window_set_title      = display_wayland_dynamic_window_set_title,
	.window_get_title      = display_wayland_dynamic_window_get_title,
	.window_set_fullscreen = display_wayland_dynamic_window_set_fullscreen,
	.window_get_fullscreen = display_wayland_dynamic_window_get_fullscreen,
	.window_show	       = display_wayland_dynamic_window_show,
	.window_hide	       = display_wayland_dynamic_window_hide,
};

DISPLAY_DRIVER(display_wayland_dynamic, &display_wayland_dynamic);
