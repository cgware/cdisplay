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
} display_x11_t;

typedef struct window_x11_s {
	u32 id;
} window_x11_t;

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
	return (4 - (length & 3)) & 3;
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

	int best_score	  = -1;
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

		enum {
			XAUTH_FAMILY_LOCAL = 256,
			XAUTH_FAMILY_WILD  = 65535,
		};

		int score = -1;
		if (data_length > 0 &&
		    (number_length == 0 || bytes_eq(number, number_length, display_number_str.data, display_number_str.len)) &&
		    bytes_eq(name, name_length, mit_magic_cookie.data, mit_magic_cookie.len)) {
			switch (family) {
			case XAUTH_FAMILY_LOCAL: {
				score = bytes_eq(address, address_length, hostname.data, hostname.len) ? 100 : 50;
				break;
			}
			case XAUTH_FAMILY_WILD: {
				score = 10;
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
	buf_init(&b, 12, d->alloc);
	strv_t byte_order = STRV("l\0");
	buf_add_str(&b, byte_order, NULL);
	u8 padding[] = {0, 0, 0};

	strv_t auth_name = cookie ? mit_magic_cookie : STRV("");

	buf_write_u16le(&b, 11);
	buf_write_u16le(&b, 0);
	buf_write_u16le(&b, auth_name.len);
	buf_write_u16le(&b, cookie_length);
	buf_write_u16le(&b, 0);

	if (sock_write_all(d->ss, dx11->sock, b.data, b.used) || sock_write_all(d->ss, dx11->sock, auth_name.data, auth_name.len) ||
	    sock_write_all(d->ss, dx11->sock, padding, pad4(auth_name.len)) || sock_write_all(d->ss, dx11->sock, cookie, cookie_length) ||
	    sock_write_all(d->ss, dx11->sock, padding, pad4(cookie_length)) || sock_read_all(d->ss, dx11->sock, b.data, 8)) {
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
	if (success != 1) {
		log_error("cwindow", "awindow_x11", NULL, "connection setup was not successful");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	off = 2;
	u16 major_version;
	buf_read_u16le(&b, &off, &major_version);
	u16 minor_version;
	buf_read_u16le(&b, &off, &minor_version);
	log_info("cwindow", "awindow_x11", NULL, "X11 protocol version: %u.%u", major_version, minor_version);

	u16 extra_words;
	buf_read_u16le(&b, &off, &extra_words);
	size_t setup_length = (size_t)extra_words * 4;

	if (setup_length < 32) {
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

	off = 4;
	buf_read_u32le(&b, &off, &dx11->resource_id_base);
	buf_read_u32le(&b, &off, &dx11->resource_id_mask);

	off = 16;
	u16 vendor_length;
	buf_read_u16le(&b, &off, &vendor_length);
	off = 20;
	u8 screen_count;
	buf_read_u8le(&b, &off, &screen_count);
	u8 format_count;
	buf_read_u8le(&b, &off, &format_count);

	size_t screen_offset = 32 + vendor_length + pad4(vendor_length) + (size_t)format_count * 8;

	if (screen_count == 0) {
		log_error("cwindow", "awindow_x11", NULL, "no screens found");
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	if (screen_offset + 40 > setup_length) {
		log_error("cwindow", "awindow_x11", NULL, "invalid screen offset: %zu", screen_offset);
		buf_free(&b);
		sock_close(d->ss, dx11->sock);
		return 1;
	}

	off = screen_offset;
	buf_read_u32le(&b, &off, &dx11->root);
	off += 4;
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
	X_CREATE_WINDOW	 = 1,
	X_DESTROY_WINDOW = 4,
	X_MAP_WINDOW	 = 8,
};

static int create_window(window_t *wnd, u16 x, u16 y)
{
	enum {
		MAX_VALUES = 15,
	};

	u8 request[32 + MAX_VALUES * 4] = {0};
	u32 values[MAX_VALUES]		= {0};
	size_t value_count		= 0;

	display_x11_t *dx11 = wnd->display->data;

	u32 background_pixel = dx11->white_pixel;
	u32 border_pixel     = dx11->black_pixel;

	u32 event_mask = (1u << 15) | (1u << 17);

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

	enum {
		COPY_FROM_PARENT = 0,
	};

	u8 depth = COPY_FROM_PARENT;

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_CREATE_WINDOW);
	cbuf_write_u8le(request, &off, depth);

	u16 length = 8 + value_count;
	cbuf_write_u16le(request, &off, length);
	cbuf_write_u32le(request, &off, wx11->id);

	u32 parent = dx11->root;
	cbuf_write_u32le(request, &off, parent);

	cbuf_write_u16le(request, &off, x);
	cbuf_write_u16le(request, &off, y);

	u16 width  = 800;
	u16 height = 600;
	cbuf_write_u16le(request, &off, width);
	cbuf_write_u16le(request, &off, height);

	u16 border_width = 1;
	cbuf_write_u16le(request, &off, border_width);

	enum {
		INPUT_OUTPUT = 1,
	};

	u16 window_class = INPUT_OUTPUT;
	cbuf_write_u16le(request, &off, window_class);

	u32 visual = 0;
	cbuf_write_u32le(request, &off, visual);

	enum {
		CW_BACK_PIXEL	= 1u << 1,
		CW_BORDER_PIXEL = 1u << 3,
		CW_EVENT_MASK	= 1u << 11,
	};

	u32 value_mask = CW_BACK_PIXEL | CW_BORDER_PIXEL | CW_EVENT_MASK;
	cbuf_write_u32le(request, &off, value_mask);

	for (size_t i = 0; i < value_count; i++) {
		cbuf_write_u32le(request, &off, values[i]);
	}

	size_t request_size = 32 + value_count * 4;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, request_size)) {
		log_error("cwindow", "awindow_x11", NULL, "failed to create window");
		return 1;
	}

	return 0;
}

static int destroy_window(window_t *wnd)
{
	u8 request[8] = {0};

	window_x11_t *wx11 = wnd->data;

	size_t off = 0;
	cbuf_write_u8le(request, &off, X_DESTROY_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, 2);
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

	u8 request[8] = {0};
	size_t off    = 0;
	cbuf_write_u8le(request, &off, X_MAP_WINDOW);
	cbuf_write_u8le(request, &off, 0);
	cbuf_write_u16le(request, &off, 2);
	cbuf_write_u32le(request, &off, wx11->id);

	const display_x11_t *dx11 = wnd->display->data;

	if (sock_write_all(wnd->display->ss, dx11->sock, request, sizeof(request))) {
		log_error("cwindow", "awindow_x11", NULL, "failed to map window");
		return 1;
	}

	return 0;
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

static int display_x11_window_init(window_t *wnd, u16 x, u16 y)
{
	if (wnd == NULL) {
		return 1;
	}

	wnd->data = mem_alloc(sizeof(window_x11_t));
	if (wnd->data == NULL) {
		return 1;
	}

	if (create_window(wnd, x, y)) {
		mem_free(wnd->data, sizeof(window_x11_t));
		wnd->data = NULL;
		return 1;
	}

	if (map_window(wnd)) {
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

static display_driver_t display_x11 = {
	.name	     = "X11",
	.init	     = display_x11_init,
	.free	     = display_x11_free,
	.window_init = display_x11_window_init,
	.window_free = display_x11_window_free,
};

DISPLAY_DRIVER(display_x11, &display_x11);
// 577 536
