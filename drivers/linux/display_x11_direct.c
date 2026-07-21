#include "display_driver.h"
#include "display_x11_priv.h"

#include "arr.h"
#include "buf.h"
#include "cbuf.h"
#include "cerr.h"
#include "log.h"
#include "mem.h"

typedef struct visual_x11_s {
	u32 id;
	u8 depth;
} visual_x11_t;

typedef struct display_x11_direct_s {
	void *sock;
	u32 resource_id_base;
	u32 resource_id_mask;
	u32 next_resource;
	u32 root;
	u32 white_pixel;
	u32 black_pixel;
	visual_x11_t *visuals;
	size_t visual_count;
	display_monitor_t *monitors;
	size_t monitor_count;
	buf_t request;
	u32 wm_protocols;
	u32 wm_delete_window;
	u32 wm_name;
	u32 net_wm_name;
	u32 utf8_string;
	u32 motif_wm_hints;
	u32 net_wm_state;
	u32 net_wm_state_fullscreen;
	u8 min_keycode;
	u8 max_keycode;
	display_key_t keys[256];
	display_modifier_t modifiers[8];
	u8 event_data[32];
	size_t event_used;
	arr_t windows;
} display_x11_direct_t;

typedef struct window_x11_s {
	u32 id;
	u32 colormap;
	char title[256];
	u16 x;
	u16 y;
	u16 width;
	u16 height;
	int borderless;
	int fullscreen;
	int mapped;
} window_x11_t;

typedef struct window_x11_slot_s {
	int used;
	window_x11_t window;
} window_x11_slot_t;

static window_x11_t *display_x11_direct_window_data(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return NULL;
	}

	display_x11_direct_t *dx11 = wnd->display->data;
	uint id			   = (uint)(uintptr_t)wnd->data - 1;
	window_x11_slot_t *slot	   = arr_get(&dx11->windows, id);
	if (slot == NULL || !slot->used) {
		return NULL;
	}

	return &slot->window;
}

static window_x11_t *display_x11_direct_window_alloc(window_t *wnd)
{
	display_x11_direct_t *dx11 = wnd->display->data;
	window_x11_slot_t *slot	   = NULL;
	uint id			   = 0;

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

static void display_x11_direct_window_release(window_t *wnd)
{
	display_x11_direct_t *dx11 = wnd->display->data;
	uint id			   = (uint)(uintptr_t)wnd->data - 1;
	window_x11_slot_t *slot	   = arr_get(&dx11->windows, id);
	if (slot != NULL) {
		mem_set(slot, 0, sizeof(*slot));
	}
	wnd->data = NULL;
}

enum {
	X11_QUERY_EXTENSION = 98,
};

enum {
	X_RANDR_GET_MONITORS = 42,
};

enum {
	X11_PROTOCOL_MAJOR = 11,
	X11_PROTOCOL_MINOR = 0,
	X11_REPLY_SUCCESS  = 1,
};

enum {
	X11_SETUP_RESOURCE_ID_BASE_OFFSET = 4,
	X11_SETUP_PROTOCOL_MAJOR_OFFSET	  = 2,
	X11_SETUP_VENDOR_LENGTH_OFFSET	  = 16,
	X11_SETUP_SCREEN_COUNT_OFFSET	  = 20,
	X11_SETUP_FORMAT_COUNT_OFFSET	  = 21,
	X11_SETUP_MIN_KEYCODE_OFFSET	  = 26,
	X11_SETUP_MAX_KEYCODE_OFFSET	  = 27,
	X11_SETUP_SCREEN_LIST_OFFSET	  = 32,
	X11_SCREEN_DEFAULT_COLORMAP_SIZE  = 4,
	X11_SCREEN_WIDTH_OFFSET		  = 20,
	X11_SCREEN_HEIGHT_OFFSET	  = 22,
	X11_SCREEN_PHYSICAL_WIDTH_OFFSET  = 24,
	X11_SCREEN_PHYSICAL_HEIGHT_OFFSET = 26,
	X11_SCREEN_DEPTH_COUNT_OFFSET	  = 39,
	X11_SCREEN_DEPTHS_OFFSET	  = 40,
	X11_DEPTH_SIZE			  = 8,
	X11_VISUAL_SIZE			  = 24,
};

enum {
	XAUTH_FAMILY_LOCAL = 256,
	XAUTH_FAMILY_WILD  = 65535,
};

enum {
	XAUTH_SCORE_NO_MATCH   = -1,
	XAUTH_SCORE_WILD       = 10,
	XAUTH_SCORE_LOCAL      = 50,
	XAUTH_SCORE_LOCAL_HOST = 100,
};

static int buf_read_blob(const buf_t *buf, u8 const **data, u16 *length, size_t *off)
{
	if (buf_read_u16be(buf, off, length)) {
		return 1;
	}

	*data = buf_read(buf, *length, off);
	return *data == NULL;
}

static int bytes_eq(const void *l, size_t l_size, const void *r, size_t r_size)
{
	return l_size == r_size && (l_size == 0 || mem_cmp(l, r, l_size) == 0);
}

/*static int discard_all(int fd, size_t length)
{
	u8 buf[1024] = {0};

	while (length > 0) {
		size_t n = length < sizeof(buf) ? length : sizeof(buf);
		if (read_all(fd, buf, n)) {
			return 1;
		}
		length -= n;
	}

	return 0;
}*/

static void get_atom_name(display_t *display, u32 atom, char *name, size_t name_size);

static int read_visuals(display_t *display, const buf_t *setup, size_t screen_offset)
{
	display_x11_direct_t *dx11 = display->data;
	size_t off		   = screen_offset + X11_SCREEN_DEPTH_COUNT_OFFSET;
	u8 depth_count		   = ((const u8 *)setup->data)[off];

	off		    = screen_offset + X11_SCREEN_DEPTHS_OFFSET;
	size_t visual_count = 0;
	for (u8 i = 0; i < depth_count; ++i) {
		if (off + X11_DEPTH_SIZE > setup->used) {
			return 1;
		}
		size_t count_off = off + 2;
		u16 count;
		buf_read_u16le(setup, &count_off, &count);
		off += X11_DEPTH_SIZE + (size_t)count * X11_VISUAL_SIZE;
		if (off > setup->used) {
			return 1;
		}
		visual_count += count;
	}

	if (visual_count == 0) {
		return 0;
	}

	dx11->visuals = alloc_alloc(&display->alloc, visual_count * sizeof(*dx11->visuals));
	if (dx11->visuals == NULL) {
		return 1;
	}
	dx11->visual_count = visual_count;

	off	     = screen_offset + X11_SCREEN_DEPTHS_OFFSET;
	size_t index = 0;
	for (u8 i = 0; i < depth_count; ++i) {
		u8 depth;
		buf_read_u8le(setup, &off, &depth);
		off++;
		u16 count;
		buf_read_u16le(setup, &off, &count);
		off += 4;
		for (u16 j = 0; j < count; ++j) {
			buf_read_u32le(setup, &off, &dx11->visuals[index].id);
			dx11->visuals[index].depth = depth;
			index++;
			off += X11_VISUAL_SIZE - sizeof(u32);
		}
	}

	return 0;
}

static size_t screen_size(const buf_t *setup, size_t screen_offset)
{
	size_t off     = screen_offset + X11_SCREEN_DEPTH_COUNT_OFFSET;
	u8 depth_count = ((const u8 *)setup->data)[off];

	off = screen_offset + X11_SCREEN_DEPTHS_OFFSET;
	for (u8 i = 0; i < depth_count; ++i) {
		u16 visual_count;
		size_t depth_offset = off;
		off += 2;
		(void)buf_read_u16le(setup, &off, &visual_count);
		off = depth_offset + 8 + (size_t)visual_count * 24;
	}

	return off - screen_offset;
}

static int read_screens(display_t *display, const buf_t *setup, size_t screen_offset, u8 screen_count)
{
	display_x11_direct_t *dx11 = display->data;

	dx11->monitors = alloc_alloc(&display->alloc, (size_t)screen_count * sizeof(*dx11->monitors));
	if (dx11->monitors == NULL) {
		return 1;
	}
	dx11->monitor_count = screen_count;

	for (u8 i = 0; i < screen_count; ++i) {
		size_t off = screen_offset;
		u32 root   = 0;
		(void)buf_read_u32le(setup, &off, &root);
		off		    = screen_offset + X11_SCREEN_WIDTH_OFFSET;
		u16 width	    = 0;
		u16 height	    = 0;
		u16 physical_width  = 0;
		u16 physical_height = 0;
		(void)buf_read_u16le(setup, &off, &width);
		(void)buf_read_u16le(setup, &off, &height);
		(void)buf_read_u16le(setup, &off, &physical_width);
		(void)buf_read_u16le(setup, &off, &physical_height);
		x11_monitor_set(
			&dx11->monitors[i], i, 0, 0, width, height, physical_width, physical_height, i == 0, (void *)(uintptr_t)root);

		screen_offset += screen_size(setup, screen_offset);
	}

	return 0;
}

static int visual_depth(const display_x11_direct_t *dx11, u32 visual, u8 *depth)
{
	for (size_t i = 0; i < dx11->visual_count; ++i) {
		if (dx11->visuals[i].id == visual) {
			*depth = dx11->visuals[i].depth;
			return 0;
		}
	}

	return 1;
}

static int open_display(display_t *d)
{
	display_x11_direct_t *dx11 = d->data;

	strv_t name = proc_getenv(d->proc, STRV("DISPLAY"));
	if (name.data == NULL) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to get display name");
		return 1;
	}

	if (name.data[0] != ':') {
		log_error("cdisplay", "display_x11_direct", NULL, "invalid display name: %.*s", name.len, name.data);
		return 1;
	}

	strv_t display_name = STRVN(&name.data[1], name.len - 1);
	strv_t display_number_str;
	if (strv_lsplit(display_name, '.', &display_number_str, NULL) || strv_to_int(display_number_str, NULL)) {
		log_error("cdisplay",
			  "display_x11_direct",
			  NULL,
			  "invalid display number: %.*s",
			  display_number_str.len,
			  display_number_str.data);
		return 1;
	}

	cerr_t err = sock_open(d->ss, SOCK_FAMILY_UNIX, SOCK_TYPE_STREAM, 0, &dx11->sock);

	if (err != CERR_OK) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to open unix socket");
		return 1;
	}

	char buf[256] = {0};
	str_t path    = STRB(buf, 0);
	str_cat(&path, STRV("/tmp/.X11-unix/X"));
	str_cat(&path, display_number_str);

	err = sock_connect(d->ss, dx11->sock, SOCK_FAMILY_UNIX, path.data, path.len + 1);
	if (err != CERR_OK) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to connect to unix socket");
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	strv_t xauthority_path = proc_getenv(d->proc, STRV("XAUTHORITY"));
	if (xauthority_path.data == NULL) {
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	buf_t xauthority = {0};
	buf_init(&xauthority, 256, d->alloc);

	if (fs_readb(d->fs, xauthority_path, &xauthority)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to read xauthority");
		buf_free(&xauthority);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	char hostname_buf[256] = {0};
	if (proc_gethostname(d->proc, hostname_buf, sizeof(hostname_buf) - 1)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to get hostname");
		buf_free(&xauthority);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	strv_t hostname = strv_cstr(hostname_buf);

	size_t off = 0;

	int best_score	  = XAUTH_SCORE_NO_MATCH;
	const u8 *cookie  = NULL;
	u16 cookie_length = 0;

	strv_t mit_magic_cookie = STRV("MIT-MAGIC-COOKIE-1");

	int ret = 0;

	while (1) {
		u16 family;
		if (buf_read_u16be(&xauthority, &off, &family)) {
			break;
		}

		const u8 *address;
		u16 address_length;
		const u8 *number;
		u16 number_length;
		const u8 *name;
		u16 name_length;
		const u8 *data;
		u16 data_length;
		if (buf_read_blob(&xauthority, &address, &address_length, &off) ||
		    buf_read_blob(&xauthority, &number, &number_length, &off) || buf_read_blob(&xauthority, &name, &name_length, &off) ||
		    buf_read_blob(&xauthority, &data, &data_length, &off)) {
			ret = 1;
			break;
		}

		int score = XAUTH_SCORE_NO_MATCH;
		if (data_length > 0 &&
		    (number_length == 0 || bytes_eq(number, number_length, display_number_str.data, display_number_str.len)) &&
		    bytes_eq(name, name_length, mit_magic_cookie.data, mit_magic_cookie.len)) {
			switch (family) {
			case XAUTH_FAMILY_LOCAL: {
				score = bytes_eq(address, address_length, hostname.data, hostname.len) ? XAUTH_SCORE_LOCAL_HOST
												       : XAUTH_SCORE_LOCAL;
				break;
			}
			case XAUTH_FAMILY_WILD: {
				score = XAUTH_SCORE_WILD;
				break;
			}
			default: {
				break;
			}
			}
		}

		if (score > best_score) {
			cookie	      = data;
			cookie_length = data_length;
			best_score    = score;
		}
	}

	if (ret) {
		log_error("cdisplay", "display_x11_direct", NULL, "malformed authority");
		buf_free(&xauthority);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	buf_t b = {0};
	buf_init(&b, X11_CONNECTION_SETUP_SIZE, d->alloc);
	strv_t byte_order = STRV("l\0");
	buf_add_str(&b, byte_order, NULL);
	u8 padding[X11_PAD_SIZE - 1] = {0};

	strv_t auth_name = cookie ? mit_magic_cookie : STRV("");

	buf_write_u16le(&b, X11_PROTOCOL_MAJOR);
	buf_write_u16le(&b, X11_PROTOCOL_MINOR);
	buf_write_u16le(&b, auth_name.len);
	buf_write_u16le(&b, cookie_length);
	buf_write_u16le(&b, 0);

	if (sock_write_all(d->ss, dx11->sock, b.data, b.used) || sock_write_all(d->ss, dx11->sock, auth_name.data, auth_name.len) ||
	    sock_write_all(d->ss, dx11->sock, padding, x11_pad4(auth_name.len)) ||
	    sock_write_all(d->ss, dx11->sock, cookie, cookie_length) ||
	    sock_write_all(d->ss, dx11->sock, padding, x11_pad4(cookie_length)) ||
	    sock_read_all(d->ss, dx11->sock, b.data, X11_SETUP_REPLY_HEADER_SIZE)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to send request");
		buf_free(&b);
		buf_free(&xauthority);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	buf_free(&xauthority);

	off = 0;
	u8 success;
	buf_read_u8le(&b, &off, &success);
	if (success != X11_REPLY_SUCCESS) {
		log_error("cdisplay", "display_x11_direct", NULL, "connection setup was not successful");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	off = X11_SETUP_PROTOCOL_MAJOR_OFFSET;
	u16 major_version;
	buf_read_u16le(&b, &off, &major_version);
	u16 minor_version;
	buf_read_u16le(&b, &off, &minor_version);
	log_info("cdisplay", "display_x11_direct", NULL, "X11 protocol version: %u.%u", major_version, minor_version);

	u16 extra_words;
	buf_read_u16le(&b, &off, &extra_words);
	size_t setup_length = (size_t)extra_words * X11_PAD_SIZE;

	if (setup_length < X11_SETUP_MIN_SIZE) {
		log_error("cdisplay", "display_x11_direct", NULL, "invalid setup length: %zu", setup_length);
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	if (buf_resize(&b, setup_length)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to allocate setup");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	if (sock_read_all(d->ss, dx11->sock, b.data, setup_length)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to read setup");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}
	b.used = setup_length;

	off = X11_SETUP_RESOURCE_ID_BASE_OFFSET;
	buf_read_u32le(&b, &off, &dx11->resource_id_base);
	buf_read_u32le(&b, &off, &dx11->resource_id_mask);

	off = X11_SETUP_VENDOR_LENGTH_OFFSET;
	u16 vendor_length;
	buf_read_u16le(&b, &off, &vendor_length);
	off = X11_SETUP_SCREEN_COUNT_OFFSET;
	u8 screen_count;
	buf_read_u8le(&b, &off, &screen_count);
	off = X11_SETUP_FORMAT_COUNT_OFFSET;
	u8 format_count;
	buf_read_u8le(&b, &off, &format_count);
	off = X11_SETUP_MIN_KEYCODE_OFFSET;
	buf_read_u8le(&b, &off, &dx11->min_keycode);
	off = X11_SETUP_MAX_KEYCODE_OFFSET;
	buf_read_u8le(&b, &off, &dx11->max_keycode);

	size_t screen_offset =
		X11_SETUP_SCREEN_LIST_OFFSET + vendor_length + x11_pad4(vendor_length) + (size_t)format_count * X11_FORMAT_SIZE;

	if (screen_count == 0) {
		log_error("cdisplay", "display_x11_direct", NULL, "no screens found");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	if (screen_offset + X11_SCREEN_MIN_SIZE > setup_length) {
		log_error("cdisplay", "display_x11_direct", NULL, "invalid screen offset: %zu", screen_offset);
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	off = screen_offset;
	buf_read_u32le(&b, &off, &dx11->root);
	off += X11_SCREEN_DEFAULT_COLORMAP_SIZE;
	buf_read_u32le(&b, &off, &dx11->white_pixel);
	buf_read_u32le(&b, &off, &dx11->black_pixel);
	if (read_visuals(d, &b, screen_offset)) {
		log_error("cdisplay", "display_x11_direct", NULL, "invalid X11 visual list");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}
	if (read_screens(d, &b, screen_offset, screen_count)) {
		log_error("cdisplay", "display_x11_direct", NULL, "invalid X11 screen list");
		alloc_free(&d->alloc, dx11->monitors, dx11->monitor_count * sizeof(*dx11->monitors));
		dx11->monitors	    = NULL;
		dx11->monitor_count = 0;
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	buf_free(&b);

	dx11->next_resource = 0;

	return 0;
}

static unsigned ctz32(uint32_t value)
{
	unsigned count = 0;

	while ((value & 1u) == 0) {
		value >>= 1;
		++count;
	}

	return count;
}

enum {
	X_CREATE_WINDOW	       = 1,
	X_DESTROY_WINDOW       = 4,
	X_MAP_WINDOW	       = 8,
	X_UNMAP_WINDOW	       = 10,
	X_CONFIGURE_WINDOW     = 12,
	X_INTERN_ATOM	       = 16,
	X_CHANGE_PROPERTY      = 18,
	X_SEND_EVENT	       = 25,
	X_CREATE_COLORMAP      = 78,
	X_FREE_COLORMAP	       = 79,
	X_GET_KEYBOARD_MAPPING = 101,
};

enum {
	X_CHANGE_PROPERTY_MODE_REPLACE = 0,
	X_PROPERTY_FORMAT_32	       = 32,
};

enum {
	X_CREATE_WINDOW_REQUEST_SIZE	= 32,
	X_CREATE_WINDOW_REQUEST_WORDS	= X_CREATE_WINDOW_REQUEST_SIZE / X11_PAD_SIZE,
	X_CREATE_WINDOW_MAX_VALUE_COUNT = 15,
	X_DEFAULT_WINDOW_BORDER_WIDTH	= 1,
};

enum {
	X_CREATE_COLORMAP_REQUEST_SIZE	= 16,
	X_CREATE_COLORMAP_REQUEST_WORDS = X_CREATE_COLORMAP_REQUEST_SIZE / X11_PAD_SIZE,
	X_FREE_COLORMAP_REQUEST_SIZE	= 8,
	X_FREE_COLORMAP_REQUEST_WORDS	= X_FREE_COLORMAP_REQUEST_SIZE / X11_PAD_SIZE,
	X_COLORMAP_ALLOC_NONE		= 0,
};

enum {
	X_INTERN_ATOM_REQUEST_SIZE	= 8,
	X_INTERN_ATOM_NAME_MAX		= 64,
	X_INTERN_ATOM_REPLY_ATOM_OFFSET = 8,
};

enum {
	X_GET_ATOM_NAME		 = 17,
	X_GET_ATOM_NAME_SIZE	 = 8,
	X_GET_ATOM_NAME_WORDS	 = X_GET_ATOM_NAME_SIZE / X11_PAD_SIZE,
	X_GET_ATOM_NAME_LENGTH	 = 8,
	X_GET_ATOM_NAME_MAX_SIZE = 128,
};

enum {
	X_RANDR_GET_MONITORS_REQUEST_SIZE = 8,
	X_RANDR_GET_MONITORS_REPLY_COUNT  = 12,
	X_RANDR_GET_MONITORS_INFO_SIZE	  = 24,
	X_RANDR_GET_MONITORS_OUTPUT_SIZE  = 4,
};

enum {
	X_CHANGE_PROPERTY_HEADER_SIZE	= 24,
	X_CHANGE_PROPERTY_MAX_DATA_SIZE = X11_PAD_SIZE * 0xffff - X_CHANGE_PROPERTY_HEADER_SIZE,
	X_CHANGE_PROPERTY_ITEM_COUNT	= 1,
};

enum {
	X_SEND_EVENT_REQUEST_SIZE  = 44,
	X_SEND_EVENT_REQUEST_WORDS = X_SEND_EVENT_REQUEST_SIZE / X11_PAD_SIZE,
};

enum {
	X_WINDOW_ID_REQUEST_SIZE  = 8,
	X_WINDOW_ID_REQUEST_WORDS = X_WINDOW_ID_REQUEST_SIZE / X11_PAD_SIZE,
};

enum {
	X_CONFIGURE_WINDOW_REQUEST_SIZE	 = 12,
	X_CONFIGURE_WINDOW_REQUEST_WORDS = X_CONFIGURE_WINDOW_REQUEST_SIZE / X11_PAD_SIZE,
};

enum {
	X_CONFIG_WINDOW_X      = 1u << 0,
	X_CONFIG_WINDOW_Y      = 1u << 1,
	X_CONFIG_WINDOW_WIDTH  = 1u << 2,
	X_CONFIG_WINDOW_HEIGHT = 1u << 3,
};

enum {
	X_GET_KEYBOARD_MAPPING_REQUEST_SIZE	   = 8,
	X_GET_KEYBOARD_MAPPING_REQUEST_WORDS	   = X_GET_KEYBOARD_MAPPING_REQUEST_SIZE / X11_PAD_SIZE,
	X_GET_KEYBOARD_MAPPING_KEYSYM_COUNT_OFFSET = 1,
	X_GET_KEYBOARD_MAPPING_LENGTH_OFFSET	   = 4,
};

enum {
	X_GET_MODIFIER_MAPPING			    = 119,
	X_GET_MODIFIER_MAPPING_REQUEST_SIZE	    = 4,
	X_GET_MODIFIER_MAPPING_REQUEST_WORDS	    = X_GET_MODIFIER_MAPPING_REQUEST_SIZE / X11_PAD_SIZE,
	X_GET_MODIFIER_MAPPING_KEYCODE_COUNT_OFFSET = 1,
	X_GET_MODIFIER_MAPPING_LENGTH_OFFSET	    = 4,
};

enum {
	X_KEY_BUTTON_EVENT_DETAIL_OFFSET   = 1,
	X_KEY_BUTTON_EVENT_WINDOW_OFFSET   = 12,
	X_KEY_BUTTON_EVENT_POSITION_OFFSET = 24,
	X_KEY_BUTTON_EVENT_STATE_OFFSET	   = 28,

	X_MOTION_EVENT_WINDOW_OFFSET   = 12,
	X_MOTION_EVENT_POSITION_OFFSET = 24,
	X_MOTION_EVENT_STATE_OFFSET    = 28,

	X_FOCUS_EVENT_WINDOW_OFFSET	  = 4,
	X_DESTROY_EVENT_WINDOW_OFFSET	  = 8,
	X_CONFIGURE_EVENT_WINDOW_OFFSET	  = 8,
	X_CONFIGURE_EVENT_GEOMETRY_OFFSET = 16,

	X_CLIENT_MESSAGE_FORMAT_OFFSET = 1,
	X_CLIENT_MESSAGE_WINDOW_OFFSET = 4,
	X_CLIENT_MESSAGE_DATA_OFFSET   = 12,
};

static int alloc_resource_id(display_x11_direct_t *dx11, u32 *id)
{
	uint shift   = ctz32(dx11->resource_id_mask);
	u32 capacity = dx11->resource_id_mask >> shift;

	if (capacity < dx11->next_resource) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to allocate resource");
		return 1;
	}

	u32 resource_bits = dx11->next_resource << shift;
	*id		  = dx11->resource_id_base | resource_bits;
	dx11->next_resource++;
	return 0;
}

static int read_reply(display_t *display, u8 reply[static X11_REPLY_SIZE])
{
	display_x11_direct_t *dx11 = display->data;
	if (sock_read_all(display->ss, dx11->sock, reply, X11_REPLY_SIZE)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to read X11 reply");
		return 1;
	}
	if (reply[0] == 0) {
		size_t off = 2;
		u16 sequence;
		u32 value;
		u16 minor_opcode;
		cbuf_read_u16le(reply, &off, &sequence);
		cbuf_read_u32le(reply, &off, &value);
		cbuf_read_u16le(reply, &off, &minor_opcode);
		log_error("cdisplay",
			  "display_x11_direct",
			  NULL,
			  "X11 request failed: error=%u sequence=%u value=%u major_opcode=%u minor_opcode=%u",
			  reply[1],
			  sequence,
			  value,
			  reply[10],
			  minor_opcode);
		return 1;
	}
	if (reply[0] != X11_REPLY_SUCCESS) {
		log_error("cdisplay", "display_x11_direct", NULL, "unexpected X11 reply type: %u", reply[0]);
		return 1;
	}

	return 0;
}

static buf_t *request_buffer(display_t *display, size_t size)
{
	display_x11_direct_t *dx11 = display->data;
	if (dx11->request.data == NULL) {
		if (buf_init(&dx11->request, size, display->alloc) == NULL) {
			return NULL;
		}
	} else if (dx11->request.size < size) {
		if (buf_resize(&dx11->request, size)) {
			return NULL;
		}
	}

	buf_reset(&dx11->request, 0);
	return &dx11->request;
}

static int request_pad4(buf_t *request, size_t length)
{
	static const u8 padding[X11_PAD_SIZE] = {0};
	return buf_add(request, x11_pad4(length), padding, NULL);
}

static int write_request_header(buf_t *request, u8 opcode, u8 detail, size_t size)
{
	// clang-format off
	return
	buf_write_u8le(request, opcode) ||
	buf_write_u8le(request, detail) ||
	buf_write_u16le(request, (u16)(size / X11_PAD_SIZE));
	// clang-format on
}

static int write_query_extension_request(buf_t *request, strv_t name, size_t size)
{
	// clang-format off
	return
	write_request_header(request, X11_QUERY_EXTENSION, 0, size) ||
	buf_write_u16le(request, (u16)name.len) ||
	buf_write_u16le(request, 0) ||
	buf_add(request, name.len, name.data, NULL) ||
	request_pad4(request, name.len);
	// clang-format on
}

static int write_ext_request(buf_t *request, u8 ext_opcode, u8 opcode, const void *data, size_t size)
{
	// clang-format off
	return
	write_request_header(request, ext_opcode, opcode, X11_PAD_SIZE + size) ||
	buf_add(request, size, data, NULL);
	// clang-format on
}

static void write_change_property_text_request(buf_t *request, window_x11_t *wx11, u32 property, u32 type, strv_t text, size_t size)
{
	write_request_header(request, X_CHANGE_PROPERTY, X_CHANGE_PROPERTY_MODE_REPLACE, size);
	buf_write_u32le(request, wx11->id);
	buf_write_u32le(request, property);
	buf_write_u32le(request, type);
	buf_write_u8le(request, 8);
	buf_write_u8le(request, 0);
	buf_write_u8le(request, 0);
	buf_write_u8le(request, 0);
	buf_write_u32le(request, (u32)text.len);
	buf_add(request, text.len, text.data, NULL);
	request_pad4(request, text.len);
}

static int query_extension(display_ext_t *ext, strv_t name, int log_unavailable)
{
	if (ext == NULL || ext->display == NULL || name.len > UINT16_MAX) {
		return 1;
	}

	size_t request_size = 8 + name.len + x11_pad4(name.len);
	buf_t *request	    = request_buffer(ext->display, request_size);
	if (request == NULL) {
		return 1;
	}

	if (write_query_extension_request(request, name, request_size)) {
		return 1;
	}

	u8 reply[X11_REPLY_SIZE];
	display_x11_direct_t *dx11 = ext->display->data;
	int ret = sock_write_all(ext->display->ss, dx11->sock, request->data, request->used) || read_reply(ext->display, reply);
	if (ret || reply[8] == 0) {
		if (log_unavailable) {
			log_error("cdisplay", "display_x11_direct", NULL, "display ext is unavailable: %.*s", name.len, name.data);
		}
		return 1;
	}

	ext->opcode	 = reply[9];
	ext->first_event = reply[10];
	ext->first_error = reply[11];
	return 0;
}

static int display_x11_direct_ext_init(display_ext_t *ext, strv_t name)
{
	return query_extension(ext, name, 1);
}

static int display_x11_direct_ext_send(display_ext_t *ext, u8 opcode, const void *data, size_t size)
{
	if (ext == NULL || ext->display == NULL || size % X11_PAD_SIZE != 0 || size > (size_t)UINT16_MAX * X11_PAD_SIZE - X11_PAD_SIZE) {
		return 1;
	}

	size_t request_size = X11_PAD_SIZE + size;
	buf_t *request	    = request_buffer(ext->display, request_size);
	if (request == NULL) {
		return 1;
	}

	if (write_ext_request(request, ext->opcode, opcode, data, size)) {
		return 1;
	}

	display_x11_direct_t *dx11 = ext->display->data;
	int ret			   = sock_write_all(ext->display->ss, dx11->sock, request->data, request->used);
	if (ret) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to send display ext request: opcode=%u", opcode);
	}
	return ret;
}

static int display_x11_direct_ext_call(display_ext_t *ext, u8 opcode, const void *data, size_t size, display_ext_reply_t *reply)
{
	if (display_x11_direct_ext_send(ext, opcode, data, size) || read_reply(ext->display, reply->header)) {
		return 1;
	}

	size_t off = 4;
	u32 words;
	cbuf_read_u32le(reply->header, &off, &words);
	reply->size = (size_t)words * X11_PAD_SIZE;
	if (reply->size == 0) {
		return 0;
	}

	reply->data = alloc_alloc(&ext->display->alloc, reply->size);
	if (reply->data == NULL) {
		u8 discard[1024];
		size_t remaining	   = reply->size;
		display_x11_direct_t *dx11 = ext->display->data;
		while (remaining != 0) {
			size_t chunk = remaining < sizeof(discard) ? remaining : sizeof(discard);
			if (sock_read_all(ext->display->ss, dx11->sock, discard, chunk)) {
				break;
			}
			remaining -= chunk;
		}
		log_error("cdisplay", "display_x11_direct", NULL, "failed to allocate display ext reply data");
		return 1;
	}

	display_x11_direct_t *dx11 = ext->display->data;
	if (sock_read_all(ext->display->ss, dx11->sock, reply->data, reply->size)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to read display ext reply data");
		return 1;
	}
	return 0;
}

static int display_x11_direct_alloc_id(display_t *display, u32 *id)
{
	return display == NULL || display->data == NULL ? 1 : alloc_resource_id(display->data, id);
}

static int display_x11_direct_visual_depth(display_t *display, u32 visual, u8 *depth)
{
	if (display == NULL || display->data == NULL || visual_depth(display->data, visual, depth)) {
		log_error("cdisplay", "display_x11_direct", NULL, "unknown X11 visual: %u", visual);
		return 1;
	}
	return 0;
}

static int display_x11_direct_randr_monitors(display_t *display, arr_t *monitors)
{
	display_ext_t randr = {.display = display};
	if (query_extension(&randr, STRV("RANDR"), 0)) {
		return 2;
	}

	display_x11_direct_t *dx11		      = display->data;
	u8 request[X_RANDR_GET_MONITORS_REQUEST_SIZE] = {0};
	size_t off				      = 0;
	cbuf_write_u32le(request, &off, dx11->root);
	cbuf_write_u8le(request, &off, 1);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, 0);

	display_ext_reply_t reply = {0};
	if (display_ext_call(&randr, X_RANDR_GET_MONITORS, request, sizeof(request), &reply)) {
		return 2;
	}

	u32 count = 0;
	cbuf_get_u32le(reply.header, X_RANDR_GET_MONITORS_REPLY_COUNT, &count);
	if (arr_resize(monitors, count)) {
		display_ext_reply_free(&reply);
		return 1;
	}
	monitors->cnt = count;

	off = 0;
	for (u32 i = 0; i < count; ++i) {
		if (off + X_RANDR_GET_MONITORS_INFO_SIZE > reply.size) {
			display_ext_reply_free(&reply);
			return 1;
		}

		u32 atom	    = 0;
		u8 primary	    = 0;
		u8 automatic	    = 0;
		u16 output_count    = 0;
		u16 x		    = 0;
		u16 y		    = 0;
		u16 width	    = 0;
		u16 height	    = 0;
		u32 physical_width  = 0;
		u32 physical_height = 0;
		cbuf_read_u32le(reply.data, &off, &atom);
		cbuf_read_u8le(reply.data, &off, &primary);
		cbuf_read_u8le(reply.data, &off, &automatic);
		cbuf_read_u16le(reply.data, &off, &output_count);
		cbuf_read_u16le(reply.data, &off, &x);
		cbuf_read_u16le(reply.data, &off, &y);
		cbuf_read_u16le(reply.data, &off, &width);
		cbuf_read_u16le(reply.data, &off, &height);
		cbuf_read_u32le(reply.data, &off, &physical_width);
		cbuf_read_u32le(reply.data, &off, &physical_height);

		size_t output_size = (size_t)output_count * X_RANDR_GET_MONITORS_OUTPUT_SIZE;
		if (off + output_size > reply.size) {
			display_ext_reply_free(&reply);
			return 1;
		}

		(void)automatic;
		void *native = NULL;
		if (output_count > 0) {
			u32 output = 0;
			cbuf_get_u32le(reply.data, off, &output);
			native = (void *)(uintptr_t)output;
		}

		display_monitor_t *monitor = arr_get(monitors, i);
		x11_monitor_set(monitor, i, x11_s16(x), x11_s16(y), width, height, physical_width, physical_height, primary != 0, native);
		off += output_size;
		get_atom_name(display, atom, monitor->name, sizeof(monitor->name));
	}

	display_ext_reply_free(&reply);
	return 0;
}

static int create_colormap(window_t *wnd, u32 visual)
{
	u8 request[X_CREATE_COLORMAP_REQUEST_SIZE] = {0};

	display_x11_direct_t *dx11 = wnd->display->data;
	window_x11_t *wx11	   = display_x11_direct_window_data(wnd);

	if (alloc_resource_id(dx11, &wx11->colormap)) {
		return 1;
	}

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_CREATE_COLORMAP);
	cbuf_write_u8le(request, &off, X_COLORMAP_ALLOC_NONE);
	cbuf_write_u16le(request, &off, X_CREATE_COLORMAP_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, wx11->colormap);
	cbuf_write_u32le(request, &off, dx11->root);
	cbuf_write_u32le(request, &off, visual);

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to create colormap");
		wx11->colormap = 0;
		return 1;
	}

	return 0;
}

static int free_colormap(window_t *wnd)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11->colormap == 0) {
		return 0;
	}

	u8 request[X_FREE_COLORMAP_REQUEST_SIZE] = {0};
	size_t off				 = 0;
	cbuf_write_u8le(request, &off, X_FREE_COLORMAP);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_FREE_COLORMAP_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, wx11->colormap);

	display_x11_direct_t *dx11 = wnd->display->data;
	int ret			   = sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request));
	wx11->colormap		   = 0;

	if (ret) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to free colormap");
		return 1;
	}

	return 0;
}

static int create_window(window_t *wnd, const window_config_t *config)
{
	u8 request[X_CREATE_WINDOW_REQUEST_SIZE + X_CREATE_WINDOW_MAX_VALUE_COUNT * X11_PAD_SIZE] = {0};
	u32 values[X_CREATE_WINDOW_MAX_VALUE_COUNT]						  = {0};
	size_t value_count									  = 0;

	display_x11_direct_t *dx11 = wnd->display->data;

	u32 background_pixel = dx11->white_pixel;
	u32 border_pixel     = dx11->black_pixel;

	u32 event_mask = X_EVENT_MASK_KEY_PRESS | X_EVENT_MASK_KEY_RELEASE | X_EVENT_MASK_BUTTON_PRESS | X_EVENT_MASK_BUTTON_RELEASE |
			 X_EVENT_MASK_POINTER_MOTION | X_EVENT_MASK_EXPOSURE | X_EVENT_MASK_STRUCTURE | X_EVENT_MASK_FOCUS_CHANGE;

	if (config->background != WINDOW_BACKGROUND_NONE) {
		values[value_count++] = background_pixel;
	}
	values[value_count++] = border_pixel;
	values[value_count++] = event_mask;

	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (alloc_resource_id(dx11, &wx11->id)) {
		return 1;
	}
	if (config->visual != 0 && create_colormap(wnd, config->visual)) {
		return 1;
	}

	if (wx11->colormap != 0) {
		values[value_count++] = wx11->colormap;
	}

	u8 depth = config->depth == 0 ? X_COPY_FROM_PARENT : config->depth;

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_CREATE_WINDOW);
	cbuf_write_u8le(request, &off, depth);

	u16 length = X_CREATE_WINDOW_REQUEST_WORDS + value_count;
	cbuf_write_u16le(request, &off, length);
	cbuf_write_u32le(request, &off, wx11->id);

	u32 parent = dx11->root;
	cbuf_write_u32le(request, &off, parent);

	cbuf_write_u16le(request, &off, config->x);
	cbuf_write_u16le(request, &off, config->y);

	cbuf_write_u16le(request, &off, config->width);
	cbuf_write_u16le(request, &off, config->height);

	u16 border_width = X_DEFAULT_WINDOW_BORDER_WIDTH;
	cbuf_write_u16le(request, &off, border_width);

	u16 window_class = X_INPUT_OUTPUT;
	cbuf_write_u16le(request, &off, window_class);

	u32 visual = config->visual == 0 ? X_COPY_FROM_PARENT : config->visual;
	cbuf_write_u32le(request, &off, visual);

	u32 value_mask = X_CW_BORDER_PIXEL | X_CW_EVENT_MASK;
	if (config->background != WINDOW_BACKGROUND_NONE) {
		value_mask |= X_CW_BACK_PIXEL;
	}
	if (wx11->colormap != 0) {
		value_mask |= X_CW_COLORMAP;
	}
	cbuf_write_u32le(request, &off, value_mask);

	for (size_t i = 0; i < value_count; i++) {
		cbuf_write_u32le(request, &off, values[i]);
	}

	size_t request_size = X_CREATE_WINDOW_REQUEST_SIZE + value_count * X11_PAD_SIZE;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, request_size)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to create window");
		return 1;
	}

	return 0;
}

static int intern_atom(display_t *display, strv_t name, u32 *atom)
{
	u8 request[X_INTERN_ATOM_REQUEST_SIZE + X_INTERN_ATOM_NAME_MAX] = {0};
	u8 reply[X11_REPLY_SIZE]					= {0};
	size_t off							= 0;

	cbuf_write_u8le(request, &off, X_INTERN_ATOM);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, (u16)(X_INTERN_ATOM_REQUEST_SIZE / X11_PAD_SIZE + (name.len + x11_pad4(name.len)) / X11_PAD_SIZE));
	cbuf_write_u16le(request, &off, name.len);
	cbuf_write_u16le(request, &off, 0);
	mem_copy(&request[off], sizeof(request) - off, name.data, name.len);
	off += name.len + x11_pad4(name.len);

	display_x11_direct_t *dx11 = display->data;
	if (sock_write_all(display->ss, dx11->sock, request, off) || sock_read_all(display->ss, dx11->sock, reply, sizeof(reply))) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to intern atom: %.*s", name.len, name.data);
		return 1;
	}

	if (reply[0] != X11_REPLY_SUCCESS) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to intern atom: %.*s", name.len, name.data);
		return 1;
	}

	cbuf_get_u32le(reply, X_INTERN_ATOM_REPLY_ATOM_OFFSET, atom);
	if (*atom == 0) {
		log_error("cdisplay", "display_x11_direct", NULL, "atom not found: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

static void get_atom_name(display_t *display, u32 atom, char *name, size_t name_size)
{
	if (name == NULL || name_size == 0 || atom == 0) {
		return;
	}

	u8 request[X_GET_ATOM_NAME_SIZE] = {0};
	u8 reply[X11_REPLY_SIZE]	 = {0};
	size_t off			 = 0;

	cbuf_write_u8le(request, &off, X_GET_ATOM_NAME);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_GET_ATOM_NAME_WORDS);
	cbuf_write_u32le(request, &off, atom);

	display_x11_direct_t *dx11 = display->data;
	if (sock_write_all(display->ss, dx11->sock, request, sizeof(request)) || read_reply(display, reply)) {
		return;
	}

	u32 words  = 0;
	u16 length = 0;
	cbuf_get_u32le(reply, 4, &words);
	cbuf_get_u16le(reply, X_GET_ATOM_NAME_LENGTH, &length);
	size_t data_size = (size_t)words * X11_PAD_SIZE;
	if (data_size == 0 || length == 0) {
		return;
	}

	u8 data[X_GET_ATOM_NAME_MAX_SIZE] = {0};
	size_t read_size		  = data_size > sizeof(data) ? sizeof(data) : data_size;
	if (sock_read_all(display->ss, dx11->sock, data, read_size)) {
		return;
	}
	for (size_t remaining = data_size - read_size; remaining > 0;) {
		u8 discard[64];
		size_t chunk = remaining < sizeof(discard) ? remaining : sizeof(discard);
		if (sock_read_all(display->ss, dx11->sock, discard, chunk)) {
			return;
		}
		remaining -= chunk;
	}

	size_t copy_size = length;
	if (copy_size >= name_size) {
		copy_size = name_size - 1;
	}
	if (copy_size > read_size) {
		copy_size = read_size;
	}
	mem_copy(name, name_size, data, copy_size);
	name[copy_size] = 0;
}

static int init_atoms(display_t *display)
{
	display_x11_direct_t *dx11 = display->data;

	if (intern_atom(display, STRV("WM_PROTOCOLS"), &dx11->wm_protocols) ||
	    intern_atom(display, STRV("WM_DELETE_WINDOW"), &dx11->wm_delete_window) ||
	    intern_atom(display, STRV("WM_NAME"), &dx11->wm_name) || intern_atom(display, STRV("_NET_WM_NAME"), &dx11->net_wm_name) ||
	    intern_atom(display, STRV("UTF8_STRING"), &dx11->utf8_string) ||
	    intern_atom(display, STRV("_MOTIF_WM_HINTS"), &dx11->motif_wm_hints) ||
	    intern_atom(display, STRV("_NET_WM_STATE"), &dx11->net_wm_state) ||
	    intern_atom(display, STRV("_NET_WM_STATE_FULLSCREEN"), &dx11->net_wm_state_fullscreen)) {
		return 1;
	}

	return 0;
}

static int set_property_text(window_t *wnd, u32 property, u32 type, strv_t text)
{
	size_t data_size    = text.len;
	size_t request_size = X_CHANGE_PROPERTY_HEADER_SIZE + data_size + x11_pad4(data_size);

	buf_t *request = request_buffer(wnd->display, request_size);
	if (request == NULL) {
		return 1;
	}

	window_x11_t *wx11 = display_x11_direct_window_data(wnd);

	write_change_property_text_request(request, wx11, property, type, text, request_size);

	display_x11_direct_t *dx11 = wnd->display->data;
	int ret			   = sock_write_all(wnd->display->ss, dx11->sock, request->data, request->used);

	if (ret) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to set window text property");
		return 1;
	}

	return 0;
}

static int set_property_u32(window_t *wnd, u32 property, u32 type, const u32 *values, u32 count)
{
	size_t data_size     = (size_t)count * sizeof(u32);
	size_t request_size  = X_CHANGE_PROPERTY_HEADER_SIZE + data_size;
	size_t request_words = request_size / X11_PAD_SIZE;

	u8 request[X_CHANGE_PROPERTY_HEADER_SIZE + X_SIZE_HINT_FIELD_COUNT * sizeof(u32)] = {0};
	mem_set(request, 0, request_size);

	window_x11_t *wx11 = display_x11_direct_window_data(wnd);

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_CHANGE_PROPERTY);
	cbuf_write_u8le(request, &off, X_CHANGE_PROPERTY_MODE_REPLACE);
	cbuf_write_u16le(request, &off, (u16)request_words);
	cbuf_write_u32le(request, &off, wx11->id);
	cbuf_write_u32le(request, &off, property);
	cbuf_write_u32le(request, &off, type);
	cbuf_write_u8le(request, &off, X_PROPERTY_FORMAT_32);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u32le(request, &off, count);

	for (u32 i = 0; i < count; i++) {
		cbuf_write_u32le(request, &off, values[i]);
	}

	display_x11_direct_t *dx11 = wnd->display->data;
	int ret			   = sock_write_all(wnd->display->ss, dx11->sock, request, request_size);

	if (ret) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to set window property");
		return 1;
	}

	return 0;
}

static int set_borderless(window_t *wnd, int borderless)
{
	display_x11_direct_t *dx11	      = wnd->display->data;
	u32 hints[MOTIF_WM_HINTS_FIELD_COUNT] = {
		MOTIF_WM_HINTS_DECORATIONS_FLAG,
		0,
		borderless ? 0 : MOTIF_WM_DECOR_ALL,
		0,
		0,
	};

	return set_property_u32(wnd, dx11->motif_wm_hints, dx11->motif_wm_hints, hints, MOTIF_WM_HINTS_FIELD_COUNT);
}

static int set_fullscreen_property(window_t *wnd, int fullscreen)
{
	display_x11_direct_t *dx11 = wnd->display->data;
	u32 state[]		   = {dx11->net_wm_state_fullscreen};

	return set_property_u32(wnd, dx11->net_wm_state, XA_ATOM, state, fullscreen ? 1 : 0);
}

static int send_fullscreen_message(window_t *wnd, int fullscreen)
{
	u8 request[X_SEND_EVENT_REQUEST_SIZE] = {0};
	size_t off			      = 0;

	window_x11_t *wx11	   = display_x11_direct_window_data(wnd);
	display_x11_direct_t *dx11 = wnd->display->data;

	cbuf_write_u8le(request, &off, X_SEND_EVENT);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_SEND_EVENT_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, dx11->root);
	cbuf_write_u32le(request, &off, X_EVENT_MASK_SUBSTRUCTURE_REDIRECT | X_EVENT_MASK_SUBSTRUCTURE_NOTIFY);
	cbuf_write_u8le(request, &off, X_EVENT_CLIENT_MESSAGE);
	cbuf_write_u8le(request, &off, X_PROPERTY_FORMAT_32);
	cbuf_write_u16le(request, &off, 0);
	cbuf_write_u32le(request, &off, wx11->id);
	cbuf_write_u32le(request, &off, dx11->net_wm_state);
	cbuf_write_u32le(request, &off, fullscreen ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE);
	cbuf_write_u32le(request, &off, dx11->net_wm_state_fullscreen);
	cbuf_write_u32le(request, &off, 0);
	cbuf_write_u32le(request, &off, 1);
	cbuf_write_u32le(request, &off, 0);

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to set fullscreen state");
		return 1;
	}

	return 0;
}

static int set_wm_normal_hints(window_t *wnd, const window_config_t *config)
{
	u32 hints[X_SIZE_HINT_FIELD_COUNT] = {
		X_SIZE_HINT_US_POSITION | X_SIZE_HINT_US_SIZE | X_SIZE_HINT_P_POSITION | X_SIZE_HINT_P_SIZE,
		config->x,
		config->y,
		config->width,
		config->height,
	};

	return set_property_u32(wnd, XA_WM_NORMAL_HINTS, XA_WM_SIZE_HINTS, hints, X_SIZE_HINT_FIELD_COUNT);
}

static int set_fullscreen(window_t *wnd, int fullscreen)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);

	if (wx11->mapped) {
		return send_fullscreen_message(wnd, fullscreen);
	}

	return set_fullscreen_property(wnd, fullscreen);
}

static int set_wm_protocols(window_t *wnd)
{
	display_x11_direct_t *dx11 = wnd->display->data;
	u32 protocols[]		   = {dx11->wm_delete_window};

	if (set_property_u32(wnd, dx11->wm_protocols, XA_ATOM, protocols, X_CHANGE_PROPERTY_ITEM_COUNT)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to set WM protocols");
		return 1;
	}

	return 0;
}

static int init_keys(display_t *display)
{
	display_x11_direct_t *dx11 = display->data;

	if (dx11->min_keycode > dx11->max_keycode) {
		log_error("cdisplay", "display_x11_direct", NULL, "invalid keycode range");
		return 1;
	}

	u8 keycode_count				= (u8)(dx11->max_keycode - dx11->min_keycode + 1);
	u8 request[X_GET_KEYBOARD_MAPPING_REQUEST_SIZE] = {0};

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_GET_KEYBOARD_MAPPING);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_GET_KEYBOARD_MAPPING_REQUEST_WORDS);
	cbuf_write_u8le(request, &off, dx11->min_keycode);
	cbuf_write_u8le(request, &off, keycode_count);
	cbuf_write_u16le(request, &off, 0);

	u8 reply[X11_REPLY_SIZE] = {0};
	if (sock_write_all(display->ss, dx11->sock, request, sizeof(request)) ||
	    sock_read_all(display->ss, dx11->sock, reply, sizeof(reply))) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to get keyboard mapping");
		return 1;
	}

	if (reply[0] != X11_REPLY_SUCCESS) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to get keyboard mapping");
		return 1;
	}

	u8 keysyms_per_keycode = reply[X_GET_KEYBOARD_MAPPING_KEYSYM_COUNT_OFFSET];
	u32 reply_words;
	cbuf_get_u32le(reply, X_GET_KEYBOARD_MAPPING_LENGTH_OFFSET, &reply_words);
	size_t keysyms_size  = (size_t)reply_words * X11_PAD_SIZE;
	size_t expected_size = (size_t)keycode_count * keysyms_per_keycode * sizeof(u32);
	if (keysyms_size != expected_size || keysyms_per_keycode == 0) {
		log_error("cdisplay", "display_x11_direct", NULL, "invalid keyboard mapping");
		return 1;
	}

	u8 *data = alloc_alloc(&display->alloc, keysyms_size);
	if (data == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	if (sock_read_all(display->ss, dx11->sock, data, keysyms_size)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to read keyboard mapping");
		alloc_free(&display->alloc, data, keysyms_size);
		return 1;
	}

	for (u8 i = 0; i < keycode_count; i++) {
		for (u8 j = 0; j < keysyms_per_keycode; j++) {
			u32 keysym;
			cbuf_get_u32le(data, ((size_t)i * keysyms_per_keycode + j) * sizeof(u32), &keysym);
			display_key_t key = x11_key_from_keysym(keysym);
			if (key != DISPLAY_KEY_UNKNOWN) {
				dx11->keys[dx11->min_keycode + i] = key;
				break;
			}
		}
	}

	alloc_free(&display->alloc, data, keysyms_size);

	return 0;
}

static int init_modifiers(display_t *display)
{
	display_x11_direct_t *dx11			= display->data;
	u8 request[X_GET_MODIFIER_MAPPING_REQUEST_SIZE] = {0};

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_GET_MODIFIER_MAPPING);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_GET_MODIFIER_MAPPING_REQUEST_WORDS);

	u8 reply[X11_REPLY_SIZE] = {0};
	if (sock_write_all(display->ss, dx11->sock, request, sizeof(request)) ||
	    sock_read_all(display->ss, dx11->sock, reply, sizeof(reply))) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to get modifier mapping");
		return 1;
	}

	if (reply[0] != X11_REPLY_SUCCESS) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to get modifier mapping");
		return 1;
	}

	u8 keycodes_per_modifier = reply[X_GET_MODIFIER_MAPPING_KEYCODE_COUNT_OFFSET];
	u32 reply_words;
	cbuf_get_u32le(reply, X_GET_MODIFIER_MAPPING_LENGTH_OFFSET, &reply_words);
	size_t keycodes_size = (size_t)reply_words * X11_PAD_SIZE;
	size_t expected_size = (size_t)X_MODIFIER_COUNT * keycodes_per_modifier;
	if (keycodes_size != expected_size) {
		log_error("cdisplay", "display_x11_direct", NULL, "invalid modifier mapping");
		return 1;
	}

	if (keycodes_size == 0) {
		return 0;
	}

	u8 *keycodes = alloc_alloc(&display->alloc, keycodes_size);
	if (keycodes == NULL) {
		return 1;
	}

	if (sock_read_all(display->ss, dx11->sock, keycodes, keycodes_size)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to read modifier mapping");
		alloc_free(&display->alloc, keycodes, keycodes_size);
		return 1;
	}

	for (u8 i = 0; i < X_MODIFIER_COUNT; i++) {
		for (u8 j = 0; j < keycodes_per_modifier; j++) {
			u8 keycode = keycodes[(size_t)i * keycodes_per_modifier + j];
			if (keycode == 0) {
				continue;
			}
			dx11->modifiers[i] = (display_modifier_t)(dx11->modifiers[i] | x11_modifier_from_key(dx11->keys[keycode]));
		}
	}

	alloc_free(&display->alloc, keycodes, keycodes_size);

	return 0;
}

static int parse_x11_event(display_t *display, const u8 *data, display_event_t *event)
{
	display_x11_direct_t *dx11 = display->data;

	*event = (display_event_t){0};

	u8 type = data[0] & X11_EVENT_TYPE_MASK;
	size_t off;
	u32 id;

	switch (type) {
	case X_EVENT_KEY_PRESS:
	case X_EVENT_KEY_RELEASE:
	case X_EVENT_BUTTON_PRESS:
	case X_EVENT_BUTTON_RELEASE: {
		u16 modifiers;
		off = X_KEY_BUTTON_EVENT_WINDOW_OFFSET;
		cbuf_read_u32le(data, &off, &id);
		event->window = id;
		off	      = X_KEY_BUTTON_EVENT_POSITION_OFFSET;
		cbuf_read_u16le(data, &off, &event->x);
		cbuf_read_u16le(data, &off, &event->y);
		off = X_KEY_BUTTON_EVENT_STATE_OFFSET;
		cbuf_read_u16le(data, &off, &modifiers);
		event->modifiers = x11_modifiers_from_state(dx11->modifiers, modifiers);
		if (type == X_EVENT_KEY_PRESS || type == X_EVENT_KEY_RELEASE) {
			event->type = type == X_EVENT_KEY_PRESS ? DISPLAY_EVENT_KEY_DOWN : DISPLAY_EVENT_KEY_UP;
			event->key  = dx11->keys[data[X_KEY_BUTTON_EVENT_DETAIL_OFFSET]];
			if (event->key == DISPLAY_KEY_UNKNOWN) {
				log_warn("cdisplay",
					 "display_x11_direct",
					 NULL,
					 "unknown X11 keycode: %u",
					 data[X_KEY_BUTTON_EVENT_DETAIL_OFFSET]);
			}
		} else {
			event->type   = type == X_EVENT_BUTTON_PRESS ? DISPLAY_EVENT_MOUSE_DOWN : DISPLAY_EVENT_MOUSE_UP;
			event->button = x11_mouse_from_button(data[X_KEY_BUTTON_EVENT_DETAIL_OFFSET], "display_x11_direct");
		}
		return 0;
	}
	case X_EVENT_MOTION_NOTIFY: {
		u16 modifiers;
		off = X_MOTION_EVENT_WINDOW_OFFSET;
		cbuf_read_u32le(data, &off, &id);
		event->window = id;
		off	      = X_MOTION_EVENT_POSITION_OFFSET;
		cbuf_read_u16le(data, &off, &event->x);
		cbuf_read_u16le(data, &off, &event->y);
		off = X_MOTION_EVENT_STATE_OFFSET;
		cbuf_read_u16le(data, &off, &modifiers);
		event->modifiers = x11_modifiers_from_state(dx11->modifiers, modifiers);
		event->type	 = DISPLAY_EVENT_MOUSE_MOVE;
		return 0;
	}
	case X_EVENT_FOCUS_IN:
	case X_EVENT_FOCUS_OUT: {
		off = X_FOCUS_EVENT_WINDOW_OFFSET;
		cbuf_read_u32le(data, &off, &id);
		event->window = id;
		event->type   = type == X_EVENT_FOCUS_IN ? DISPLAY_EVENT_FOCUS_GAINED : DISPLAY_EVENT_FOCUS_LOST;
		return 0;
	}
	case X_EVENT_EXPOSE: {
		return X11_EVENT_IGNORED;
	}
	case X_EVENT_DESTROY_NOTIFY: {
		off = X_DESTROY_EVENT_WINDOW_OFFSET;
		cbuf_read_u32le(data, &off, &id);
		event->window = id;
		event->type   = DISPLAY_EVENT_CLOSE;
		return 0;
	}
	case X_EVENT_UNMAP_NOTIFY: {
		return X11_EVENT_IGNORED;
	}
	case X_EVENT_MAP_NOTIFY: {
		return X11_EVENT_IGNORED;
	}
	case X_EVENT_REPARENT_NOTIFY: {
		return X11_EVENT_IGNORED;
	}
	case X_EVENT_CONFIGURE_NOTIFY: {
		off = X_CONFIGURE_EVENT_WINDOW_OFFSET;
		cbuf_read_u32le(data, &off, &id);
		event->window = id;
		off	      = X_CONFIGURE_EVENT_GEOMETRY_OFFSET;
		cbuf_read_u16le(data, &off, &event->x);
		cbuf_read_u16le(data, &off, &event->y);
		cbuf_read_u16le(data, &off, &event->width);
		cbuf_read_u16le(data, &off, &event->height);
		event->type = DISPLAY_EVENT_RESIZE;
		return 0;
	}
	case X_EVENT_CLIENT_MESSAGE: {
		off = X_CLIENT_MESSAGE_WINDOW_OFFSET;
		cbuf_read_u32le(data, &off, &id);
		u32 message_type;
		cbuf_read_u32le(data, &off, &message_type);
		off = X_CLIENT_MESSAGE_DATA_OFFSET;
		u32 message;
		cbuf_read_u32le(data, &off, &message);
		if (data[X_CLIENT_MESSAGE_FORMAT_OFFSET] == X_PROPERTY_FORMAT_32 && message_type == dx11->wm_protocols &&
		    message == dx11->wm_delete_window) {
			event->window = id;
			event->type   = DISPLAY_EVENT_CLOSE;
			return 0;
		}
		return X11_EVENT_IGNORED;
	}
	case X_EVENT_MAPPING_NOTIFY: {
		return X11_EVENT_IGNORED;
	}
	default: {
		log_error("cdisplay", "display_x11_direct", NULL, "unsupported X11 event: %u", type);
		return 1;
	}
	}
}

static int read_x11_event(display_t *display, display_event_t *event)
{
	u8 data[X11_EVENT_SIZE]	   = {0};
	display_x11_direct_t *dx11 = display->data;

	if (sock_read_all(display->ss, dx11->sock, data, sizeof(data))) {
		return 1;
	}

	return parse_x11_event(display, data, event);
}

static int poll_x11_event(display_t *display, display_event_t *event)
{
	display_x11_direct_t *dx11 = display->data;
	while (dx11->event_used < sizeof(dx11->event_data)) {
		size_t n = 0;
		cerr_t err;
		do {
			err = sock_read(display->ss,
					dx11->sock,
					&dx11->event_data[dx11->event_used],
					sizeof(dx11->event_data) - dx11->event_used,
					&n);
		} while (err == CERR_INTERRUPT);

		if (err == CERR_AGAIN) {
			return X11_EVENT_NONE;
		}
		if (err != CERR_OK || n == 0) {
			log_error("cdisplay", "display_x11_direct", NULL, "failed to read X11 event: %s", cerr_str(err));
			dx11->event_used = 0;
			return 1;
		}
		dx11->event_used += n;
	}

	int ret		 = parse_x11_event(display, dx11->event_data, event);
	dx11->event_used = 0;
	return ret;
}

static int destroy_window(window_t *wnd)
{
	u8 request[X_WINDOW_ID_REQUEST_SIZE] = {0};

	window_x11_t *wx11 = display_x11_direct_window_data(wnd);

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_DESTROY_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_WINDOW_ID_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, wx11->id);

	const display_x11_direct_t *dx11 = wnd->display->data;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to destroy window");
		return 1;
	}

	return 0;
}

static int map_window(window_t *wnd)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);

	u8 request[X_WINDOW_ID_REQUEST_SIZE] = {0};
	size_t off			     = 0;
	cbuf_write_u8le(request, &off, X_MAP_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_WINDOW_ID_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, wx11->id);

	const display_x11_direct_t *dx11 = wnd->display->data;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to map window");
		return 1;
	}

	return 0;
}

static int unmap_window(window_t *wnd)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);

	u8 request[X_WINDOW_ID_REQUEST_SIZE] = {0};
	size_t off			     = 0;
	cbuf_write_u8le(request, &off, X_UNMAP_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_WINDOW_ID_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, wx11->id);

	const display_x11_direct_t *dx11 = wnd->display->data;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to unmap window");
		return 1;
	}

	return 0;
}

static int configure_window(window_t *wnd, u32 value_mask, const u32 *values, size_t value_count)
{
	u8 request[X_CONFIGURE_WINDOW_REQUEST_SIZE + X11_PAD_SIZE * 4] = {0};

	window_x11_t *wx11 = display_x11_direct_window_data(wnd);

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_CONFIGURE_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, (u16)(X_CONFIGURE_WINDOW_REQUEST_WORDS + value_count));
	cbuf_write_u32le(request, &off, wx11->id);
	cbuf_write_u32le(request, &off, value_mask);

	for (size_t i = 0; i < value_count; i++) {
		cbuf_write_u32le(request, &off, values[i]);
	}

	const display_x11_direct_t *dx11 = wnd->display->data;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, X_CONFIGURE_WINDOW_REQUEST_SIZE + value_count * X11_PAD_SIZE)) {
		log_error("cdisplay", "display_x11_direct", NULL, "failed to configure window");
		return 1;
	}

	return 0;
}

static int display_x11_direct_poll_events(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	display_x11_direct_t *dx11 = display->data;
	if (dx11 == NULL) {
		return 1;
	}

	int flags;
	if (sock_get_flags(display->ss, dx11->sock, &flags) || sock_set_flags(display->ss, dx11->sock, flags | X11_SOCKET_NONBLOCK)) {
		return 1;
	}

	int ret;
	display_event_t event = {0};
	do {
		ret = poll_x11_event(display, &event);
	} while (ret == X11_EVENT_IGNORED);

	ret = sock_set_flags(display->ss, dx11->sock, flags) ? 1 : ret;
	if (ret == 0) {
		display_emit_event(display, &event);
		return 0;
	}

	return ret == X11_EVENT_NONE ? 0 : ret;
}

static int display_x11_direct_wait_events(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	display_x11_direct_t *dx11 = display->data;
	if (dx11 == NULL) {
		return 1;
	}

	int ret;
	display_event_t event = {0};
	do {
		ret = read_x11_event(display, &event);
	} while (ret == X11_EVENT_IGNORED);

	if (ret == 0) {
		display_emit_event(display, &event);
	}

	return ret;
}

static int display_x11_direct_init(display_t *display)
{
	if (display == NULL || display->alloc.alloc == NULL) {
		return 1;
	}

	log_info("cdisplay", "display_x11_direct", NULL, "Initializing X11...");

	display->data = alloc_alloc(&display->alloc, sizeof(display_x11_direct_t));
	if (display->data == NULL) {
		return 1;
	}
	mem_set(display->data, 0, sizeof(display_x11_direct_t));

	display_x11_direct_t *dx11 = display->data;
	if (arr_init(&dx11->windows, 8, sizeof(window_x11_slot_t), display->alloc) == NULL || open_display(display)) {
		alloc_free(&display->alloc, dx11->monitors, dx11->monitor_count * sizeof(*dx11->monitors));
		alloc_free(&display->alloc, dx11->visuals, dx11->visual_count * sizeof(*dx11->visuals));
		buf_free(&dx11->request);
		arr_free(&dx11->windows);
		alloc_free(&display->alloc, display->data, sizeof(display_x11_direct_t));
		display->data = NULL;
		return 1;
	}

	if (init_keys(display) || init_modifiers(display) || init_atoms(display)) {
		sock_close(display->ss, dx11->sock);
		alloc_free(&display->alloc, dx11->monitors, dx11->monitor_count * sizeof(*dx11->monitors));
		alloc_free(&display->alloc, dx11->visuals, dx11->visual_count * sizeof(*dx11->visuals));
		buf_free(&dx11->request);
		arr_free(&dx11->windows);
		alloc_free(&display->alloc, display->data, sizeof(display_x11_direct_t));
		display->data = NULL;
		return 1;
	}

	return 0;
}

static int display_x11_direct_available(display_driver_t *driver, proc_t *proc)
{
	(void)driver;
	return proc != NULL && proc_getenv(proc, STRV("DISPLAY")).data != NULL;
}

static int display_x11_direct_free(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	display_x11_direct_t *dx11 = display->data;

	log_info("cdisplay", "display_x11_direct", NULL, "Freeing X11...");
	sock_close(display->ss, dx11->sock);
	alloc_free(&display->alloc, dx11->monitors, dx11->monitor_count * sizeof(*dx11->monitors));
	alloc_free(&display->alloc, dx11->visuals, dx11->visual_count * sizeof(*dx11->visuals));
	buf_free(&dx11->request);
	arr_free(&dx11->windows);

	alloc_free(&display->alloc, display->data, sizeof(display_x11_direct_t));

	return 0;
}

static int display_x11_direct_monitors(display_t *display, arr_t *monitors)
{
	if (display == NULL || display->data == NULL || monitors == NULL) {
		return 1;
	}

	display_x11_direct_t *dx11 = display->data;
	int randr		   = display_x11_direct_randr_monitors(display, monitors);
	if (randr != 2) {
		return randr;
	}

	if (arr_resize(monitors, (u32)dx11->monitor_count)) {
		return 1;
	}
	monitors->cnt = (u32)dx11->monitor_count;

	for (u32 i = 0; i < dx11->monitor_count; ++i) {
		display_monitor_t *monitor = arr_get(monitors, i);
		*monitor		   = dx11->monitors[i];
	}

	return 0;
}

static int display_x11_direct_window_init(window_t *wnd, const window_config_t *config)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->display->alloc.alloc == NULL || config == NULL) {
		return 1;
	}

	window_x11_t *wx11 = display_x11_direct_window_alloc(wnd);
	if (wx11 == NULL) {
		return 1;
	}
	wx11->x	     = config->x;
	wx11->y	     = config->y;
	wx11->width  = config->width;
	wx11->height = config->height;

	if (create_window(wnd, config)) {
		free_colormap(wnd);
		display_x11_direct_window_release(wnd);
		return 1;
	}

	if (set_wm_normal_hints(wnd, config) || set_wm_protocols(wnd)) {
		destroy_window(wnd);
		free_colormap(wnd);
		display_x11_direct_window_release(wnd);
		return 1;
	}

	return 0;
}

static int display_x11_direct_window_free(window_t *wnd)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	destroy_window(wnd);
	free_colormap(wnd);
	display_x11_direct_window_release(wnd);

	return 0;
}

static u32 display_x11_direct_window_id(window_t *wnd)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL) {
		return 0;
	}

	return wx11->id;
}

static int display_x11_direct_window_native(window_t *wnd, window_native_t *native)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL || native == NULL) {
		return 1;
	}

	native->type   = DISPLAY_NATIVE_X11;
	native->window = (void *)(uintptr_t)wx11->id;
	return wx11->id == 0;
}

static int display_x11_direct_window_set_title(window_t *wnd, strv_t title)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL || title.len >= sizeof(wx11->title) || (title.data == NULL && title.len > 0)) {
		return 1;
	}

	display_x11_direct_t *dx11 = wnd->display->data;
	if (set_property_text(wnd, dx11->wm_name, XA_STRING, title) ||
	    set_property_text(wnd, dx11->net_wm_name, dx11->utf8_string, title)) {
		return 1;
	}

	if (title.len > 0) {
		mem_copy(wx11->title, sizeof(wx11->title), title.data, title.len);
	}
	wx11->title[title.len] = 0;
	return 0;
}

static int display_x11_direct_window_get_title(window_t *wnd, char *title, size_t size)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL || title == NULL || size == 0) {
		return 1;
	}

	size_t len = 0;
	while (len < sizeof(wx11->title) && wx11->title[len] != 0) {
		len++;
	}
	if (len >= size) {
		return 1;
	}

	mem_copy(title, size, wx11->title, len + 1);
	return 0;
}

static int display_x11_direct_window_set_position(window_t *wnd, u16 x, u16 y)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	u32 values[] = {x, y};

	if (configure_window(wnd, X_CONFIG_WINDOW_X | X_CONFIG_WINDOW_Y, values, 2)) {
		return 1;
	}

	wx11->x = x;
	wx11->y = y;
	return 0;
}

static int display_x11_direct_window_get_position(window_t *wnd, u16 *x, u16 *y)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL || x == NULL || y == NULL) {
		return 1;
	}

	*x = wx11->x;
	*y = wx11->y;
	return 0;
}

static int display_x11_direct_window_set_size(window_t *wnd, u16 width, u16 height)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	u32 values[] = {width, height};

	if (configure_window(wnd, X_CONFIG_WINDOW_WIDTH | X_CONFIG_WINDOW_HEIGHT, values, 2)) {
		return 1;
	}

	wx11->width  = width;
	wx11->height = height;
	return 0;
}

static int display_x11_direct_window_get_size(window_t *wnd, u16 *width, u16 *height)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL || width == NULL || height == NULL) {
		return 1;
	}

	*width	= wx11->width;
	*height = wx11->height;
	return 0;
}

static int display_x11_direct_window_set_borderless(window_t *wnd, int borderless)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	if (set_borderless(wnd, borderless)) {
		return 1;
	}

	wx11->borderless = borderless != 0;
	return 0;
}

static int display_x11_direct_window_get_borderless(window_t *wnd, int *borderless)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL || borderless == NULL) {
		return 1;
	}

	*borderless = wx11->borderless;
	return 0;
}

static int display_x11_direct_window_set_fullscreen(window_t *wnd, int fullscreen)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	if (set_fullscreen(wnd, fullscreen)) {
		return 1;
	}

	wx11->fullscreen = fullscreen != 0;
	return 0;
}

static int display_x11_direct_window_get_fullscreen(window_t *wnd, int *fullscreen)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL || fullscreen == NULL) {
		return 1;
	}

	*fullscreen = wx11->fullscreen;
	return 0;
}

static int display_x11_direct_window_show(window_t *wnd)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	if (map_window(wnd)) {
		return 1;
	}

	wx11->mapped = 1;
	return 0;
}

static int display_x11_direct_window_hide(window_t *wnd)
{
	window_x11_t *wx11 = display_x11_direct_window_data(wnd);
	if (wx11 == NULL) {
		return 1;
	}

	if (unmap_window(wnd)) {
		return 1;
	}

	wx11->mapped = 0;
	return 0;
}

static display_driver_t display_x11_direct = {
	.name		       = "X11-direct",
	.available	       = display_x11_direct_available,
	.init		       = display_x11_direct_init,
	.free		       = display_x11_direct_free,
	.poll_events	       = display_x11_direct_poll_events,
	.wait_events	       = display_x11_direct_wait_events,
	.monitors	       = display_x11_direct_monitors,
	.window_init	       = display_x11_direct_window_init,
	.window_free	       = display_x11_direct_window_free,
	.window_id	       = display_x11_direct_window_id,
	.window_native	       = display_x11_direct_window_native,
	.window_set_title      = display_x11_direct_window_set_title,
	.window_get_title      = display_x11_direct_window_get_title,
	.window_set_position   = display_x11_direct_window_set_position,
	.window_get_position   = display_x11_direct_window_get_position,
	.window_set_size       = display_x11_direct_window_set_size,
	.window_get_size       = display_x11_direct_window_get_size,
	.window_set_borderless = display_x11_direct_window_set_borderless,
	.window_get_borderless = display_x11_direct_window_get_borderless,
	.window_set_fullscreen = display_x11_direct_window_set_fullscreen,
	.window_get_fullscreen = display_x11_direct_window_get_fullscreen,
	.window_show	       = display_x11_direct_window_show,
	.window_hide	       = display_x11_direct_window_hide,
	.ext_init	       = display_x11_direct_ext_init,
	.ext_send	       = display_x11_direct_ext_send,
	.ext_call	       = display_x11_direct_ext_call,
	.alloc_id	       = display_x11_direct_alloc_id,
	.visual_depth	       = display_x11_direct_visual_depth,
};

DISPLAY_DRIVER(display_x11_direct, &display_x11_direct);
