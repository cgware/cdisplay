#include "test.h"

#include "cbuf.h"
#include "display.h"
#include "display_driver.h"
#include "log.h"
#include "mem.h"

#define T_XAUTHORITY "/xauthority"
#define T_X11_PATH   "/tmp/.X11-unix/X0"

static const u8 t_x11_cookie[] = {
	0xcf, 0x22, 0x17, 0x5a, 0xea, 0x39, 0x0a, 0xd7,
	0xb4, 0xb6, 0x47, 0xd3, 0x65, 0xac, 0xa0, 0x81,
};

typedef struct t_alloc_s {
	int fail_realloc;
} t_alloc_t;

static void *t_alloc_alloc(alloc_t *alloc, size_t size)
{
	(void)alloc;
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
		if (strv_eq(strv_cstr(drv->name), STRV("X11"))) {
			return drv;
		}
	}

	return NULL;
}

static void t_x11_env_init(fs_t *fs, proc_t *proc, sock_t *ss)
{
	fs_init(fs, 8, 1, ALLOC_STD);
	proc_init(proc, 64, 1);
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

static void t_x11_write_file(fs_t *fs, strv_t path, const void *data, size_t size)
{
	void *file;
	fs_open(fs, path, "w", &file);
	fs_write(fs, file, STRVN(data, size));
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

	t_x11_write_file(fs, STRV(T_XAUTHORITY), auth.data, auth.used);
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

	t_x11_add_authority(&auth, family, STRV("host"), STRV(""), STRV("MIT-MAGIC-COOKIE-1"),
			    STRVN((const char *)t_x11_cookie, sizeof(t_x11_cookie)));

	t_x11_write_file(fs, STRV(T_XAUTHORITY), auth.data, auth.used);
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
	}
}

static void t_x11_script_setup_data(sock_t *ss, void *server, u8 success, const void *setup_data, size_t setup_size)
{
	buf_t script = {0};
	u8 header[8] = {0};

	t_x11_setup_header(header, success, setup_size / 4);

	buf_init(&script, sizeof(header) + setup_size, ALLOC_STD);
	buf_add(&script, sizeof(header), header, NULL);
	buf_add(&script, setup_size, setup_data, NULL);
	sock_script(ss, server, script.data, script.used);
	buf_free(&script);
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

TEST(display_x11_driver_is_registered)
{
	START;

	EXPECT_NE(t_x11_driver(), NULL);

	END;
}

TEST(display_x11_init_null_display)
{
	START;

	display_driver_t *drv = t_x11_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->init(NULL), 1);

	END;
}

TEST(display_x11_init_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	display_t display     = {0};

	EXPECT_NE(drv, NULL);
	mem_oom(1);
	EXPECT_EQ(drv->init(&display), 1);
	mem_oom(0);

	END;
}

TEST(display_x11_free_null_display)
{
	START;

	display_driver_t *drv = t_x11_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->free(NULL), 1);

	END;
}

TEST(display_x11_window_init_null_window)
{
	START;

	display_driver_t *drv = t_x11_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_init(NULL, 0, 0), 1);

	END;
}

TEST(display_x11_window_init_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	window_t wnd	      = {0};

	EXPECT_NE(drv, NULL);
	mem_oom(1);
	EXPECT_EQ(drv->window_init(&wnd, 0, 0), 1);
	mem_oom(0);

	END;
}

TEST(display_x11_window_free_null_window)
{
	START;

	display_driver_t *drv = t_x11_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_free(NULL), 1);

	END;
}

TEST(display_x11_init_success)
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

TEST(display_x11_window_init_writes_requests)
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
	u8 create_request[44] = {0};
	u8 map_request[8]     = {0};
	u32 window_id	      = 0;
	u32 parent	      = 0;
	u16 x		      = 0;
	u16 y		      = 0;

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_authority(&fs);
	t_x11_listen(&ss, &server);
	t_x11_script_setup(&ss, server);
	log_set_quiet(0, 1);
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	EXPECT_EQ(window_init(&window, &display, 11, 22), &window);
	log_set_quiet(0, 0);

	sock_accept(&ss, server, &peer);
	sock_read_all(&ss, peer, setup_request, sizeof(setup_request));
	sock_read_all(&ss, peer, create_request, sizeof(create_request));
	sock_read_all(&ss, peer, map_request, sizeof(map_request));

	cbuf_get_u32le(create_request, 4, &window_id);
	cbuf_get_u32le(create_request, 8, &parent);
	cbuf_get_u16le(create_request, 12, &x);
	cbuf_get_u16le(create_request, 14, &y);
	EXPECT_EQ(create_request[0], 1);
	EXPECT_EQ(window_id, 0x00100000);
	EXPECT_EQ(parent, 0x00000040);
	EXPECT_EQ(x, 11);
	EXPECT_EQ(y, 22);
	EXPECT_EQ(map_request[0], 8);

	window_free(&window);
	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_init_wild_authority)
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

TEST(display_x11_init_unknown_authority_family)
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

TEST(display_x11_init_missing_display_env)
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

TEST(display_x11_init_invalid_display_name)
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

TEST(display_x11_init_invalid_display_number)
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

TEST(display_x11_init_connect_not_found)
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

TEST(display_x11_init_missing_xauthority_env)
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

TEST(display_x11_init_missing_xauthority_file)
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

TEST(display_x11_init_missing_hostname)
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

TEST(display_x11_init_malformed_authority)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 malformed[]	      = {0x01, 0x00, 0x00};

	t_x11_env_init(&fs, &proc, &ss);
	t_x11_set_display(&proc, STRV(":0"));
	t_x11_set_xauthority(&proc);
	t_x11_write_file(&fs, STRV(T_XAUTHORITY), malformed, sizeof(malformed));
	t_x11_listen(&ss, &server);
	log_set_quiet(0, 1);
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_init_rejected_setup)
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

TEST(display_x11_init_short_setup)
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

TEST(display_x11_init_truncated_setup)
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

TEST(display_x11_init_setup_without_screens)
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

TEST(display_x11_init_invalid_screen_offset)
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

TEST(display_x11_init_setup_alloc_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	void *server	      = NULL;
	u8 setup[72]	      = {0};
	t_alloc_t state       = {.fail_realloc = 1};

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

TEST(display_x11_init_socket_open_failure)
{
	START;

	display_driver_t *drv = t_x11_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	t_alloc_t state       = {.fail_realloc = 1};

	log_set_quiet(0, 1);
	fs_init(&fs, 8, 1, ALLOC_STD);
	proc_init(&proc, 64, 1);
	sock_init(&ss, 0, 1, t_alloc(&state));
	proc.hostname = STR("host");
	t_x11_set_display(&proc, STRV(":0"));
	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_window_init_resource_exhausted)
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
	window_init(&first, &display, 0, 0);
	window_init(&second, &display, 0, 0);
	EXPECT_EQ(window_init(&third, &display, 0, 0), NULL);
	log_set_quiet(0, 0);

	window_free(&second);
	window_free(&first);
	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_window_init_create_write_failure)
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
	EXPECT_EQ(window_init(&window, &display, 0, 0), NULL);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_window_init_map_write_failure)
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
	sock_setopt(&ss, peer, SOCK_OPT_RCVBUF, &rcvbuf, sizeof(rcvbuf));
	EXPECT_EQ(window_init(&window, &display, 0, 0), NULL);
	log_set_quiet(0, 0);

	display_free(&display);
	sock_close(&ss, peer);
	sock_close(&ss, server);
	t_x11_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_x11_window_free_destroy_write_failure)
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
	window_init(&window, &display, 0, 0);
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

TEST(display_x11_init_valid_authority_writes_cookie)
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

STEST(display_x11)
{
	SSTART;

	RUN(display_x11_driver_is_registered);
	RUN(display_x11_init_null_display);
	RUN(display_x11_init_alloc_failure);
	RUN(display_x11_free_null_display);
	RUN(display_x11_window_init_null_window);
	RUN(display_x11_window_init_alloc_failure);
	RUN(display_x11_window_free_null_window);
	RUN(display_x11_init_success);
	RUN(display_x11_window_init_writes_requests);
	RUN(display_x11_init_wild_authority);
	RUN(display_x11_init_unknown_authority_family);
	RUN(display_x11_init_missing_display_env);
	RUN(display_x11_init_invalid_display_name);
	RUN(display_x11_init_invalid_display_number);
	RUN(display_x11_init_connect_not_found);
	RUN(display_x11_init_missing_xauthority_env);
	RUN(display_x11_init_missing_xauthority_file);
	RUN(display_x11_init_missing_hostname);
	RUN(display_x11_init_malformed_authority);
	RUN(display_x11_init_rejected_setup);
	RUN(display_x11_init_short_setup);
	RUN(display_x11_init_truncated_setup);
	RUN(display_x11_init_setup_without_screens);
	RUN(display_x11_init_invalid_screen_offset);
	RUN(display_x11_init_setup_alloc_failure);
	RUN(display_x11_init_socket_open_failure);
	RUN(display_x11_window_init_resource_exhausted);
	RUN(display_x11_window_init_create_write_failure);
	RUN(display_x11_window_init_map_write_failure);
	RUN(display_x11_window_free_destroy_write_failure);
	RUN(display_x11_init_valid_authority_writes_cookie);

	SEND;
}
