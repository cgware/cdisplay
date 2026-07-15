#include "test.h"

#include "cbuf.h"
#include "display.h"
#include "display_driver.h"
#include "log.h"
#include "mem.h"

#define T_XAUTHORITY "/xauthority"
#define T_X11_PATH   "/tmp/.X11-unix/X0"

static const u8 t_x11_cookie[] = {
	0xcf,
	0x22,
	0x17,
	0x5a,
	0xea,
	0x39,
	0x0a,
	0xd7,
	0xb4,
	0xb6,
	0x47,
	0xd3,
	0x65,
	0xac,
	0xa0,
	0x81,
};

typedef struct t_alloc_s {
	int alloc_calls;
	int fail_alloc_after;
	int fail_realloc;
} t_alloc_t;

static void *t_alloc_alloc(alloc_t *alloc, size_t size)
{
	t_alloc_t *state = alloc->priv;
	state->alloc_calls++;
	if (state->fail_alloc_after == state->alloc_calls) {
		return NULL;
	}

	return mem_alloc(size);
}

static int t_alloc_realloc(alloc_t *alloc, void **ptr, size_t *old_size, size_t new_size)
{
	t_alloc_t *state = alloc->priv;
	if (state->fail_realloc) {
		return 1;
	}

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

static display_driver_t *t_x11_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != DISPLAY_DRIVER_TYPE) {
			continue;
		}

		display_driver_t *drv = i->data;
		if (strv_eq(strv_cstr(drv->name), STRV("X11-direct"))) {
			return drv;
		}
	}

	return NULL;
}

static int t_x11_event_calls;
static display_event_t t_x11_event;

static void t_x11_event_reset(void)
{
	t_x11_event_calls = 0;
	t_x11_event	  = (display_event_t){0};
}

static void t_x11_event_cb(display_t *display, const display_event_t *event, void *user)
{
	(void)display;
	(void)user;
	t_x11_event_calls++;
	t_x11_event = *event;
}

static void t_x11_env_init(fs_t *fs, proc_t *proc, sock_t *ss)
{
	fs_init(fs, 8, 1, ALLOC_STD);
	proc_init(proc, 64, 1, ALLOC_STD);
	sock_init(ss, 8, 1, ALLOC_STD);
	proc->hostname = STR("host");
}

static void t_x11_env_free(fs_t *fs, proc_t *proc, sock_t *ss)
{
	fs_free(fs);
	proc_free(proc);
	sock_free(ss);
}

static void t_x11_set_display(proc_t *proc, strv_t display)
{
	proc_setenv(proc, STRV("DISPLAY"), display, 1);
}

static void t_x11_set_xauthority(proc_t *proc)
{
	proc_setenv(proc, STRV("XAUTHORITY"), STRV(T_XAUTHORITY), 1);
}

static void t_x11_listen(sock_t *ss, void **server)
{
	sock_open(ss, SOCK_FAMILY_UNIX, SOCK_TYPE_STREAM, 0, server);
	sock_bind(ss, *server, SOCK_FAMILY_UNIX, T_X11_PATH, sizeof(T_X11_PATH));
	sock_listen(ss, *server, 1);
}

static void t_x11_write_file(fs_t *fs, strv_t path, buf_t data)
{
	void *file;
	fs_open(fs, path, "wb", &file);
	fs_writeb(fs, file, data);
	fs_close(fs, file);
}

static void t_x11_write_u16be(buf_t *buf, u16 val)
{
	buf_write_u8be(buf, (val >> 8) & 0xFF);
	buf_write_u8be(buf, val & 0xFF);
}

static void t_x11_write_blob(buf_t *buf, const void *data, u16 size)
{
	t_x11_write_u16be(buf, size);
	buf_add(buf, size, data, NULL);
}

static void t_x11_add_authority(buf_t *auth, u16 family, strv_t address, strv_t number, strv_t name, strv_t data)
{
	t_x11_write_u16be(auth, family);
	t_x11_write_blob(auth, address.data, address.len);
	t_x11_write_blob(auth, number.data, number.len);
	t_x11_write_blob(auth, name.data, name.len);
	t_x11_write_blob(auth, data.data, data.len);
}

static void t_x11_write_authority_data(fs_t *fs, const void *data, size_t size)
{
	buf_t auth = {0};
	buf_init(&auth, 128, ALLOC_STD);

	t_x11_add_authority(&auth, 256, STRV("host"), STRV(""), STRV("MIT-MAGIC-COOKIE-1"), STRVN(data, size));

	t_x11_write_file(fs, STRV(T_XAUTHORITY), auth);
	buf_free(&auth);
}

static void t_x11_write_authority(fs_t *fs)
{
	t_x11_write_authority_data(fs, t_x11_cookie, sizeof(t_x11_cookie));
}

static void t_x11_write_authority_family(fs_t *fs, u16 family)
{
	buf_t auth = {0};
	buf_init(&auth, 128, ALLOC_STD);

	t_x11_add_authority(
		&auth, family, STRV("host"), STRV(""), STRV("MIT-MAGIC-COOKIE-1"), STRVN((const char *)t_x11_cookie, sizeof(t_x11_cookie)));

	t_x11_write_file(fs, STRV(T_XAUTHORITY), auth);
	buf_free(&auth);
}

static void t_x11_setup_header(u8 header[8], u8 success, u16 extra_words)
{
	size_t off = 0;

	cbuf_write_u8le(header, &off, success);
	cbuf_write_u8le(header, &off, 0);
	cbuf_write_u16le(header, &off, 11);
	cbuf_write_u16le(header, &off, 0);
	cbuf_write_u16le(header, &off, extra_words);
}

static void t_x11_setup_data(u8 *setup, size_t size, u8 screen_count, u16 vendor_length)
{
	mem_set(setup, 0, size);

	cbuf_set_u32le(setup, 4, 0x00100000);
	cbuf_set_u32le(setup, 8, 0x001ffffe);
	cbuf_set_u16le(setup, 16, vendor_length);
	cbuf_set_u8le(setup, 20, screen_count);
	cbuf_set_u8le(setup, 21, 0);
	if (size >= 72) {
		cbuf_set_u32le(setup, 32, 0x00000040);
		cbuf_set_u32le(setup, 40, 0x00ffffff);
		cbuf_set_u32le(setup, 44, 0x00000000);
		cbuf_set_u8le(setup, 24, 32);
		cbuf_set_u8le(setup, 25, 32);
		cbuf_set_u8le(setup, 26, 8);
		cbuf_set_u8le(setup, 27, 40);
	}
}

static void t_x11_atom_reply(u8 reply[32], u8 success, u32 atom)
{
	mem_set(reply, 0, 32);
	cbuf_set_u8le(reply, 0, success);
	cbuf_set_u32le(reply, 8, atom);
}

static void t_x11_default_atom_replies(u8 replies[256])
{
	t_x11_atom_reply(replies, 1, 0x000000f0);
	t_x11_atom_reply(&replies[32], 1, 0x000000f1);
	t_x11_atom_reply(&replies[64], 1, 0x000000f2);
	t_x11_atom_reply(&replies[96], 1, 0x000000f3);
	t_x11_atom_reply(&replies[128], 1, 0x000000f4);
	t_x11_atom_reply(&replies[160], 1, 0x000000f5);
	t_x11_atom_reply(&replies[192], 1, 0x000000f6);
	t_x11_atom_reply(&replies[224], 1, 0x000000f7);
}

static void t_x11_keyboard_mapping(buf_t *buf)
{
	u8 reply[32]	   = {0};
	u8 keysyms[33 * 4] = {0};

	cbuf_set_u8le(reply, 0, 1);
	cbuf_set_u8le(reply, 1, 1);
	cbuf_set_u32le(reply, 4, 33);

	cbuf_set_u32le(keysyms, 0 * 4, 'A');
	cbuf_set_u32le(keysyms, 1 * 4, 0xff1b);
	cbuf_set_u32le(keysyms, 2 * 4, 0xffbe);
	cbuf_set_u32le(keysyms, 3 * 4, '1');
	cbuf_set_u32le(keysyms, 4 * 4, 0xff0d);
	cbuf_set_u32le(keysyms, 5 * 4, 0xff09);
	cbuf_set_u32le(keysyms, 6 * 4, 0xff08);
	cbuf_set_u32le(keysyms, 7 * 4, ' ');
	cbuf_set_u32le(keysyms, 8 * 4, 0xff51);
	cbuf_set_u32le(keysyms, 9 * 4, 0xff53);
	cbuf_set_u32le(keysyms, 10 * 4, 0xff52);
	cbuf_set_u32le(keysyms, 11 * 4, 0xff54);
	cbuf_set_u32le(keysyms, 12 * 4, 0xffe1);
	cbuf_set_u32le(keysyms, 13 * 4, 0xffe2);
	cbuf_set_u32le(keysyms, 14 * 4, 0xffe3);
	cbuf_set_u32le(keysyms, 15 * 4, 0xffe4);
	cbuf_set_u32le(keysyms, 16 * 4, 0xffe9);
	cbuf_set_u32le(keysyms, 17 * 4, 0xffea);
	cbuf_set_u32le(keysyms, 18 * 4, 0xffeb);
	cbuf_set_u32le(keysyms, 19 * 4, 0xffec);
	cbuf_set_u32le(keysyms, 20 * 4, 0xffe5);
	cbuf_set_u32le(keysyms, 21 * 4, 0xff7f);
	cbuf_set_u32le(keysyms, 22 * 4, 0xff14);
	cbuf_set_u32le(keysyms, 23 * 4, 0xff13);
	cbuf_set_u32le(keysyms, 24 * 4, 0xff63);
	cbuf_set_u32le(keysyms, 25 * 4, 0xffff);
	cbuf_set_u32le(keysyms, 26 * 4, 0xff50);
	cbuf_set_u32le(keysyms, 27 * 4, 0xff57);
	cbuf_set_u32le(keysyms, 28 * 4, 0xff55);
	cbuf_set_u32le(keysyms, 29 * 4, 0xff56);
	cbuf_set_u32le(keysyms, 30 * 4, 'a');
	cbuf_set_u32le(keysyms, 31 * 4, 0xff61);

	buf_add(buf, sizeof(reply), reply, NULL);
	buf_add(buf, sizeof(keysyms), keysyms, NULL);
}

static void t_x11_extended_keyboard_mapping(buf_t *buf)
{
	u8 reply[32]	   = {0};
	u8 keysyms[63 * 4] = {0};

	cbuf_set_u8le(reply, 0, 1);
	cbuf_set_u8le(reply, 1, 1);
	cbuf_set_u32le(reply, 4, 63);

	cbuf_set_u32le(keysyms, 33 * 4, 0x0060);
	cbuf_set_u32le(keysyms, 34 * 4, 0x003d);
	cbuf_set_u32le(keysyms, 35 * 4, 0x002d);
	cbuf_set_u32le(keysyms, 36 * 4, 0x005b);
	cbuf_set_u32le(keysyms, 37 * 4, 0x005d);
	cbuf_set_u32le(keysyms, 38 * 4, 0x005c);
	cbuf_set_u32le(keysyms, 39 * 4, 0x003b);
	cbuf_set_u32le(keysyms, 40 * 4, 0x0027);
	cbuf_set_u32le(keysyms, 41 * 4, 0x002c);
	cbuf_set_u32le(keysyms, 42 * 4, 0x002e);
	cbuf_set_u32le(keysyms, 43 * 4, 0x002f);
	cbuf_set_u32le(keysyms, 44 * 4, 0xff67);
	cbuf_set_u32le(keysyms, 45 * 4, 0xffaf);
	cbuf_set_u32le(keysyms, 46 * 4, 0xffaa);
	cbuf_set_u32le(keysyms, 47 * 4, 0xffad);
	cbuf_set_u32le(keysyms, 48 * 4, 0xffab);
	cbuf_set_u32le(keysyms, 49 * 4, 0xff8d);
	cbuf_set_u32le(keysyms, 50 * 4, 0xff9e);
	cbuf_set_u32le(keysyms, 51 * 4, 0xff9c);
	cbuf_set_u32le(keysyms, 52 * 4, 0xff99);
	cbuf_set_u32le(keysyms, 53 * 4, 0xff9b);
	cbuf_set_u32le(keysyms, 54 * 4, 0xff96);
	cbuf_set_u32le(keysyms, 55 * 4, 0xff9d);
	cbuf_set_u32le(keysyms, 56 * 4, 0xff98);
	cbuf_set_u32le(keysyms, 57 * 4, 0xff95);
	cbuf_set_u32le(keysyms, 58 * 4, 0xff97);
	cbuf_set_u32le(keysyms, 59 * 4, 0xff9a);
	cbuf_set_u32le(keysyms, 60 * 4, 0xffae);
	cbuf_set_u32le(keysyms, 61 * 4, 0xffb0);
	cbuf_set_u32le(keysyms, 62 * 4, 0xff9f);

	buf_add(buf, sizeof(reply), reply, NULL);
	buf_add(buf, sizeof(keysyms), keysyms, NULL);
}

static void t_x11_modifier_mapping(buf_t *buf)
{
	u8 reply[32]	= {0};
	u8 keycodes[16] = {0};

	cbuf_set_u8le(reply, 0, 1);
	cbuf_set_u8le(reply, 1, 2);
	cbuf_set_u32le(reply, 4, 4);

	keycodes[0]  = 20;
	keycodes[1]  = 21;
	keycodes[2]  = 28;
	keycodes[4]  = 22;
	keycodes[5]  = 23;
	keycodes[6]  = 24;
	keycodes[7]  = 25;
	keycodes[8]  = 29;
	keycodes[12] = 26;
	keycodes[13] = 27;
	keycodes[14] = 8;

	buf_add(buf, sizeof(reply), reply, NULL);
	buf_add(buf, sizeof(keycodes), keycodes, NULL);
}

static void t_x11_modifier_reply(u8 reply[32], u8 success, u8 keycodes_per_modifier, u32 words)
{
	mem_set(reply, 0, 32);
	cbuf_set_u8le(reply, 0, success);
	cbuf_set_u8le(reply, 1, keycodes_per_modifier);
	cbuf_set_u32le(reply, 4, words);
}

static void t_x11_keyboard_reply(u8 reply[32], u8 success, u8 keysyms_per_keycode, u32 words)
{
	mem_set(reply, 0, 32);
	cbuf_set_u8le(reply, 0, success);
	cbuf_set_u8le(reply, 1, keysyms_per_keycode);
	cbuf_set_u32le(reply, 4, words);
}

static void t_x11_script_setup_data_keyboard_atoms(sock_t *ss, void *server, u8 success, const void *setup_data, size_t setup_size,
						   const void *keyboard_data, size_t keyboard_size, const void *atom_data, size_t atom_size)
{
	buf_t script = {0};
	u8 header[8] = {0};

	t_x11_setup_header(header, success, setup_size / 4);

	buf_init(&script, sizeof(header) + setup_size + keyboard_size + atom_size, ALLOC_STD);
	buf_add(&script, sizeof(header), header, NULL);
	buf_add(&script, setup_size, setup_data, NULL);
	if (keyboard_data != NULL) {
		buf_add(&script, keyboard_size, keyboard_data, NULL);
	}
	if (atom_data != NULL) {
		buf_add(&script, atom_size, atom_data, NULL);
	}
	sock_script(ss, server, script.data, script.used);
	buf_free(&script);
}

static void t_x11_script_setup_data_atoms(sock_t *ss, void *server, u8 success, const void *setup_data, size_t setup_size,
					  const void *atom_data, size_t atom_size)
{
	buf_t keyboard = {0};
	buf_init(&keyboard, 32 + 33 * 4 + 32 + 16, ALLOC_STD);
	if (success == 1) {
		t_x11_keyboard_mapping(&keyboard);
		t_x11_modifier_mapping(&keyboard);
	}
	t_x11_script_setup_data_keyboard_atoms(ss,
					       server,
					       success,
					       setup_data,
					       setup_size,
					       success == 1 ? keyboard.data : NULL,
					       success == 1 ? keyboard.used : 0,
					       atom_data,
					       atom_size);
	buf_free(&keyboard);
}

static void t_x11_script_setup_data(sock_t *ss, void *server, u8 success, const void *setup_data, size_t setup_size)
{
	u8 atom_reply[256] = {0};

	if (success == 1) {
		t_x11_default_atom_replies(atom_reply);
	}

	t_x11_script_setup_data_atoms(
		ss, server, success, setup_data, setup_size, success == 1 ? atom_reply : NULL, success == 1 ? sizeof(atom_reply) : 0);
}

static void t_x11_script_setup_header(sock_t *ss, void *server, u8 success, u16 extra_words)
{
	u8 header[8] = {0};

	t_x11_setup_header(header, success, extra_words);
	sock_script(ss, server, header, sizeof(header));
}

static void t_x11_script_setup(sock_t *ss, void *server)
{
	u8 setup[72] = {0};

	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_script_setup_data(ss, server, 1, setup, sizeof(setup));
}

static void t_x11_script_setup_visual(sock_t *ss, void *server, u32 visual, u8 depth)
{
	u8 setup[104] = {0};

	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	cbuf_set_u8le(setup, 71, 1);
	cbuf_set_u8le(setup, 72, depth);
	cbuf_set_u16le(setup, 74, 1);
	cbuf_set_u32le(setup, 80, visual);
	t_x11_script_setup_data(ss, server, 1, setup, sizeof(setup));
}

static void t_x11_drain_display_init_requests(sock_t *ss, void *peer);

static void t_x11_open_display(display_driver_t *drv, fs_t *fs, proc_t *proc, sock_t *ss, display_t *display, void **server, void **peer)
{
	t_x11_env_init(fs, proc, ss);
	t_x11_set_display(proc, STRV(":0"));
	t_x11_set_xauthority(proc);
	t_x11_write_authority(fs);
	t_x11_listen(ss, server);
	t_x11_script_setup(ss, *server);
	log_set_quiet(0, 1);
	display_init(display, drv, fs, proc, ss, ALLOC_STD);
	log_set_quiet(0, 0);
	sock_accept(ss, *server, peer);
	t_x11_drain_display_init_requests(ss, *peer);
}

static void t_x11_close_display(fs_t *fs, proc_t *proc, sock_t *ss, display_t *display, void *server, void *peer)
{
	display_free(display);
	sock_close(ss, peer);
	sock_close(ss, server);
	t_x11_env_free(fs, proc, ss);
}

static void t_x11_open_window_config(display_driver_t *drv, fs_t *fs, proc_t *proc, sock_t *ss, display_t *display, window_t *window,
				     const window_config_t *config, void **server, void **peer)
{
	t_x11_env_init(fs, proc, ss);
	t_x11_set_display(proc, STRV(":0"));
	t_x11_set_xauthority(proc);
	t_x11_write_authority(fs);
	t_x11_listen(ss, server);
	t_x11_script_setup(ss, *server);
	log_set_quiet(0, 1);
	display_init(display, drv, fs, proc, ss, ALLOC_STD);
	t_x11_event_reset();
	display_set_event_callback(display, t_x11_event_cb, NULL);
	window_init(window, display, config);
	log_set_quiet(0, 0);
	sock_accept(ss, *server, peer);
}

static void t_x11_open_window(display_driver_t *drv, fs_t *fs, proc_t *proc, sock_t *ss, display_t *display, window_t *window,
			      void **server, void **peer)
{
	t_x11_open_window_config(drv, fs, proc, ss, display, window, &(window_config_t){.width = 640, .height = 480}, server, peer);
}

static void t_x11_drain_display_init_requests(sock_t *ss, void *peer)
{
	u8 setup_request[48] = {0};
	u8 keymap_request[8] = {0};
	u8 modmap_request[4] = {0};
	u8 atom_request[20]  = {0};
	u8 atom_request2[24] = {0};
	u8 atom_request3[16] = {0};
	u8 atom_request4[20] = {0};
	u8 atom_request5[20] = {0};
	u8 atom_request6[24] = {0};
	u8 atom_request7[24] = {0};
	u8 atom_request8[32] = {0};

	sock_read_all(ss, peer, setup_request, sizeof(setup_request));
	sock_read_all(ss, peer, keymap_request, sizeof(keymap_request));
	sock_read_all(ss, peer, modmap_request, sizeof(modmap_request));
	sock_read_all(ss, peer, atom_request, sizeof(atom_request));
	sock_read_all(ss, peer, atom_request2, sizeof(atom_request2));
	sock_read_all(ss, peer, atom_request3, sizeof(atom_request3));
	sock_read_all(ss, peer, atom_request4, sizeof(atom_request4));
	sock_read_all(ss, peer, atom_request5, sizeof(atom_request5));
	sock_read_all(ss, peer, atom_request6, sizeof(atom_request6));
	sock_read_all(ss, peer, atom_request7, sizeof(atom_request7));
	sock_read_all(ss, peer, atom_request8, sizeof(atom_request8));
}

static void t_x11_drain_open_window_requests(sock_t *ss, void *peer)
{
	u8 create_request[44]	= {0};
	u8 property_request[28] = {0};

	t_x11_drain_display_init_requests(ss, peer);
	sock_read_all(ss, peer, create_request, sizeof(create_request));
	sock_read_all(ss, peer, property_request, sizeof(property_request));
}

static void t_x11_write_key_event(sock_t *ss, void *peer, u8 type, u8 detail, u32 window, u16 x, u16 y, u16 state)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, type);
	cbuf_set_u8le(event, 1, detail);
	cbuf_set_u32le(event, 12, window);
	cbuf_set_u16le(event, 24, x);
	cbuf_set_u16le(event, 26, y);
	cbuf_set_u16le(event, 28, state);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_motion_event(sock_t *ss, void *peer, u32 window, u16 x, u16 y, u16 state)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 6);
	cbuf_set_u32le(event, 12, window);
	cbuf_set_u16le(event, 24, x);
	cbuf_set_u16le(event, 26, y);
	cbuf_set_u16le(event, 28, state);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_focus_event(sock_t *ss, void *peer, u8 type, u32 window)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, type);
	cbuf_set_u32le(event, 4, window);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_expose_event(sock_t *ss, void *peer, u32 window)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 12);
	cbuf_set_u32le(event, 4, window);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_destroy_event(sock_t *ss, void *peer, u32 window)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 17);
	cbuf_set_u32le(event, 8, window);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_unmap_event(sock_t *ss, void *peer, u32 window)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 18);
	cbuf_set_u32le(event, 8, window);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_reparent_event(sock_t *ss, void *peer, u32 window)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 21);
	cbuf_set_u32le(event, 8, window);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_map_event(sock_t *ss, void *peer, u32 window)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 19);
	cbuf_set_u32le(event, 8, window);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_configure_event(sock_t *ss, void *peer, u32 window, u16 x, u16 y, u16 width, u16 height)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 22);
	cbuf_set_u32le(event, 8, window);
	cbuf_set_u16le(event, 16, x);
	cbuf_set_u16le(event, 18, y);
	cbuf_set_u16le(event, 20, width);
	cbuf_set_u16le(event, 22, height);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_client_event(sock_t *ss, void *peer, u32 window)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 33);
	cbuf_set_u8le(event, 1, 32);
	cbuf_set_u32le(event, 4, window);
	cbuf_set_u32le(event, 8, 0x000000f0);
	cbuf_set_u32le(event, 12, 0x000000f1);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_unknown_client_event(sock_t *ss, void *peer, u32 window)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 33);
	cbuf_set_u8le(event, 1, 32);
	cbuf_set_u32le(event, 4, window);
	cbuf_set_u32le(event, 8, 0x000000f0);
	cbuf_set_u32le(event, 12, 0x000000ff);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_mapping_event(sock_t *ss, void *peer)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 34);
	sock_write_all(ss, peer, event, sizeof(event));
}

static void t_x11_write_unknown_event(sock_t *ss, void *peer)
{
	u8 event[32] = {0};

	cbuf_set_u8le(event, 0, 99);
	sock_write_all(ss, peer, event, sizeof(event));
}

TEST(display_x11_direct_driver_is_registered)
{
	START;

	EXPECT_NE(t_x11_driver(), NULL);

	END;
}

TEST(display_x11_direct_init_null_display)
{
	START;

	display_driver_t *drv = t_x11_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->init(NULL), 1);

	END;
}

TEST(display_x11_direct_init_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	t_alloc_t state	      = {.fail_alloc_after = 1};
	display_t display     = {.alloc = t_alloc(&state)};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->init(&display), 1);

	END;
}

TEST(display_x11_direct_free_null_display)
{
	START;

	display_driver_t *drv = t_x11_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->free(NULL), 1);

	END;
}

TEST(display_x11_direct_window_init_null_window)
{
	START;

	display_driver_t *drv = t_x11_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_init(NULL, &(window_config_t){.width = 640, .height = 480}), 1);

	END;
}

TEST(display_x11_direct_window_init_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	t_alloc_t state	      = {.fail_alloc_after = 1};
	display_t display     = {.alloc = t_alloc(&state)};
	window_t wnd	      = {.display = &display};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_init(&wnd, &(window_config_t){.width = 640, .height = 480}), 1);

	END;
}

TEST(display_x11_direct_window_free_null_window)
{
	START;

	display_driver_t *drv = t_x11_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_free(NULL), 1);

	END;
}

TEST(display_x11_direct_window_id_null_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t window	      = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_id(&window), 0);

	END;
}

TEST(display_x11_direct_window_native_null_window)
{
	START;

	display_driver_t *drv	  = t_x11_driver();
	window_native_t native = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_native(NULL, &native), 1);

	END;
}

TEST(display_x11_direct_window_native_null_native)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t window	     = {.data = (void *)0x1234};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_native(&window, NULL), 1);

	END;
}

TEST(display_x11_direct_window_native_returns_window)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	window_native_t native = {0};

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);

	EXPECT_EQ(drv->window_native(&window, &native), 0);
	EXPECT_EQ(native.type, DISPLAY_NATIVE_X11);
	EXPECT_EQ(native.window, (void *)(uintptr_t)0x00100000);

	window_free(&window);
	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_init_success)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup(&ss, server);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), &display);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_init_writes_requests)
{
	START;

	display_driver_t *drv	= t_x11_driver();
	fs_t fs			= {0};
	proc_t proc		= {0};
	sock_t ss		= {0};
	display_t display	= {0};
	window_t window		= {0};
	void *server		= NULL;
	void *peer		= NULL;
	u8 setup_request[48]	= {0};
	u8 keymap_request[8]	= {0};
	u8 modmap_request[4]	= {0};
	u8 atom_request[20]	= {0};
	u8 atom_request2[24]	= {0};
	u8 atom_request3[16]	= {0};
	u8 atom_request4[20]	= {0};
	u8 atom_request5[20]	= {0};
	u8 atom_request6[24]	= {0};
	u8 atom_request7[24]	= {0};
	u8 atom_request8[32]	= {0};
	u8 create_request[44]	= {0};
	u8 property_request[28] = {0};
	u32 x11_window_id	= 0;
	u32 parent		= 0;
	u32 event_mask		= 0;
	u32 property		= 0;
	u32 property_type	= 0;
	u32 property_data	= 0;
	u16 x			= 0;
	u16 y			= 0;
	u16 width		= 0;
	u16 height		= 0;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup(&ss, server);
	log_set_quiet(0, 1);
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.x = 11, .y = 22, .width = 333, .height = 444}), &window);
	log_set_quiet(0, 0);

	sock_accept(&ss, server, &peer);
	sock_read_all(&ss, peer, setup_request, sizeof(setup_request));
	sock_read_all(&ss, peer, keymap_request, sizeof(keymap_request));
	sock_read_all(&ss, peer, modmap_request, sizeof(modmap_request));
	sock_read_all(&ss, peer, atom_request, sizeof(atom_request));
	sock_read_all(&ss, peer, atom_request2, sizeof(atom_request2));
	sock_read_all(&ss, peer, atom_request3, sizeof(atom_request3));
	sock_read_all(&ss, peer, atom_request4, sizeof(atom_request4));
	sock_read_all(&ss, peer, atom_request5, sizeof(atom_request5));
	sock_read_all(&ss, peer, atom_request6, sizeof(atom_request6));
	sock_read_all(&ss, peer, atom_request7, sizeof(atom_request7));
	sock_read_all(&ss, peer, atom_request8, sizeof(atom_request8));
	sock_read_all(&ss, peer, create_request, sizeof(create_request));
	sock_read_all(&ss, peer, property_request, sizeof(property_request));

	EXPECT_EQ(keymap_request[0], 101);
	EXPECT_EQ(keymap_request[4], 8);
	EXPECT_EQ(keymap_request[5], 33);
	EXPECT_EQ(modmap_request[0], 119);
	EXPECT_EQ(atom_request[0], 16);
	EXPECT_EQ(atom_request2[0], 16);
	EXPECT_EQ(atom_request3[0], 16);
	EXPECT_EQ(atom_request4[0], 16);
	EXPECT_EQ(atom_request5[0], 16);
	EXPECT_EQ(atom_request6[0], 16);
	EXPECT_EQ(atom_request7[0], 16);
	EXPECT_EQ(atom_request8[0], 16);

	cbuf_get_u32le(create_request, 4, &x11_window_id);
	cbuf_get_u32le(create_request, 8, &parent);
	cbuf_get_u16le(create_request, 12, &x);
	cbuf_get_u16le(create_request, 14, &y);
	cbuf_get_u16le(create_request, 16, &width);
	cbuf_get_u16le(create_request, 18, &height);
	cbuf_get_u32le(create_request, 40, &event_mask);
	EXPECT_EQ(create_request[0], 1);
	EXPECT_EQ(x11_window_id, 0x00100000);
	EXPECT_EQ(window_id(&window), 0x00100000);
	EXPECT_EQ(parent, 0x00000040);
	EXPECT_EQ(x, 11);
	EXPECT_EQ(y, 22);
	EXPECT_EQ(width, 333);
	EXPECT_EQ(height, 444);
	EXPECT_EQ(event_mask, (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 6) | (1u << 15) | (1u << 17) | (1u << 21));

	cbuf_get_u32le(property_request, 8, &property);
	cbuf_get_u32le(property_request, 12, &property_type);
	cbuf_get_u32le(property_request, 24, &property_data);
	EXPECT_EQ(property_request[0], 18);
	EXPECT_EQ(property, 0x000000f0);
	EXPECT_EQ(property_type, 4);
	EXPECT_EQ(property_request[16], 32);
	EXPECT_EQ(property_data, 0x000000f1);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_init_custom_visual_writes_requests)
{
	START;

	display_driver_t *drv	= t_x11_driver();
	fs_t fs			= {0};
	proc_t proc		= {0};
	sock_t ss		= {0};
	display_t display	= {0};
	window_t window		= {0};
	void *server		= NULL;
	void *peer		= NULL;
	u8 colormap_request[16] = {0};
	u8 create_request[48]	= {0};
	u8 property_request[28] = {0};
	u32 colormap		= 0;
	u32 root		= 0;
	u32 visual		= 0;
	u32 create_visual	= 0;
	u32 value_mask		= 0;
	u32 create_colormap	= 0;

	t_x11_open_window_config(drv,
				 &fs,
				 &proc,
				 &ss,
				 &display,
				 &window,
				 &(window_config_t){.width = 640, .height = 480, .depth = 24, .visual = 0x21},
				 &server,
				 &peer);
	t_x11_drain_display_init_requests(&ss, peer);
	sock_read_all(&ss, peer, colormap_request, sizeof(colormap_request));
	sock_read_all(&ss, peer, create_request, sizeof(create_request));
	sock_read_all(&ss, peer, property_request, sizeof(property_request));

	cbuf_get_u32le(colormap_request, 4, &colormap);
	cbuf_get_u32le(colormap_request, 8, &root);
	cbuf_get_u32le(colormap_request, 12, &visual);
	EXPECT_EQ(colormap_request[0], 78);
	EXPECT_EQ(colormap_request[1], 0);
	EXPECT_EQ(colormap, 0x00100002);
	EXPECT_EQ(root, 0x00000040);
	EXPECT_EQ(visual, 0x21);

	cbuf_get_u32le(create_request, 24, &create_visual);
	cbuf_get_u32le(create_request, 28, &value_mask);
	cbuf_get_u32le(create_request, 44, &create_colormap);
	EXPECT_EQ(create_request[0], 1);
	EXPECT_EQ(create_request[1], 24);
	EXPECT_EQ(create_visual, 0x21);
	EXPECT_EQ(value_mask, (1u << 1) | (1u << 3) | (1u << 11) | (1u << 13));
	EXPECT_EQ(create_colormap, colormap);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_free_custom_visual_writes_request)
{
	START;

	display_driver_t *drv  = t_x11_driver();
	fs_t fs		       = {0};
	proc_t proc	       = {0};
	sock_t ss	       = {0};
	display_t display      = {0};
	window_t window	       = {0};
	void *server	       = NULL;
	void *peer	       = NULL;
	u8 destroy_request[8]  = {0};
	u8 colormap_request[8] = {0};
	u32 colormap	       = 0;

	t_x11_open_window_config(drv,
				 &fs,
				 &proc,
				 &ss,
				 &display,
				 &window,
				 &(window_config_t){.width = 640, .height = 480, .depth = 24, .visual = 0x21},
				 &server,
				 &peer);
	t_x11_drain_display_init_requests(&ss, peer);
	sock_read_all(&ss, peer, (u8[16]){0}, 16);
	sock_read_all(&ss, peer, (u8[48]){0}, 48);
	sock_read_all(&ss, peer, (u8[28]){0}, 28);

	window_free(&window);
	sock_read_all(&ss, peer, destroy_request, sizeof(destroy_request));
	sock_read_all(&ss, peer, colormap_request, sizeof(colormap_request));

	cbuf_get_u32le(colormap_request, 4, &colormap);
	EXPECT_EQ(colormap_request[0], 79);
	EXPECT_EQ(colormap, 0x00100002);

	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_title_writes_requests)
{
	START;

	display_driver_t *drv	   = t_x11_driver();
	fs_t fs			   = {0};
	proc_t proc		   = {0};
	sock_t ss		   = {0};
	display_t display	   = {0};
	window_t window		   = {0};
	void *server		   = NULL;
	void *peer		   = NULL;
	u8 wm_name_request[28]	   = {0};
	u8 net_wm_name_request[28] = {0};
	u32 property		   = 0;
	u32 property_type	   = 0;
	u32 length		   = 0;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_set_title(&window, STRV("Test")), 0);
	sock_read_all(&ss, peer, wm_name_request, sizeof(wm_name_request));
	sock_read_all(&ss, peer, net_wm_name_request, sizeof(net_wm_name_request));

	cbuf_get_u32le(wm_name_request, 8, &property);
	cbuf_get_u32le(wm_name_request, 12, &property_type);
	cbuf_get_u32le(wm_name_request, 20, &length);
	EXPECT_EQ(wm_name_request[0], 18);
	EXPECT_EQ(property, 0x000000f2);
	EXPECT_EQ(property_type, 31);
	EXPECT_EQ(wm_name_request[16], 8);
	EXPECT_EQ(length, 4);
	EXPECT_EQ(wm_name_request[24], 'T');
	EXPECT_EQ(wm_name_request[25], 'e');
	EXPECT_EQ(wm_name_request[26], 's');
	EXPECT_EQ(wm_name_request[27], 't');

	cbuf_get_u32le(net_wm_name_request, 8, &property);
	cbuf_get_u32le(net_wm_name_request, 12, &property_type);
	cbuf_get_u32le(net_wm_name_request, 20, &length);
	EXPECT_EQ(net_wm_name_request[0], 18);
	EXPECT_EQ(property, 0x000000f3);
	EXPECT_EQ(property_type, 0x000000f4);
	EXPECT_EQ(net_wm_name_request[16], 8);
	EXPECT_EQ(length, 4);
	EXPECT_EQ(net_wm_name_request[24], 'T');
	EXPECT_EQ(net_wm_name_request[25], 'e');
	EXPECT_EQ(net_wm_name_request[26], 's');
	EXPECT_EQ(net_wm_name_request[27], 't');

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_title_null_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t window	      = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_set_title(&window, STRV("title")), 1);

	END;
}

TEST(display_x11_direct_window_set_title_invalid_text)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_set_title(&window, STRVN(NULL, 1)), 1);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_title_too_long)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	log_set_quiet(0, 1);
	EXPECT_EQ(window_set_title(&window, STRVN("", 262117)), 1);
	log_set_quiet(0, 0);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_title_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	mem_oom(1);
	EXPECT_EQ(window_set_title(&window, STRV("title")), 1);
	mem_oom(0);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_title_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);
	sock_close(&ss, peer);

	log_set_quiet(0, 1);
	EXPECT_EQ(window_set_title(&window, STRV("title")), 1);
	window_free(&window);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_geometry_writes_requests)
{
	START;

	display_driver_t *drv	= t_x11_driver();
	fs_t fs			= {0};
	proc_t proc		= {0};
	sock_t ss		= {0};
	display_t display	= {0};
	window_t window		= {0};
	void *server		= NULL;
	void *peer		= NULL;
	u8 position_request[20] = {0};
	u8 size_request[20]	= {0};
	u32 mask		= 0;
	u32 first		= 0;
	u32 second		= 0;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_set_position(&window, 11, 22), 0);
	EXPECT_EQ(window_set_size(&window, 333, 444), 0);
	sock_read_all(&ss, peer, position_request, sizeof(position_request));
	sock_read_all(&ss, peer, size_request, sizeof(size_request));

	cbuf_get_u32le(position_request, 8, &mask);
	cbuf_get_u32le(position_request, 12, &first);
	cbuf_get_u32le(position_request, 16, &second);
	EXPECT_EQ(position_request[0], 12);
	EXPECT_EQ(mask, (1u << 0) | (1u << 1));
	EXPECT_EQ(first, 11);
	EXPECT_EQ(second, 22);

	cbuf_get_u32le(size_request, 8, &mask);
	cbuf_get_u32le(size_request, 12, &first);
	cbuf_get_u32le(size_request, 16, &second);
	EXPECT_EQ(size_request[0], 12);
	EXPECT_EQ(mask, (1u << 2) | (1u << 3));
	EXPECT_EQ(first, 333);
	EXPECT_EQ(second, 444);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_position_null_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t window	      = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_set_position(&window, 11, 22), 1);

	END;
}

TEST(display_x11_direct_window_set_size_null_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t window	      = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_set_size(&window, 333, 444), 1);

	END;
}

TEST(display_x11_direct_window_set_borderless_null_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t window	      = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_set_borderless(&window, 1), 1);

	END;
}

TEST(display_x11_direct_window_set_borderless_writes_request)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 request[44]	      = {0};
	u32 property	      = 0;
	u32 property_type     = 0;
	u32 length	      = 0;
	u32 flags	      = 0;
	u32 functions	      = 0;
	u32 decorations	      = 0;
	u32 input_mode	      = 0;
	u32 status	      = 0;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_set_borderless(&window, 1), 0);
	sock_read_all(&ss, peer, request, sizeof(request));

	cbuf_get_u32le(request, 8, &property);
	cbuf_get_u32le(request, 12, &property_type);
	cbuf_get_u32le(request, 20, &length);
	cbuf_get_u32le(request, 24, &flags);
	cbuf_get_u32le(request, 28, &functions);
	cbuf_get_u32le(request, 32, &decorations);
	cbuf_get_u32le(request, 36, &input_mode);
	cbuf_get_u32le(request, 40, &status);
	EXPECT_EQ(request[0], 18);
	EXPECT_EQ(property, 0x000000f5);
	EXPECT_EQ(property_type, 0x000000f5);
	EXPECT_EQ(request[16], 32);
	EXPECT_EQ(length, 5);
	EXPECT_EQ(flags, 2);
	EXPECT_EQ(functions, 0);
	EXPECT_EQ(decorations, 0);
	EXPECT_EQ(input_mode, 0);
	EXPECT_EQ(status, 0);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_bordered_writes_request)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 request[44]	      = {0};
	u32 decorations	      = 0;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_set_borderless(&window, 0), 0);
	sock_read_all(&ss, peer, request, sizeof(request));

	cbuf_get_u32le(request, 32, &decorations);
	EXPECT_EQ(decorations, 1);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_fullscreen_null_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t window	      = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_set_fullscreen(&window, 1), 1);

	END;
}

TEST(display_x11_direct_window_set_fullscreen_unmapped_writes_property)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 request[28]	      = {0};
	u32 property	      = 0;
	u32 property_type     = 0;
	u32 length	      = 0;
	u32 state	      = 0;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_set_fullscreen(&window, 1), 0);
	sock_read_all(&ss, peer, request, sizeof(request));

	cbuf_get_u32le(request, 8, &property);
	cbuf_get_u32le(request, 12, &property_type);
	cbuf_get_u32le(request, 20, &length);
	cbuf_get_u32le(request, 24, &state);
	EXPECT_EQ(request[0], 18);
	EXPECT_EQ(property, 0x000000f6);
	EXPECT_EQ(property_type, 4);
	EXPECT_EQ(request[16], 32);
	EXPECT_EQ(length, 1);
	EXPECT_EQ(state, 0x000000f7);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_windowed_unmapped_writes_empty_property)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 request[24]	      = {0};
	u32 length	      = 0;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_set_fullscreen(&window, 0), 0);
	sock_read_all(&ss, peer, request, sizeof(request));

	cbuf_get_u32le(request, 20, &length);
	EXPECT_EQ(request[0], 18);
	EXPECT_EQ(length, 0);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_fullscreen_mapped_writes_client_message)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 show_request[8]    = {0};
	u8 request[44]	      = {0};
	u32 destination	      = 0;
	u32 mask	      = 0;
	u32 window_id	      = 0;
	u32 message_type      = 0;
	u32 action	      = 0;
	u32 state	      = 0;
	u32 source	      = 0;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_show(&window), 0);
	EXPECT_EQ(window_set_fullscreen(&window, 1), 0);
	sock_read_all(&ss, peer, show_request, sizeof(show_request));
	sock_read_all(&ss, peer, request, sizeof(request));

	cbuf_get_u32le(request, 4, &destination);
	cbuf_get_u32le(request, 8, &mask);
	cbuf_get_u32le(request, 16, &window_id);
	cbuf_get_u32le(request, 20, &message_type);
	cbuf_get_u32le(request, 24, &action);
	cbuf_get_u32le(request, 28, &state);
	cbuf_get_u32le(request, 36, &source);
	EXPECT_EQ(show_request[0], 8);
	EXPECT_EQ(request[0], 25);
	EXPECT_EQ(destination, 0x00000040);
	EXPECT_EQ(mask, (1u << 19) | (1u << 20));
	EXPECT_EQ(request[12], 33);
	EXPECT_EQ(request[13], 32);
	EXPECT_EQ(window_id, 0x00100000);
	EXPECT_EQ(message_type, 0x000000f6);
	EXPECT_EQ(action, 1);
	EXPECT_EQ(state, 0x000000f7);
	EXPECT_EQ(source, 1);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_windowed_mapped_writes_client_message)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 show_request[8]    = {0};
	u8 request[44]	      = {0};
	u32 action	      = 0;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_show(&window), 0);
	EXPECT_EQ(window_set_fullscreen(&window, 0), 0);
	sock_read_all(&ss, peer, show_request, sizeof(show_request));
	sock_read_all(&ss, peer, request, sizeof(request));

	cbuf_get_u32le(request, 24, &action);
	EXPECT_EQ(show_request[0], 8);
	EXPECT_EQ(request[0], 25);
	EXPECT_EQ(action, 0);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_fullscreen_mapped_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 show_request[8]    = {0};

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_show(&window), 0);
	sock_read_all(&ss, peer, show_request, sizeof(show_request));
	sock_close(&ss, peer);

	log_set_quiet(0, 1);
	EXPECT_EQ(window_set_fullscreen(&window, 1), 1);
	window_free(&window);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_set_position_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);
	sock_close(&ss, peer);

	log_set_quiet(0, 1);
	EXPECT_EQ(window_set_position(&window, 11, 22), 1);
	window_free(&window);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_visibility_writes_requests)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 hide_request[8]    = {0};
	u8 show_request[8]    = {0};

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);

	EXPECT_EQ(window_hide(&window), 0);
	EXPECT_EQ(window_show(&window), 0);
	sock_read_all(&ss, peer, hide_request, sizeof(hide_request));
	sock_read_all(&ss, peer, show_request, sizeof(show_request));

	EXPECT_EQ(hide_request[0], 10);
	EXPECT_EQ(show_request[0], 8);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_show_null_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t window	      = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_show(&window), 1);

	END;
}

TEST(display_x11_direct_window_hide_null_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t window	      = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_hide(&window), 1);

	END;
}

TEST(display_x11_direct_window_hide_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);
	sock_close(&ss, peer);

	log_set_quiet(0, 1);
	EXPECT_EQ(window_hide(&window), 1);
	window_free(&window);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_poll_event_no_event)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_poll_events(&display), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_NONE);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_poll_event_configure_notify)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_configure_event(&ss, peer, 0x00100000, 10, 20, 640, 480);

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_x11_event_calls, 1);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_RESIZE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);
	EXPECT_EQ(t_x11_event.x, 10);
	EXPECT_EQ(t_x11_event.y, 20);
	EXPECT_EQ(t_x11_event.width, 640);
	EXPECT_EQ(t_x11_event.height, 480);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_poll_event_null_display)
{
	START;

	display_driver_t *drv = t_x11_driver();

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->poll_events(NULL), 1);

	END;
}

TEST(display_x11_direct_poll_event_null_event)
{
	START;

	display_driver_t *drv = t_x11_driver();
	display_t display     = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->poll_events(&display), 1);

	END;
}

TEST(display_x11_direct_poll_event_get_flags_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	sock_t *display_ss    = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	display_ss = display.ss;
	display.ss = NULL;

	EXPECT_EQ(drv->poll_events(&display), 1);

	display.ss = display_ss;
	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_null_display)
{
	START;

	display_driver_t *drv = t_x11_driver();

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->wait_events(NULL), 1);

	END;
}

TEST(display_x11_direct_wait_event_null_event)
{
	START;

	display_driver_t *drv = t_x11_driver();
	display_t display     = {0};

	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->wait_events(&display), 1);

	END;
}

TEST(display_x11_direct_wait_event_unknown_event)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_unknown_event(&ss, peer);

	log_set_quiet(0, 1);
	EXPECT_EQ(display_wait_events(&display), 1);
	log_set_quiet(0, 0);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_configure_notify)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_configure_event(&ss, peer, 0x00100000, 10, 20, 640, 480);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_RESIZE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);
	EXPECT_EQ(t_x11_event.x, 10);
	EXPECT_EQ(t_x11_event.y, 20);
	EXPECT_EQ(t_x11_event.width, 640);
	EXPECT_EQ(t_x11_event.height, 480);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_skips_expose)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_expose_event(&ss, peer, 0x00100000);
	t_x11_write_configure_event(&ss, peer, 0x00100000, 10, 20, 640, 480);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_RESIZE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);
	EXPECT_EQ(t_x11_event.x, 10);
	EXPECT_EQ(t_x11_event.y, 20);
	EXPECT_EQ(t_x11_event.width, 640);
	EXPECT_EQ(t_x11_event.height, 480);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_skips_map_notify)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_map_event(&ss, peer, 0x00100000);
	t_x11_write_configure_event(&ss, peer, 0x00100000, 10, 20, 640, 480);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_RESIZE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);
	EXPECT_EQ(t_x11_event.x, 10);
	EXPECT_EQ(t_x11_event.y, 20);
	EXPECT_EQ(t_x11_event.width, 640);
	EXPECT_EQ(t_x11_event.height, 480);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_skips_unmap_notify)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_unmap_event(&ss, peer, 0x00100000);
	t_x11_write_configure_event(&ss, peer, 0x00100000, 10, 20, 640, 480);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_RESIZE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);
	EXPECT_EQ(t_x11_event.x, 10);
	EXPECT_EQ(t_x11_event.y, 20);
	EXPECT_EQ(t_x11_event.width, 640);
	EXPECT_EQ(t_x11_event.height, 480);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_skips_reparent_notify)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_reparent_event(&ss, peer, 0x00100000);
	t_x11_write_configure_event(&ss, peer, 0x00100000, 10, 20, 640, 480);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_RESIZE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);
	EXPECT_EQ(t_x11_event.x, 10);
	EXPECT_EQ(t_x11_event.y, 20);
	EXPECT_EQ(t_x11_event.width, 640);
	EXPECT_EQ(t_x11_event.height, 480);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_skips_mapping_notify)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_mapping_event(&ss, peer);
	t_x11_write_configure_event(&ss, peer, 0x00100000, 10, 20, 640, 480);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_RESIZE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);
	EXPECT_EQ(t_x11_event.x, 10);
	EXPECT_EQ(t_x11_event.y, 20);
	EXPECT_EQ(t_x11_event.width, 640);
	EXPECT_EQ(t_x11_event.height, 480);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_client_message_close)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_client_event(&ss, peer, 0x00100000);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_CLOSE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_skips_unknown_client_message)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_unknown_client_event(&ss, peer, 0x00100000);
	t_x11_write_configure_event(&ss, peer, 0x00100000, 10, 20, 640, 480);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_RESIZE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_inputs)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_key_event(&ss, peer, 2, 38, 0x00100000, 11, 22, 1);
	t_x11_write_key_event(&ss, peer, 3, 38, 0x00100000, 12, 23, 2);
	t_x11_write_key_event(&ss, peer, 2, 9, 0x00100000, 12, 23, 2);
	t_x11_write_key_event(&ss, peer, 2, 10, 0x00100000, 12, 23, 2);
	t_x11_write_key_event(&ss, peer, 2, 40, 0x00100000, 12, 23, 2);
	t_x11_write_key_event(&ss, peer, 4, 1, 0x00100000, 13, 24, 4);
	t_x11_write_key_event(&ss, peer, 5, 1, 0x00100000, 14, 25, 8);
	t_x11_write_motion_event(&ss, peer, 0x00100000, 15, 26, 16);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.window, 0x00100000);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_A);
	EXPECT_EQ(t_x11_event.x, 11);
	EXPECT_EQ(t_x11_event.y, 22);
	EXPECT_EQ(t_x11_event.modifiers, DISPLAY_MOD_SHIFT);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_UP);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_A);
	EXPECT_EQ(t_x11_event.modifiers, DISPLAY_MOD_CAPS_LOCK);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_ESCAPE);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_F1);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_UNKNOWN);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_LEFT);
	EXPECT_EQ(t_x11_event.modifiers, DISPLAY_MOD_CONTROL);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_UP);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_LEFT);
	EXPECT_EQ(t_x11_event.modifiers, DISPLAY_MOD_ALT);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_MOVE);
	EXPECT_EQ(t_x11_event.x, 15);
	EXPECT_EQ(t_x11_event.y, 26);
	EXPECT_EQ(t_x11_event.modifiers, DISPLAY_MOD_NUM_LOCK);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_mouse_buttons)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_key_event(&ss, peer, 4, 2, 0x00100000, 10, 20, 1);
	t_x11_write_key_event(&ss, peer, 4, 3, 0x00100000, 11, 21, 2);
	t_x11_write_key_event(&ss, peer, 4, 4, 0x00100000, 12, 22, 3);
	t_x11_write_key_event(&ss, peer, 4, 5, 0x00100000, 13, 23, 4);
	t_x11_write_key_event(&ss, peer, 4, 6, 0x00100000, 14, 24, 5);
	t_x11_write_key_event(&ss, peer, 4, 7, 0x00100000, 15, 25, 6);
	t_x11_write_key_event(&ss, peer, 4, 8, 0x00100000, 16, 26, 7);
	t_x11_write_key_event(&ss, peer, 4, 9, 0x00100000, 17, 27, 8);
	t_x11_write_key_event(&ss, peer, 4, 10, 0x00100000, 18, 28, 9);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_MIDDLE);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_RIGHT);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_WHEEL_UP);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_WHEEL_DOWN);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_WHEEL_LEFT);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_WHEEL_RIGHT);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_BACK);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_FORWARD);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_x11_event.button, DISPLAY_MOUSE_UNKNOWN);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_extended_keys)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_key_event(&ss, peer, 2, 30, 0x00100000, 10, 20, 0);
	t_x11_write_key_event(&ss, peer, 2, 31, 0x00100000, 10, 20, 0);
	t_x11_write_key_event(&ss, peer, 2, 32, 0x00100000, 10, 20, 0);
	t_x11_write_key_event(&ss, peer, 2, 39, 0x00100000, 10, 20, 0);
	t_x11_write_key_event(&ss, peer, 2, 33, 0x00100000, 10, 20, 0);
	t_x11_write_key_event(&ss, peer, 2, 34, 0x00100000, 10, 20, 0);
	t_x11_write_key_event(&ss, peer, 2, 35, 0x00100000, 10, 20, 0);
	t_x11_write_key_event(&ss, peer, 2, 36, 0x00100000, 10, 20, 0);
	t_x11_write_key_event(&ss, peer, 2, 37, 0x00100000, 10, 20, 0);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_SCROLL_LOCK);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_PAUSE);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_INSERT);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_PRINT_SCREEN);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_DELETE);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_HOME);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_END);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_PAGE_UP);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_PAGE_DOWN);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_keypad_keys)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 setup[72]	      = {0};
	u8 atom_reply[256]    = {0};
	buf_t keyboard	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	cbuf_set_u8le(setup, 27, 70);
	t_x11_default_atom_replies(atom_reply);
	buf_init(&keyboard, 32 + 63 * 4 + 32 + 16, ALLOC_STD);
	t_x11_extended_keyboard_mapping(&keyboard);
	t_x11_modifier_mapping(&keyboard);
	t_x11_script_setup_data_keyboard_atoms(
		&ss, server, 1, setup, sizeof(setup), keyboard.data, keyboard.used, atom_reply, sizeof(atom_reply));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), &display);
	t_x11_event_reset();
	display_set_event_callback(&display, t_x11_event_cb, NULL);
	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}), &window);
	log_set_quiet(0, 0);
	sock_accept(&ss, server, &peer);

	for (u8 keycode = 41; keycode <= 70; keycode++) {
		t_x11_write_key_event(&ss, peer, 2, keycode, 0x00100000, 10, 20, 0);
	}

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_GRAVE);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_EQUAL);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_MINUS);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_LEFT_BRACKET);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_RIGHT_BRACKET);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_BACKSLASH);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_SEMICOLON);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_APOSTROPHE);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_COMMA);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_PERIOD);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_SLASH);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_MENU);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_DIVIDE);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_MULTIPLY);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_SUBTRACT);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_ADD);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_ENTER);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_0);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_1);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_2);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_3);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_4);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_5);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_6);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_7);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_8);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_9);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_DECIMAL);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_0);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_KP_DECIMAL);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	buf_free(&keyboard);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_modifiers)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_key_event(&ss, peer, 2, 38, 0x00100000, 11, 22, 64 | 256 | 512 | 1024 | 2048 | 4096);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_x11_event.key, DISPLAY_KEY_A);
	EXPECT_EQ(t_x11_event.modifiers,
		  DISPLAY_MOD_SUPER | DISPLAY_MOD_MOUSE_LEFT | DISPLAY_MOD_MOUSE_MIDDLE | DISPLAY_MOD_MOUSE_RIGHT |
			  DISPLAY_MOD_MOUSE_WHEEL_UP | DISPLAY_MOD_MOUSE_WHEEL_DOWN);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_wait_event_focus_and_close)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_write_focus_event(&ss, peer, 9, 0x00100000);
	t_x11_write_focus_event(&ss, peer, 10, 0x00100000);
	t_x11_write_destroy_event(&ss, peer, 0x00100000);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_FOCUS_GAINED);
	EXPECT_EQ(t_x11_event.window, 0x00100000);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_FOCUS_LOST);
	EXPECT_EQ(t_x11_event.window, 0x00100000);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_x11_event.type, DISPLAY_EVENT_CLOSE);
	EXPECT_EQ(t_x11_event.window, 0x00100000);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_wild_authority)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority_family(&fs, 65535);
	t_x11_listen(&ss, &server);
	t_x11_script_setup(&ss, server);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), &display);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_unknown_authority_family)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 setup[12]	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority_family(&fs, 1);
	t_x11_listen(&ss, &server);
	t_x11_script_setup(&ss, server);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), &display);
	log_set_quiet(0, 0);

	sock_accept(&ss, server, &peer);
	sock_read_all(&ss, peer, setup, sizeof(setup));
	EXPECT_EQ(setup[6], 0);
	EXPECT_EQ(setup[8], 0);

	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_missing_display_env)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};

	t_x11_env_init(&fs, &proc, &ss);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_invalid_display_name)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV("localhost:0"));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_invalid_display_number)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":abc"));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_connect_not_found)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_missing_xauthority_env)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_listen(&ss, &server);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_missing_xauthority_file)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_listen(&ss, &server);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_missing_hostname)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	str_free(&proc.hostname);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_malformed_authority)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 malformed[]	      = {0x01, 0x00, 0x00};
	buf_t malformed_buf   = {.data = malformed, .used = sizeof(malformed)};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_file(&fs, STRV(T_XAUTHORITY), malformed_buf);
	t_x11_listen(&ss, &server);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_rejected_setup)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup_header(&ss, server, 0, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_short_setup)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup_header(&ss, server, 1, 1);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_truncated_setup)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup_header(&ss, server, 1, 8);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_setup_without_screens)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 0, 0);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_invalid_screen_offset)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[32]	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_setup_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	t_alloc_t state	      = {.fail_realloc = 1};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, t_alloc(&state)), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_invalid_keycode_range)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	cbuf_set_u8le(setup, 26, 40);
	cbuf_set_u8le(setup, 27, 8);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_keyboard_mapping_read_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_script_setup_data_keyboard_atoms(&ss, server, 1, setup, sizeof(setup), NULL, 0, NULL, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_keyboard_mapping_rejected)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	u8 keyboard_reply[32] = {0};
	u8 atom_reply[256]    = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_keyboard_reply(keyboard_reply, 0, 0, 0);
	t_x11_default_atom_replies(atom_reply);
	t_x11_script_setup_data_keyboard_atoms(
		&ss, server, 1, setup, sizeof(setup), keyboard_reply, sizeof(keyboard_reply), atom_reply, sizeof(atom_reply));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_keyboard_mapping_invalid)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	u8 keyboard_reply[32] = {0};
	u8 atom_reply[256]    = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_keyboard_reply(keyboard_reply, 1, 1, 1);
	t_x11_default_atom_replies(atom_reply);
	t_x11_script_setup_data_keyboard_atoms(
		&ss, server, 1, setup, sizeof(setup), keyboard_reply, sizeof(keyboard_reply), atom_reply, sizeof(atom_reply));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_keyboard_mapping_data_read_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	u8 keyboard_reply[32] = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_keyboard_reply(keyboard_reply, 1, 1, 33);
	t_x11_script_setup_data_keyboard_atoms(&ss, server, 1, setup, sizeof(setup), keyboard_reply, sizeof(keyboard_reply), NULL, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_keyboard_mapping_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	t_alloc_t state	      = {.fail_alloc_after = 3};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, t_alloc(&state)), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_modifier_mapping_read_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	buf_t keyboard	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	buf_init(&keyboard, 32 + 33 * 4, ALLOC_STD);
	t_x11_keyboard_mapping(&keyboard);
	t_x11_script_setup_data_keyboard_atoms(&ss, server, 1, setup, sizeof(setup), keyboard.data, keyboard.used, NULL, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	buf_free(&keyboard);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_modifier_mapping_rejected)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	u8 modifier_reply[32] = {0};
	buf_t mapping	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_modifier_reply(modifier_reply, 0, 0, 0);
	buf_init(&mapping, 32 + 33 * 4 + sizeof(modifier_reply), ALLOC_STD);
	t_x11_keyboard_mapping(&mapping);
	buf_add(&mapping, sizeof(modifier_reply), modifier_reply, NULL);
	t_x11_script_setup_data_keyboard_atoms(&ss, server, 1, setup, sizeof(setup), mapping.data, mapping.used, NULL, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	buf_free(&mapping);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_modifier_mapping_invalid)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	u8 modifier_reply[32] = {0};
	buf_t mapping	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_modifier_reply(modifier_reply, 1, 2, 3);
	buf_init(&mapping, 32 + 33 * 4 + sizeof(modifier_reply), ALLOC_STD);
	t_x11_keyboard_mapping(&mapping);
	buf_add(&mapping, sizeof(modifier_reply), modifier_reply, NULL);
	t_x11_script_setup_data_keyboard_atoms(&ss, server, 1, setup, sizeof(setup), mapping.data, mapping.used, NULL, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	buf_free(&mapping);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_modifier_mapping_data_read_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	u8 modifier_reply[32] = {0};
	buf_t mapping	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_modifier_reply(modifier_reply, 1, 2, 4);
	buf_init(&mapping, 32 + 33 * 4 + sizeof(modifier_reply), ALLOC_STD);
	t_x11_keyboard_mapping(&mapping);
	buf_add(&mapping, sizeof(modifier_reply), modifier_reply, NULL);
	t_x11_script_setup_data_keyboard_atoms(&ss, server, 1, setup, sizeof(setup), mapping.data, mapping.used, NULL, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	buf_free(&mapping);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_modifier_mapping_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	buf_t mapping	      = {0};
	t_alloc_t state	      = {.fail_alloc_after = 5};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	buf_init(&mapping, 32 + 33 * 4 + 32 + 16, ALLOC_STD);
	t_x11_keyboard_mapping(&mapping);
	t_x11_modifier_mapping(&mapping);
	t_x11_script_setup_data_keyboard_atoms(&ss, server, 1, setup, sizeof(setup), mapping.data, mapping.used, NULL, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, t_alloc(&state)), NULL);
	log_set_quiet(0, 0);
	buf_free(&mapping);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_empty_modifier_mapping)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 setup[72]	      = {0};
	u8 modifier_reply[32] = {0};
	u8 atom_reply[256]    = {0};
	buf_t mapping	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_modifier_reply(modifier_reply, 1, 0, 0);
	t_x11_default_atom_replies(atom_reply);
	buf_init(&mapping, 32 + 33 * 4 + sizeof(modifier_reply), ALLOC_STD);
	t_x11_keyboard_mapping(&mapping);
	buf_add(&mapping, sizeof(modifier_reply), modifier_reply, NULL);
	t_x11_script_setup_data_keyboard_atoms(
		&ss, server, 1, setup, sizeof(setup), mapping.data, mapping.used, atom_reply, sizeof(atom_reply));
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), &display);
	sock_accept(&ss, server, &peer);
	display_free(&display);
	sock_close(&ss, peer);
	buf_free(&mapping);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_intern_atom_read_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_script_setup_data_atoms(&ss, server, 1, setup, sizeof(setup), NULL, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);

	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_intern_atom_rejected_reply)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	u8 atom_reply[32]     = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_atom_reply(atom_reply, 0, 0);
	t_x11_script_setup_data_atoms(&ss, server, 1, setup, sizeof(setup), atom_reply, sizeof(atom_reply));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);

	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_intern_atom_missing)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	u8 atom_reply[32]     = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	t_x11_atom_reply(atom_reply, 1, 0);
	t_x11_script_setup_data_atoms(&ss, server, 1, setup, sizeof(setup), atom_reply, sizeof(atom_reply));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);

	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_socket_open_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	t_alloc_t state	      = {.fail_realloc = 1};

	log_set_quiet(0, 1);
	fs_init(&fs, 8, 1, ALLOC_STD);
	proc_init(&proc, 64, 1, ALLOC_STD);
	sock_init(&ss, 0, 1, t_alloc(&state));
	proc.hostname = STR("host");
	t_x11_set_display(&proc, STRV(":0"));
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_init_resource_exhausted)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t first	      = {0};
	window_t second	      = {0};
	window_t third	      = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	cbuf_set_u32le(setup, 8, 1);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	window_init(&first, &display, &(window_config_t){.width = 640, .height = 480});
	window_init(&second, &display, &(window_config_t){.width = 640, .height = 480});
	EXPECT_EQ(window_init(&third, &display, &(window_config_t){.width = 640, .height = 480}), NULL);
	log_set_quiet(0, 0);

	window_free(&second);
	window_free(&first);
	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_init_colormap_resource_exhausted)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t first	      = {0};
	window_t second	      = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	cbuf_set_u32le(setup, 8, 1);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	window_init(&first, &display, &(window_config_t){.width = 640, .height = 480});
	EXPECT_EQ(window_init(&second, &display, &(window_config_t){.width = 640, .height = 480, .depth = 24, .visual = 0x21}), NULL);
	log_set_quiet(0, 0);

	window_free(&first);
	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_init_colormap_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup(&ss, server);
	log_set_quiet(0, 1);
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	sock_accept(&ss, server, &peer);
	sock_close(&ss, peer);
	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480, .depth = 24, .visual = 0x21}), NULL);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_init_create_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup(&ss, server);
	log_set_quiet(0, 1);
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	sock_accept(&ss, server, &peer);
	sock_close(&ss, peer);
	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}), NULL);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_show_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window(drv, &fs, &proc, &ss, &display, &window, &server, &peer);
	t_x11_drain_open_window_requests(&ss, peer);
	sock_close(&ss, peer);

	log_set_quiet(0, 1);
	EXPECT_EQ(window_show(&window), 1);
	window_free(&window);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_init_wm_protocols_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 setup_request[48]  = {0};
	u8 keymap_request[8]  = {0};
	u8 modmap_request[4]  = {0};
	u8 atom_request[20]   = {0};
	u8 atom_request2[24]  = {0};
	u8 atom_request3[16]  = {0};
	u8 atom_request4[20]  = {0};
	u8 atom_request5[20]  = {0};
	u8 atom_request6[24]  = {0};
	u8 atom_request7[24]  = {0};
	u8 atom_request8[32]  = {0};
	size_t rcvbuf	      = 44;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup(&ss, server);
	log_set_quiet(0, 1);
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	sock_accept(&ss, server, &peer);
	sock_read_all(&ss, peer, setup_request, sizeof(setup_request));
	sock_read_all(&ss, peer, keymap_request, sizeof(keymap_request));
	sock_read_all(&ss, peer, modmap_request, sizeof(modmap_request));
	sock_read_all(&ss, peer, atom_request, sizeof(atom_request));
	sock_read_all(&ss, peer, atom_request2, sizeof(atom_request2));
	sock_read_all(&ss, peer, atom_request3, sizeof(atom_request3));
	sock_read_all(&ss, peer, atom_request4, sizeof(atom_request4));
	sock_read_all(&ss, peer, atom_request5, sizeof(atom_request5));
	sock_read_all(&ss, peer, atom_request6, sizeof(atom_request6));
	sock_read_all(&ss, peer, atom_request7, sizeof(atom_request7));
	sock_read_all(&ss, peer, atom_request8, sizeof(atom_request8));
	sock_setopt(&ss, peer, SOCK_OPT_RCVBUF, &rcvbuf, sizeof(rcvbuf));
	EXPECT_EQ(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}), NULL);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_free_destroy_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup(&ss, server);
	log_set_quiet(0, 1);
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	window_init(&window, &display, &(window_config_t){.width = 640, .height = 480});
	sock_accept(&ss, server, &peer);
	sock_close(&ss, peer);
	window_free(&window);
	log_set_quiet(0, 0);

	EXPECT_EQ(window.display, NULL);
	EXPECT_EQ(window.data, NULL);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_window_free_colormap_write_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_window_config(drv,
				 &fs,
				 &proc,
				 &ss,
				 &display,
				 &window,
				 &(window_config_t){.width = 640, .height = 480, .depth = 24, .visual = 0x21},
				 &server,
				 &peer);
	t_x11_drain_display_init_requests(&ss, peer);
	sock_read_all(&ss, peer, (u8[16]){0}, 16);
	sock_read_all(&ss, peer, (u8[48]){0}, 48);
	sock_read_all(&ss, peer, (u8[28]){0}, 28);
	sock_close(&ss, peer);

	log_set_quiet(0, 1);
	window_free(&window);
	log_set_quiet(0, 0);

	EXPECT_EQ(window.display, NULL);
	EXPECT_EQ(window.data, NULL);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_valid_authority_writes_cookie)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 setup[12]	      = {0};
	char auth_name[18]    = {0};
	u8 cookie[16]	      = {0};
	u8 padding[2]	      = {0};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);

	sock_accept(&ss, server, &peer);
	sock_read_all(&ss, peer, setup, sizeof(setup));
	sock_read_all(&ss, peer, auth_name, sizeof(auth_name));
	sock_read_all(&ss, peer, padding, sizeof(padding));
	sock_read_all(&ss, peer, cookie, sizeof(cookie));

	EXPECT_EQ(setup[0], 'l');
	EXPECT_EQ(setup[2], 11);
	EXPECT_EQ(setup[6], 18);
	EXPECT_EQ(setup[8], 16);
	EXPECT_STRN(auth_name, "MIT-MAGIC-COOKIE-1", sizeof(auth_name));
	EXPECT_EQ(mem_cmp(cookie, t_x11_cookie, sizeof(cookie)), 0);

	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_visual_depth_returns_setup_depth)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 depth	      = 0;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup_visual(&ss, server, 0x21, 32);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), &display);
	log_set_quiet(0, 0);
	EXPECT_EQ(display_visual_depth(&display, 0x21, &depth), 0);
	EXPECT_EQ(depth, 32);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_visual_depth_rejects_unknown_visual)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup_visual(&ss, server, 0x21, 32);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), &display);
	EXPECT_EQ(display_visual_depth(&display, 0x22, &(u8){0}), 1);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_ext_init_writes_query)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 reply[32]	      = {0};
	u8 request[16]	      = {0};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	cbuf_set_u8le(reply, 0, 1);
	cbuf_set_u8le(reply, 8, 1);
	cbuf_set_u8le(reply, 9, 130);
	cbuf_set_u8le(reply, 10, 80);
	cbuf_set_u8le(reply, 11, 160);
	sock_write_all(&ss, peer, reply, sizeof(reply));
	EXPECT_EQ(display_ext_init(&ext, &display, STRV("RANDR")), &ext);
	EXPECT_EQ(ext.opcode, 130);
	EXPECT_EQ(ext.first_event, 80);
	EXPECT_EQ(ext.first_error, 160);
	sock_read_all(&ss, peer, request, sizeof(request));
	EXPECT_EQ(request[0], 98);
	EXPECT_EQ(request[2], 4);
	EXPECT_EQ(request[4], 5);
	EXPECT_EQ(mem_cmp(&request[8], "RANDR", 5), 0);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_send_writes_request)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 data[]	      = {1, 2, 3, 4};
	u8 request[8]	      = {0};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	ext.display = &display;
	EXPECT_EQ(display_ext_send(&ext, 7, data, sizeof(data)), 0);
	sock_read_all(&ss, peer, request, sizeof(request));
	EXPECT_EQ(request[0], 130);
	EXPECT_EQ(request[1], 7);
	EXPECT_EQ(request[2], 2);
	EXPECT_EQ(mem_cmp(&request[4], data, sizeof(data)), 0);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_call_reads_reply_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	display_ext_reply_t reply;
	void *server	= NULL;
	void *peer	= NULL;
	u8 response[36] = {0};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	ext.display = &display;
	cbuf_set_u8le(response, 0, 1);
	cbuf_set_u32le(response, 4, 1);
	cbuf_set_u32le(response, 32, 0x12345678);
	sock_write_all(&ss, peer, response, sizeof(response));
	EXPECT_EQ(display_ext_call(&ext, 7, NULL, 0, &reply), 0);
	EXPECT_EQ(reply.size, 4);
	EXPECT_EQ(mem_cmp(reply.data, &response[32], 4), 0);
	display_ext_reply_free(&reply);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_call_accepts_empty_reply)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	display_ext_reply_t reply;
	void *server	= NULL;
	void *peer	= NULL;
	u8 response[32] = {1};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	ext.display = &display;
	sock_write_all(&ss, peer, response, sizeof(response));
	EXPECT_EQ(display_ext_call(&ext, 7, NULL, 0, &reply), 0);
	EXPECT_EQ(reply.size, 0);
	EXPECT_EQ(reply.data, NULL);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_init_rejects_unavailable_extension)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 response[32]	      = {1};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	sock_write_all(&ss, peer, response, sizeof(response));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_ext_init(&ext, &display, STRV("missing")), NULL);
	log_set_quiet(0, 0);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_call_rejects_error_reply)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	display_ext_reply_t reply;
	void *server	= NULL;
	void *peer	= NULL;
	u8 response[32] = {0};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	ext.display = &display;
	cbuf_set_u8le(response, 1, 2);
	cbuf_set_u16le(response, 2, 3);
	cbuf_set_u32le(response, 4, 4);
	cbuf_set_u16le(response, 8, 5);
	cbuf_set_u8le(response, 10, 130);
	sock_write_all(&ss, peer, response, sizeof(response));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_ext_call(&ext, 7, NULL, 0, &reply), 1);
	log_set_quiet(0, 0);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_call_rejects_unexpected_reply)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	display_ext_reply_t reply;
	void *server	= NULL;
	void *peer	= NULL;
	u8 response[32] = {2};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	ext.display = &display;
	sock_write_all(&ss, peer, response, sizeof(response));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_ext_call(&ext, 7, NULL, 0, &reply), 1);
	log_set_quiet(0, 0);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_rejects_invalid_driver_arguments)
{
	START;

	display_driver_t *drv = t_x11_driver();
	display_t display     = {0};
	display_ext_t ext     = {.display = &display};

	EXPECT_EQ(drv->ext_init(NULL, STRV("test")), 1);
	EXPECT_EQ(drv->ext_init(&(display_ext_t){0}, STRV("test")), 1);
	EXPECT_EQ(drv->ext_init(&ext, STRVN("test", (size_t)UINT16_MAX + 1)), 1);
	EXPECT_EQ(drv->ext_send(NULL, 0, NULL, 0), 1);
	EXPECT_EQ(drv->ext_send(&(display_ext_t){0}, 0, NULL, 0), 1);
	EXPECT_EQ(drv->ext_send(&ext, 0, NULL, 1), 1);
	EXPECT_EQ(drv->ext_send(&ext, 0, NULL, (size_t)UINT16_MAX * 4), 1);
	EXPECT_EQ(drv->alloc_id(NULL, &(u32){0}), 1);
	EXPECT_EQ(drv->alloc_id(&display, &(u32){0}), 1);
	EXPECT_EQ(drv->visual_depth(NULL, 1, &(u8){0}), 1);
	EXPECT_EQ(drv->visual_depth(&display, 1, &(u8){0}), 1);

	END;
}

TEST(display_x11_direct_ext_init_rejects_invalid_name_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.display = &display};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	EXPECT_EQ(drv->ext_init(&ext, STRVN(NULL, 1)), 1);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_send_rejects_invalid_data)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130, .display = &display};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	EXPECT_EQ(drv->ext_send(&ext, 1, NULL, 4), 1);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_init_rejects_truncated_depth)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};

	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	cbuf_set_u8le(setup, 71, 1);
	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);

	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_rejects_truncated_visual)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[80]	      = {0};

	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	cbuf_set_u8le(setup, 71, 1);
	cbuf_set_u16le(setup, 74, 1);
	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);

	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_init_rejects_visual_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	t_alloc_t state	      = {.fail_alloc_after = 4};
	void *server	      = NULL;
	u8 setup[104]	      = {0};

	t_x11_setup_data(setup, sizeof(setup), 1, 0);
	cbuf_set_u8le(setup, 71, 1);
	cbuf_set_u8le(setup, 72, 24);
	cbuf_set_u16le(setup, 74, 1);
	cbuf_set_u32le(setup, 80, 0x21);
	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup_data(&ss, server, 1, setup, sizeof(setup));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, t_alloc(&state)), NULL);
	log_set_quiet(0, 0);

	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_ext_init_rejects_request_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {0};
	t_alloc_t state	      = {.fail_alloc_after = 1};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	display.alloc = t_alloc(&state);
	EXPECT_EQ(display_ext_init(&ext, &display, STRV("RANDR")), NULL);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_send_rejects_request_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	t_alloc_t state	      = {.fail_alloc_after = 1};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	display.alloc = t_alloc(&state);
	ext.display   = &display;
	EXPECT_EQ(display_ext_send(&ext, 1, NULL, 0), 1);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_send_reuses_request_buffer)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	t_alloc_t state	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 request[8]	      = {0};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	display.alloc = t_alloc(&state);
	ext.display   = &display;
	EXPECT_EQ(display_ext_send(&ext, 1, NULL, 0), 0);
	EXPECT_EQ(display_ext_send(&ext, 2, NULL, 0), 0);
	EXPECT_EQ(state.alloc_calls, 1);
	sock_read_all(&ss, peer, request, sizeof(request));

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_send_rejects_buffer_growth_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	t_alloc_t state	      = {0};
	void *server	      = NULL;
	void *peer	      = NULL;
	u8 data[8]	      = {0};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	display.alloc = t_alloc(&state);
	ext.display   = &display;
	EXPECT_EQ(display_ext_send(&ext, 1, NULL, 0), 0);
	state.fail_realloc = 1;
	EXPECT_EQ(display_ext_send(&ext, 2, data, sizeof(data)), 1);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_call_rejects_reply_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	display_ext_reply_t reply;
	t_alloc_t state = {0};
	void *server	= NULL;
	void *peer	= NULL;
	u8 response[36] = {0};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	display.alloc = t_alloc(&state);
	ext.display   = &display;
	EXPECT_EQ(display_ext_send(&ext, 1, NULL, 0), 0);
	state.fail_alloc_after = state.alloc_calls + 1;
	cbuf_set_u8le(response, 0, 1);
	cbuf_set_u32le(response, 4, 1);
	sock_write_all(&ss, peer, response, sizeof(response));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_ext_call(&ext, 2, NULL, 0, &reply), 1);
	log_set_quiet(0, 0);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_call_handles_reply_discard_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	display_ext_reply_t reply;
	t_alloc_t state = {0};
	void *server	= NULL;
	void *peer	= NULL;
	u8 response[32] = {1};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	display.alloc = t_alloc(&state);
	ext.display   = &display;
	EXPECT_EQ(display_ext_send(&ext, 1, NULL, 0), 0);
	state.fail_alloc_after = state.alloc_calls + 1;
	cbuf_set_u32le(response, 4, 1);
	sock_write_all(&ss, peer, response, sizeof(response));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_ext_call(&ext, 2, NULL, 0, &reply), 1);
	log_set_quiet(0, 0);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_send_returns_socket_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	void *server	      = NULL;
	void *peer	      = NULL;

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	ext.display = &display;
	sock_close(&ss, peer);
	log_set_quiet(0, 1);
	EXPECT_NE(display_ext_send(&ext, 1, NULL, 0), 0);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_direct_ext_call_rejects_reply_read_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	display_ext_reply_t reply;
	void *server = NULL;
	void *peer   = NULL;

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	ext.display = &display;
	log_set_quiet(0, 1);
	EXPECT_EQ(display_ext_call(&ext, 1, NULL, 0, &reply), 1);
	log_set_quiet(0, 0);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

TEST(display_x11_direct_ext_call_rejects_reply_data_read_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	display_ext_t ext     = {.opcode = 130};
	display_ext_reply_t reply;
	void *server	= NULL;
	void *peer	= NULL;
	u8 response[32] = {1};

	t_x11_open_display(drv, &fs, &proc, &ss, &display, &server, &peer);
	ext.display = &display;
	cbuf_set_u32le(response, 4, 1);
	sock_write_all(&ss, peer, response, sizeof(response));
	log_set_quiet(0, 1);
	EXPECT_EQ(display_ext_call(&ext, 1, NULL, 0, &reply), 1);
	log_set_quiet(0, 0);

	t_x11_close_display(&fs, &proc, &ss, &display, server, peer);

	END;
}

STEST(display_x11_direct)
{
	SSTART;

	RUN(display_x11_direct_driver_is_registered);
	RUN(display_x11_direct_init_null_display);
	RUN(display_x11_direct_init_alloc_failure);
	RUN(display_x11_direct_free_null_display);
	RUN(display_x11_direct_window_init_null_window);
	RUN(display_x11_direct_window_init_alloc_failure);
	RUN(display_x11_direct_window_free_null_window);
	RUN(display_x11_direct_window_id_null_data);
	RUN(display_x11_direct_window_native_null_window);
	RUN(display_x11_direct_window_native_null_native);
	RUN(display_x11_direct_window_native_returns_window);
	RUN(display_x11_direct_init_success);
	RUN(display_x11_direct_window_init_writes_requests);
	RUN(display_x11_direct_window_init_custom_visual_writes_requests);
	RUN(display_x11_direct_window_free_custom_visual_writes_request);
	RUN(display_x11_direct_window_set_title_writes_requests);
	RUN(display_x11_direct_window_set_title_null_data);
	RUN(display_x11_direct_window_set_title_invalid_text);
	RUN(display_x11_direct_window_set_title_too_long);
	RUN(display_x11_direct_window_set_title_alloc_failure);
	RUN(display_x11_direct_window_set_title_write_failure);
	RUN(display_x11_direct_window_geometry_writes_requests);
	RUN(display_x11_direct_window_set_position_null_data);
	RUN(display_x11_direct_window_set_size_null_data);
	RUN(display_x11_direct_window_set_borderless_null_data);
	RUN(display_x11_direct_window_set_borderless_writes_request);
	RUN(display_x11_direct_window_set_bordered_writes_request);
	RUN(display_x11_direct_window_set_fullscreen_null_data);
	RUN(display_x11_direct_window_set_fullscreen_unmapped_writes_property);
	RUN(display_x11_direct_window_set_windowed_unmapped_writes_empty_property);
	RUN(display_x11_direct_window_set_fullscreen_mapped_writes_client_message);
	RUN(display_x11_direct_window_set_windowed_mapped_writes_client_message);
	RUN(display_x11_direct_window_set_fullscreen_mapped_write_failure);
	RUN(display_x11_direct_window_set_position_write_failure);
	RUN(display_x11_direct_window_visibility_writes_requests);
	RUN(display_x11_direct_window_show_null_data);
	RUN(display_x11_direct_window_hide_null_data);
	RUN(display_x11_direct_window_hide_write_failure);
	RUN(display_x11_direct_poll_event_no_event);
	RUN(display_x11_direct_poll_event_configure_notify);
	RUN(display_x11_direct_poll_event_null_display);
	RUN(display_x11_direct_poll_event_null_event);
	RUN(display_x11_direct_poll_event_get_flags_failure);
	RUN(display_x11_direct_wait_event_null_display);
	RUN(display_x11_direct_wait_event_null_event);
	RUN(display_x11_direct_wait_event_unknown_event);
	RUN(display_x11_direct_wait_event_configure_notify);
	RUN(display_x11_direct_wait_event_skips_expose);
	RUN(display_x11_direct_wait_event_skips_map_notify);
	RUN(display_x11_direct_wait_event_skips_unmap_notify);
	RUN(display_x11_direct_wait_event_skips_reparent_notify);
	RUN(display_x11_direct_wait_event_skips_mapping_notify);
	RUN(display_x11_direct_wait_event_client_message_close);
	RUN(display_x11_direct_wait_event_skips_unknown_client_message);
	RUN(display_x11_direct_wait_event_inputs);
	RUN(display_x11_direct_wait_event_mouse_buttons);
	RUN(display_x11_direct_wait_event_extended_keys);
	RUN(display_x11_direct_wait_event_keypad_keys);
	RUN(display_x11_direct_wait_event_modifiers);
	RUN(display_x11_direct_wait_event_focus_and_close);
	RUN(display_x11_direct_init_wild_authority);
	RUN(display_x11_direct_init_unknown_authority_family);
	RUN(display_x11_direct_init_missing_display_env);
	RUN(display_x11_direct_init_invalid_display_name);
	RUN(display_x11_direct_init_invalid_display_number);
	RUN(display_x11_direct_init_connect_not_found);
	RUN(display_x11_direct_init_missing_xauthority_env);
	RUN(display_x11_direct_init_missing_xauthority_file);
	RUN(display_x11_direct_init_missing_hostname);
	RUN(display_x11_direct_init_malformed_authority);
	RUN(display_x11_direct_init_rejected_setup);
	RUN(display_x11_direct_init_short_setup);
	RUN(display_x11_direct_init_truncated_setup);
	RUN(display_x11_direct_init_setup_without_screens);
	RUN(display_x11_direct_init_invalid_screen_offset);
	RUN(display_x11_direct_init_setup_alloc_failure);
	RUN(display_x11_direct_init_invalid_keycode_range);
	RUN(display_x11_direct_init_keyboard_mapping_read_failure);
	RUN(display_x11_direct_init_keyboard_mapping_rejected);
	RUN(display_x11_direct_init_keyboard_mapping_invalid);
	RUN(display_x11_direct_init_keyboard_mapping_data_read_failure);
	RUN(display_x11_direct_init_keyboard_mapping_alloc_failure);
	RUN(display_x11_direct_init_modifier_mapping_read_failure);
	RUN(display_x11_direct_init_modifier_mapping_rejected);
	RUN(display_x11_direct_init_modifier_mapping_invalid);
	RUN(display_x11_direct_init_modifier_mapping_data_read_failure);
	RUN(display_x11_direct_init_modifier_mapping_alloc_failure);
	RUN(display_x11_direct_init_empty_modifier_mapping);
	RUN(display_x11_direct_init_intern_atom_read_failure);
	RUN(display_x11_direct_init_intern_atom_rejected_reply);
	RUN(display_x11_direct_init_intern_atom_missing);
	RUN(display_x11_direct_init_socket_open_failure);
	RUN(display_x11_direct_window_init_resource_exhausted);
	RUN(display_x11_direct_window_init_colormap_resource_exhausted);
	RUN(display_x11_direct_window_init_colormap_write_failure);
	RUN(display_x11_direct_window_init_create_write_failure);
	RUN(display_x11_direct_window_init_wm_protocols_write_failure);
	RUN(display_x11_direct_window_show_write_failure);
	RUN(display_x11_direct_window_free_destroy_write_failure);
	RUN(display_x11_direct_window_free_colormap_write_failure);
	RUN(display_x11_direct_init_valid_authority_writes_cookie);
	RUN(display_x11_direct_visual_depth_returns_setup_depth);
	RUN(display_x11_direct_visual_depth_rejects_unknown_visual);
	RUN(display_x11_direct_ext_init_writes_query);
	RUN(display_x11_direct_ext_send_writes_request);
	RUN(display_x11_direct_ext_call_reads_reply_data);
	RUN(display_x11_direct_ext_call_accepts_empty_reply);
	RUN(display_x11_direct_ext_init_rejects_unavailable_extension);
	RUN(display_x11_direct_ext_call_rejects_error_reply);
	RUN(display_x11_direct_ext_call_rejects_unexpected_reply);
	RUN(display_x11_direct_ext_rejects_invalid_driver_arguments);
	RUN(display_x11_direct_ext_init_rejects_invalid_name_data);
	RUN(display_x11_direct_ext_send_rejects_invalid_data);
	RUN(display_x11_direct_init_rejects_truncated_depth);
	RUN(display_x11_direct_init_rejects_truncated_visual);
	RUN(display_x11_direct_init_rejects_visual_alloc_failure);
	RUN(display_x11_direct_ext_init_rejects_request_alloc_failure);
	RUN(display_x11_direct_ext_send_rejects_request_alloc_failure);
	RUN(display_x11_direct_ext_send_reuses_request_buffer);
	RUN(display_x11_direct_ext_send_rejects_buffer_growth_failure);
	RUN(display_x11_direct_ext_call_rejects_reply_alloc_failure);
	RUN(display_x11_direct_ext_call_handles_reply_discard_failure);
	RUN(display_x11_direct_ext_send_returns_socket_failure);
	RUN(display_x11_direct_ext_call_rejects_reply_read_failure);
	RUN(display_x11_direct_ext_call_rejects_reply_data_read_failure);

	SEND;
}
