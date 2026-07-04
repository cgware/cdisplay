#include "display_driver.h"

#include "cbuf.h"
#include "cerr.h"
#include "log.h"
#include "mem.h"

typedef struct display_x11_s {
	void *sock;
	u32 resource_id_base;
	u32 resource_id_mask;
	u32 next_resource;
	u32 root;
	u32 white_pixel;
	u32 black_pixel;
	u32 wm_protocols;
	u32 wm_delete_window;
	u32 wm_name;
	u32 net_wm_name;
	u32 utf8_string;
} display_x11_t;

typedef struct window_x11_s {
	u32 id;
} window_x11_t;

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
	X11_SETUP_SCREEN_LIST_OFFSET	  = 32,
	X11_SCREEN_DEFAULT_COLORMAP_SIZE  = 4,
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

static size_t pad4(size_t length)
{
	return (X11_PAD_SIZE - (length & (X11_PAD_SIZE - 1))) & (X11_PAD_SIZE - 1);
}

static int open_display(display_t *d)
{
	display_x11_t *dx11 = d->data;

	strv_t name = proc_getenv(d->proc, STRV("DISPLAY"));
	if (name.data == NULL) {
		log_error("cwindow", "awindow_x11", NULL, "failed to get display name");
		return 1;
	}

	if (name.data[0] != ':') {
		log_error("cwindow", "awindow_x11", NULL, "invalid display name: %.*s", name.len, name.data);
		return 1;
	}

	strv_t display_name = STRVN(&name.data[1], name.len - 1);
	strv_t display_number_str;
	if (strv_lsplit(display_name, '.', &display_number_str, NULL) || strv_to_int(display_number_str, NULL)) {
		log_error("cwindow", "awindow_x11", NULL, "invalid display number: %.*s", display_number_str.len, display_number_str.data);
		return 1;
	}

	cerr_t err = sock_open(d->ss, SOCK_FAMILY_UNIX, SOCK_TYPE_STREAM, 0, &dx11->sock);

	if (err != CERR_OK) {
		log_error("cwindow", "awindow_x11", NULL, "failed to open unix socket");
		return 1;
	}

	char buf[256] = {0};
	str_t path    = STRB(buf, 0);
	str_cat(&path, STRV("/tmp/.X11-unix/X"));
	str_cat(&path, display_number_str);

	err = sock_connect(d->ss, dx11->sock, SOCK_FAMILY_UNIX, path.data, path.len + 1);
	if (err != CERR_OK) {
		log_error("cwindow", "awindow_x11", NULL, "failed to connect to unix socket");
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
		log_error("cwindow", "awindow_x11", NULL, "failed to read xauthority");
		buf_free(&xauthority);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	char hostname_buf[256] = {0};
	if (proc_gethostname(d->proc, hostname_buf, sizeof(hostname_buf) - 1)) {
		log_error("cwindow", "awindow_x11", NULL, "failed to get hostname");
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
		log_error("cwindow", "awindow_x11", NULL, "malformed authority");
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
	    sock_write_all(d->ss, dx11->sock, padding, pad4(auth_name.len)) || sock_write_all(d->ss, dx11->sock, cookie, cookie_length) ||
	    sock_write_all(d->ss, dx11->sock, padding, pad4(cookie_length)) ||
	    sock_read_all(d->ss, dx11->sock, b.data, X11_SETUP_REPLY_HEADER_SIZE)) {
		log_error("cwindow", "awindow_x11", NULL, "failed to send request");
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
		log_error("cwindow", "awindow_x11", NULL, "connection setup was not successful");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	off = X11_SETUP_PROTOCOL_MAJOR_OFFSET;
	u16 major_version;
	buf_read_u16le(&b, &off, &major_version);
	u16 minor_version;
	buf_read_u16le(&b, &off, &minor_version);
	log_info("cwindow", "awindow_x11", NULL, "X11 protocol version: %u.%u", major_version, minor_version);

	u16 extra_words;
	buf_read_u16le(&b, &off, &extra_words);
	size_t setup_length = (size_t)extra_words * X11_PAD_SIZE;

	if (setup_length < X11_SETUP_MIN_SIZE) {
		log_error("cwindow", "awindow_x11", NULL, "invalid setup length: %zu", setup_length);
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	if (buf_resize(&b, setup_length)) {
		log_error("cwindow", "awindow_x11", NULL, "failed to allocate setup");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	if (sock_read_all(d->ss, dx11->sock, b.data, setup_length)) {
		log_error("cwindow", "awindow_x11", NULL, "failed to read setup");
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

	size_t screen_offset = X11_SETUP_SCREEN_LIST_OFFSET + vendor_length + pad4(vendor_length) + (size_t)format_count * X11_FORMAT_SIZE;

	if (screen_count == 0) {
		log_error("cwindow", "awindow_x11", NULL, "no screens found");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	if (screen_offset + X11_SCREEN_MIN_SIZE > setup_length) {
		log_error("cwindow", "awindow_x11", NULL, "invalid screen offset: %zu", screen_offset);
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	off = screen_offset;
	buf_read_u32le(&b, &off, &dx11->root);
	off += X11_SCREEN_DEFAULT_COLORMAP_SIZE;
	buf_read_u32le(&b, &off, &dx11->white_pixel);
	buf_read_u32le(&b, &off, &dx11->black_pixel);

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
	X_CREATE_WINDOW	   = 1,
	X_DESTROY_WINDOW   = 4,
	X_MAP_WINDOW	   = 8,
	X_UNMAP_WINDOW	   = 10,
	X_CONFIGURE_WINDOW = 12,
	X_INTERN_ATOM	   = 16,
	X_CHANGE_PROPERTY  = 18,
};

enum {
	XA_ATOM	  = 4,
	XA_STRING = 31,
};

enum {
	X_COPY_FROM_PARENT = 0,
	X_INPUT_OUTPUT	   = 1,
};

enum {
	X_CHANGE_PROPERTY_MODE_REPLACE = 0,
	X_PROPERTY_FORMAT_32	       = 32,
};

enum {
	X_CW_BACK_PIXEL	  = 1u << 1,
	X_CW_BORDER_PIXEL = 1u << 3,
	X_CW_EVENT_MASK	  = 1u << 11,
};

enum {
	X_EVENT_MASK_KEY_PRESS	    = 1u << 0,
	X_EVENT_MASK_KEY_RELEASE    = 1u << 1,
	X_EVENT_MASK_BUTTON_PRESS   = 1u << 2,
	X_EVENT_MASK_BUTTON_RELEASE = 1u << 3,
	X_EVENT_MASK_POINTER_MOTION = 1u << 6,
	X_EVENT_MASK_EXPOSURE	    = 1u << 15,
	X_EVENT_MASK_STRUCTURE	    = 1u << 17,
	X_EVENT_MASK_FOCUS_CHANGE   = 1u << 21,
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
};

enum {
	X_CREATE_WINDOW_REQUEST_SIZE	= 32,
	X_CREATE_WINDOW_REQUEST_WORDS	= X_CREATE_WINDOW_REQUEST_SIZE / X11_PAD_SIZE,
	X_CREATE_WINDOW_MAX_VALUE_COUNT = 15,
	X_DEFAULT_WINDOW_BORDER_WIDTH	= 1,
};

enum {
	X_INTERN_ATOM_REQUEST_SIZE	= 8,
	X_INTERN_ATOM_NAME_MAX		= 64,
	X_INTERN_ATOM_REPLY_ATOM_OFFSET = 8,
};

enum {
	X_CHANGE_PROPERTY_REQUEST_SIZE	= 28,
	X_CHANGE_PROPERTY_HEADER_SIZE	= 24,
	X_CHANGE_PROPERTY_MAX_DATA_SIZE = X11_PAD_SIZE * 0xffff - X_CHANGE_PROPERTY_HEADER_SIZE,
	X_CHANGE_PROPERTY_REQUEST_WORDS = X_CHANGE_PROPERTY_REQUEST_SIZE / X11_PAD_SIZE,
	X_CHANGE_PROPERTY_ITEM_COUNT	= 1,
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

static int create_window(window_t *wnd, u16 x, u16 y, u16 width, u16 height)
{
	u8 request[X_CREATE_WINDOW_REQUEST_SIZE + X_CREATE_WINDOW_MAX_VALUE_COUNT * X11_PAD_SIZE] = {0};
	u32 values[X_CREATE_WINDOW_MAX_VALUE_COUNT]						  = {0};
	size_t value_count									  = 0;

	display_x11_t *dx11 = wnd->display->data;

	u32 background_pixel = dx11->white_pixel;
	u32 border_pixel     = dx11->black_pixel;

	u32 event_mask = X_EVENT_MASK_KEY_PRESS | X_EVENT_MASK_KEY_RELEASE | X_EVENT_MASK_BUTTON_PRESS | X_EVENT_MASK_BUTTON_RELEASE |
			 X_EVENT_MASK_POINTER_MOTION | X_EVENT_MASK_EXPOSURE | X_EVENT_MASK_STRUCTURE | X_EVENT_MASK_FOCUS_CHANGE;

	values[value_count++] = background_pixel;
	values[value_count++] = border_pixel;
	values[value_count++] = event_mask;

	uint shift   = ctz32(dx11->resource_id_mask);
	u32 capacity = dx11->resource_id_mask >> shift;

	if (capacity < dx11->next_resource) {
		log_error("cwindow", "awindow_x11", NULL, "failed to allocate resource");
		return 1;
	}

	u32 resource_bits = dx11->next_resource << shift;

	window_x11_t *wx11 = wnd->data;

	wx11->id = dx11->resource_id_base | resource_bits;

	dx11->next_resource++;

	u8 depth = X_COPY_FROM_PARENT;

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_CREATE_WINDOW);
	cbuf_write_u8le(request, &off, depth);

	u16 length = X_CREATE_WINDOW_REQUEST_WORDS + value_count;
	cbuf_write_u16le(request, &off, length);
	cbuf_write_u32le(request, &off, wx11->id);

	u32 parent = dx11->root;
	cbuf_write_u32le(request, &off, parent);

	cbuf_write_u16le(request, &off, x);
	cbuf_write_u16le(request, &off, y);

	cbuf_write_u16le(request, &off, width);
	cbuf_write_u16le(request, &off, height);

	u16 border_width = X_DEFAULT_WINDOW_BORDER_WIDTH;
	cbuf_write_u16le(request, &off, border_width);

	u16 window_class = X_INPUT_OUTPUT;
	cbuf_write_u16le(request, &off, window_class);

	u32 visual = X_COPY_FROM_PARENT;
	cbuf_write_u32le(request, &off, visual);

	u32 value_mask = X_CW_BACK_PIXEL | X_CW_BORDER_PIXEL | X_CW_EVENT_MASK;
	cbuf_write_u32le(request, &off, value_mask);

	for (size_t i = 0; i < value_count; i++) {
		cbuf_write_u32le(request, &off, values[i]);
	}

	size_t request_size = X_CREATE_WINDOW_REQUEST_SIZE + value_count * X11_PAD_SIZE;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, request_size)) {
		log_error("cwindow", "awindow_x11", NULL, "failed to create window");
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
	cbuf_write_u16le(request, &off, (u16)(X_INTERN_ATOM_REQUEST_SIZE / X11_PAD_SIZE + (name.len + pad4(name.len)) / X11_PAD_SIZE));
	cbuf_write_u16le(request, &off, name.len);
	cbuf_write_u16le(request, &off, 0);
	mem_copy(&request[off], sizeof(request) - off, name.data, name.len);
	off += name.len + pad4(name.len);

	display_x11_t *dx11 = display->data;
	if (sock_write_all(display->ss, dx11->sock, request, off) || sock_read_all(display->ss, dx11->sock, reply, sizeof(reply))) {
		log_error("cwindow", "display_x11", NULL, "failed to intern atom: %.*s", name.len, name.data);
		return 1;
	}

	if (reply[0] != X11_REPLY_SUCCESS) {
		log_error("cwindow", "display_x11", NULL, "failed to intern atom: %.*s", name.len, name.data);
		return 1;
	}

	cbuf_get_u32le(reply, X_INTERN_ATOM_REPLY_ATOM_OFFSET, atom);
	if (*atom == 0) {
		log_error("cwindow", "display_x11", NULL, "atom not found: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

static int init_atoms(display_t *display)
{
	display_x11_t *dx11 = display->data;

	if (intern_atom(display, STRV("WM_PROTOCOLS"), &dx11->wm_protocols) ||
	    intern_atom(display, STRV("WM_DELETE_WINDOW"), &dx11->wm_delete_window) ||
	    intern_atom(display, STRV("WM_NAME"), &dx11->wm_name) || intern_atom(display, STRV("_NET_WM_NAME"), &dx11->net_wm_name) ||
	    intern_atom(display, STRV("UTF8_STRING"), &dx11->utf8_string)) {
		return 1;
	}

	return 0;
}

static int set_property_text(window_t *wnd, u32 property, u32 type, strv_t text)
{
	if (text.data == NULL && text.len > 0) {
		return 1;
	}

	if (text.len > X_CHANGE_PROPERTY_MAX_DATA_SIZE) {
		log_error("cwindow", "display_x11", NULL, "property text is too long");
		return 1;
	}

	size_t data_size     = text.len;
	size_t request_size  = X_CHANGE_PROPERTY_HEADER_SIZE + data_size + pad4(data_size);
	size_t request_words = request_size / X11_PAD_SIZE;

	u8 *request = mem_alloc(request_size);
	if (request == NULL) {
		return 1;
	}

	mem_set(request, 0, request_size);

	window_x11_t *wx11 = wnd->data;

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_CHANGE_PROPERTY);
	cbuf_write_u8le(request, &off, X_CHANGE_PROPERTY_MODE_REPLACE);
	cbuf_write_u16le(request, &off, (u16)request_words);
	cbuf_write_u32le(request, &off, wx11->id);
	cbuf_write_u32le(request, &off, property);
	cbuf_write_u32le(request, &off, type);
	cbuf_write_u8le(request, &off, 8);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u32le(request, &off, (u32)text.len);
	if (text.len > 0) {
		mem_copy(&request[off], request_size - off, text.data, text.len);
	}

	display_x11_t *dx11 = wnd->display->data;
	int ret		    = sock_write_all(wnd->display->ss, dx11->sock, request, request_size);
	mem_free(request, request_size);

	if (ret) {
		log_error("cwindow", "display_x11", NULL, "failed to set window text property");
		return 1;
	}

	return 0;
}

static int set_wm_protocols(window_t *wnd)
{
	u8 request[X_CHANGE_PROPERTY_REQUEST_SIZE] = {0};
	size_t off				   = 0;

	window_x11_t *wx11  = wnd->data;
	display_x11_t *dx11 = wnd->display->data;

	cbuf_write_u8le(request, &off, X_CHANGE_PROPERTY);
	cbuf_write_u8le(request, &off, X_CHANGE_PROPERTY_MODE_REPLACE);
	cbuf_write_u16le(request, &off, X_CHANGE_PROPERTY_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, wx11->id);
	cbuf_write_u32le(request, &off, dx11->wm_protocols);
	cbuf_write_u32le(request, &off, XA_ATOM);
	cbuf_write_u8le(request, &off, X_PROPERTY_FORMAT_32);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u32le(request, &off, X_CHANGE_PROPERTY_ITEM_COUNT);
	cbuf_write_u32le(request, &off, dx11->wm_delete_window);

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cwindow", "display_x11", NULL, "failed to set WM protocols");
		return 1;
	}

	return 0;
}

static int read_x11_event(display_t *display, display_event_t *event)
{
	u8 data[X11_EVENT_SIZE] = {0};
	display_x11_t *dx11	= display->data;

	*event = (display_event_t){0};

	if (sock_read_all(display->ss, dx11->sock, data, sizeof(data))) {
		return 1;
	}

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
		event->modifiers = modifiers;
		if (type == X_EVENT_KEY_PRESS || type == X_EVENT_KEY_RELEASE) {
			event->type = type == X_EVENT_KEY_PRESS ? DISPLAY_EVENT_KEY_DOWN : DISPLAY_EVENT_KEY_UP;
			event->key  = data[X_KEY_BUTTON_EVENT_DETAIL_OFFSET];
		} else {
			event->type   = type == X_EVENT_BUTTON_PRESS ? DISPLAY_EVENT_MOUSE_DOWN : DISPLAY_EVENT_MOUSE_UP;
			event->button = data[X_KEY_BUTTON_EVENT_DETAIL_OFFSET];
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
		event->modifiers = modifiers;
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
	default: {
		log_error("cwindow", "display_x11", NULL, "unsupported X11 event: %u", type);
		return 1;
	}
	}
}

static int destroy_window(window_t *wnd)
{
	u8 request[X_WINDOW_ID_REQUEST_SIZE] = {0};

	window_x11_t *wx11 = wnd->data;

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_DESTROY_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_WINDOW_ID_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, wx11->id);

	const display_x11_t *dx11 = wnd->display->data;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cwindow", "awindow_x11", NULL, "failed to destroy window");
		return 1;
	}

	return 0;
}

static int map_window(window_t *wnd)
{
	window_x11_t *wx11 = wnd->data;

	u8 request[X_WINDOW_ID_REQUEST_SIZE] = {0};
	size_t off			     = 0;
	cbuf_write_u8le(request, &off, X_MAP_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_WINDOW_ID_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, wx11->id);

	const display_x11_t *dx11 = wnd->display->data;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cwindow", "awindow_x11", NULL, "failed to map window");
		return 1;
	}

	return 0;
}

static int unmap_window(window_t *wnd)
{
	window_x11_t *wx11 = wnd->data;

	u8 request[X_WINDOW_ID_REQUEST_SIZE] = {0};
	size_t off			     = 0;
	cbuf_write_u8le(request, &off, X_UNMAP_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, X_WINDOW_ID_REQUEST_WORDS);
	cbuf_write_u32le(request, &off, wx11->id);

	const display_x11_t *dx11 = wnd->display->data;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cwindow", "awindow_x11", NULL, "failed to unmap window");
		return 1;
	}

	return 0;
}

static int configure_window(window_t *wnd, u32 value_mask, const u32 *values, size_t value_count)
{
	u8 request[X_CONFIGURE_WINDOW_REQUEST_SIZE + X11_PAD_SIZE * 4] = {0};

	window_x11_t *wx11 = wnd->data;

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_CONFIGURE_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, (u16)(X_CONFIGURE_WINDOW_REQUEST_WORDS + value_count));
	cbuf_write_u32le(request, &off, wx11->id);
	cbuf_write_u32le(request, &off, value_mask);

	for (size_t i = 0; i < value_count; i++) {
		cbuf_write_u32le(request, &off, values[i]);
	}

	const display_x11_t *dx11 = wnd->display->data;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, X_CONFIGURE_WINDOW_REQUEST_SIZE + value_count * X11_PAD_SIZE)) {
		log_error("cwindow", "display_x11", NULL, "failed to configure window");
		return 1;
	}

	return 0;
}

static int display_x11_poll_event(display_t *display, display_event_t *event)
{
	if (display == NULL || event == NULL) {
		return 1;
	}

	display_x11_t *dx11 = display->data;
	int flags;
	if (sock_get_flags(display->ss, dx11->sock, &flags) || sock_set_flags(display->ss, dx11->sock, flags | X11_SOCKET_NONBLOCK)) {
		return 1;
	}

	int ret;
	do {
		ret = read_x11_event(display, event);
	} while (ret == X11_EVENT_IGNORED);

	ret = sock_set_flags(display->ss, dx11->sock, flags) ? 1 : ret;

	return ret;
}

static int display_x11_wait_event(display_t *display, display_event_t *event)
{
	if (display == NULL || event == NULL) {
		return 1;
	}

	int ret;
	do {
		ret = read_x11_event(display, event);
	} while (ret == X11_EVENT_IGNORED);

	return ret;
}

static int display_x11_init(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	log_info("cwindow", "awindow_x11", NULL, "Initializing X11...\n");

	display->data = mem_alloc(sizeof(display_x11_t));
	if (display->data == NULL) {
		return 1;
	}

	if (open_display(display)) {
		mem_free(display->data, sizeof(display_x11_t));
		display->data = NULL;
		return 1;
	}

	if (init_atoms(display)) {
		display_x11_t *dx11 = display->data;
		sock_close(display->ss, dx11->sock);
		mem_free(display->data, sizeof(display_x11_t));
		display->data = NULL;
		return 1;
	}

	return 0;
}

static int display_x11_free(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	display_x11_t *dx11 = display->data;

	log_info("cwindow", "display_x11", NULL, "Freeing X11...\n");
	sock_close(display->ss, dx11->sock);

	mem_free(display->data, sizeof(display_x11_t));

	return 0;
}

static int display_x11_window_init(window_t *wnd, u16 x, u16 y, u16 width, u16 height)
{
	if (wnd == NULL) {
		return 1;
	}

	wnd->data = mem_alloc(sizeof(window_x11_t));
	if (wnd->data == NULL) {
		return 1;
	}

	if (create_window(wnd, x, y, width, height)) {
		mem_free(wnd->data, sizeof(window_x11_t));
		wnd->data = NULL;
		return 1;
	}

	if (set_wm_protocols(wnd)) {
		destroy_window(wnd);
		mem_free(wnd->data, sizeof(window_x11_t));
		wnd->data = NULL;
		return 1;
	}

	return 0;
}

static int display_x11_window_free(window_t *wnd)
{
	if (wnd == NULL) {
		return 1;
	}

	destroy_window(wnd);
	mem_free(wnd->data, sizeof(window_x11_t));

	return 0;
}

static u32 display_x11_window_id(window_t *wnd)
{
	if (wnd == NULL || wnd->data == NULL) {
		return 0;
	}

	window_x11_t *wx11 = wnd->data;
	return wx11->id;
}

static int display_x11_window_set_title(window_t *wnd, strv_t title)
{
	if (wnd == NULL || wnd->data == NULL) {
		return 1;
	}

	display_x11_t *dx11 = wnd->display->data;
	if (set_property_text(wnd, dx11->wm_name, XA_STRING, title) ||
	    set_property_text(wnd, dx11->net_wm_name, dx11->utf8_string, title)) {
		return 1;
	}

	return 0;
}

static int display_x11_window_set_position(window_t *wnd, u16 x, u16 y)
{
	if (wnd == NULL || wnd->data == NULL) {
		return 1;
	}

	u32 values[] = {x, y};

	return configure_window(wnd, X_CONFIG_WINDOW_X | X_CONFIG_WINDOW_Y, values, 2);
}

static int display_x11_window_set_size(window_t *wnd, u16 width, u16 height)
{
	if (wnd == NULL || wnd->data == NULL) {
		return 1;
	}

	u32 values[] = {width, height};

	return configure_window(wnd, X_CONFIG_WINDOW_WIDTH | X_CONFIG_WINDOW_HEIGHT, values, 2);
}

static int display_x11_window_show(window_t *wnd)
{
	if (wnd == NULL || wnd->data == NULL) {
		return 1;
	}

	return map_window(wnd);
}

static int display_x11_window_hide(window_t *wnd)
{
	if (wnd == NULL || wnd->data == NULL) {
		return 1;
	}

	return unmap_window(wnd);
}

static display_driver_t display_x11 = {
	.name		     = "X11",
	.init		     = display_x11_init,
	.free		     = display_x11_free,
	.poll_event	     = display_x11_poll_event,
	.wait_event	     = display_x11_wait_event,
	.window_init	     = display_x11_window_init,
	.window_free	     = display_x11_window_free,
	.window_id	     = display_x11_window_id,
	.window_set_title    = display_x11_window_set_title,
	.window_set_position = display_x11_window_set_position,
	.window_set_size     = display_x11_window_set_size,
	.window_show	     = display_x11_window_show,
	.window_hide	     = display_x11_window_hide,
};

DISPLAY_DRIVER(display_x11, &display_x11);
