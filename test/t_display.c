#include "test.h"

#include "display.h"
#include "display_driver.h"
#include "log.h"

static int t_display_init_calls;
static int t_display_free_calls;
static int t_display_poll_events_calls;
static int t_display_wait_events_calls;
static int t_display_native_calls;
static int t_display_native_free_calls;
static int t_display_init_ret;
static int t_display_poll_events_ret;
static int t_display_wait_events_ret;
static int t_display_native_ret;
static int t_display_native_free_ret;
static int t_display_event_calls;
static display_event_t t_display_event;
static display_native_t t_display_native;
static void *t_display_native_free_data;
static int t_display_log_calls;
static int t_display_log_level;
static const char *t_display_log_pkg;
static const char *t_display_log_file;
static char t_display_log_message[256];
static log_t t_display_log_state;

static int t_display_driver_init(display_t *display)
{
	t_display_init_calls++;
	display->data = (void *)0x1234;
	return t_display_init_ret;
}

static int t_display_driver_free(display_t *display)
{
	t_display_free_calls++;
	display->data = NULL;
	return 0;
}

static int t_display_poll_events(display_t *display)
{
	t_display_poll_events_calls++;
	display_event_t event = {
		.type = DISPLAY_EVENT_CLOSE,
	};
	display_emit_event(display, &event);
	return t_display_poll_events_ret;
}

static int t_display_wait_events(display_t *display)
{
	t_display_wait_events_calls++;
	display_event_t event = {
		.type = DISPLAY_EVENT_RESIZE,
	};
	display_emit_event(display, &event);
	return t_display_wait_events_ret;
}

static int t_display_native_fn(display_t *display, display_native_t *native)
{
	(void)display;
	t_display_native_calls++;
	*native = t_display_native;
	return t_display_native_ret;
}

static int t_display_native_free_fn(display_t *display, void *data)
{
	(void)display;
	t_display_native_free_calls++;
	t_display_native_free_data = data;
	return t_display_native_free_ret;
}

static int t_display_window_init(window_t *window, const window_config_t *config)
{
	(void)window;
	(void)config;
	return 0;
}

static int t_display_window_free(window_t *window)
{
	(void)window;
	return 0;
}

static display_driver_t t_display_driver = {
	.name	     = "test",
	.init	     = t_display_driver_init,
	.free	     = t_display_driver_free,
	.poll_events = t_display_poll_events,
	.wait_events = t_display_wait_events,
	.native	     = t_display_native_fn,
	.native_free = t_display_native_free_fn,
	.window_init = t_display_window_init,
	.window_free = t_display_window_free,
};

DRIVER(t_display_non_display_driver, 1, NULL);

static void t_display_reset(void)
{
	t_display_init_calls	    = 0;
	t_display_free_calls	    = 0;
	t_display_poll_events_calls = 0;
	t_display_wait_events_calls = 0;
	t_display_native_calls	    = 0;
	t_display_native_free_calls = 0;
	t_display_init_ret	    = 0;
	t_display_poll_events_ret   = 0;
	t_display_wait_events_ret   = 0;
	t_display_native_ret	    = 0;
	t_display_native_free_ret   = 0;
	t_display_event_calls	    = 0;
	t_display_event		    = (display_event_t){0};
	t_display_native	    = (display_native_t){0};
	t_display_native_free_data  = NULL;
	t_display_log_calls	    = 0;
	t_display_log_level	    = 0;
	t_display_log_pkg	    = NULL;
	t_display_log_file	    = NULL;
	t_display_log_message[0]    = 0;
	t_display_log_state	    = (log_t){0};
}

static void t_display_event_cb(display_t *display, const display_event_t *event, void *user)
{
	(void)display;
	(void)user;
	t_display_event_calls++;
	t_display_event = *event;
}

static size_t t_display_log(log_event_t *ev)
{
	t_display_log_calls++;
	t_display_log_level = ev->level;
	t_display_log_pkg   = ev->pkg;
	t_display_log_file  = ev->file;
	c_sprintv(t_display_log_message, sizeof(t_display_log_message), 0, ev->fmt, ev->ap);
	return 0;
}

static void t_display_log_init(void)
{
	t_display_reset();
	log_set(&t_display_log_state);
	log_add_callback(t_display_log, (dst_t){0}, LOG_INFO, 0, 0);
}

TEST(display_init_null_display)
{
	START;

	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};

	EXPECT_EQ(display_init(NULL, &t_display_driver, &fs, &proc, &ss, ALLOC_STD), NULL);

	END;
}

TEST(display_init_null_driver)
{
	START;

	display_t display = {0};
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};

	EXPECT_EQ(display_init(&display, NULL, &fs, &proc, &ss, ALLOC_STD), NULL);

	END;
}

TEST(display_init_null_fs)
{
	START;

	display_t display = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};

	EXPECT_EQ(display_init(&display, &t_display_driver, NULL, &proc, &ss, ALLOC_STD), NULL);

	END;
}

TEST(display_init_null_proc)
{
	START;

	display_t display = {0};
	fs_t fs		  = {0};
	sock_t ss	  = {0};

	EXPECT_EQ(display_init(&display, &t_display_driver, &fs, NULL, &ss, ALLOC_STD), NULL);

	END;
}

TEST(display_init_null_sock)
{
	START;

	display_t display = {0};
	fs_t fs		  = {0};
	proc_t proc	  = {0};

	EXPECT_EQ(display_init(&display, &t_display_driver, &fs, &proc, NULL, ALLOC_STD), NULL);

	END;
}

TEST(display_init_calls_driver)
{
	START;

	t_display_reset();
	display_t display = {0};
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};

	EXPECT_EQ(display_init(&display, &t_display_driver, &fs, &proc, &ss, ALLOC_STD), &display);
	EXPECT_EQ(t_display_init_calls, 1);

	END;
}

TEST(display_init_sets_fields)
{
	START;

	t_display_reset();
	display_t display = {0};
	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t ss	  = {0};

	display_init(&display, &t_display_driver, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(display.drv, &t_display_driver);
	EXPECT_EQ(display.fs, &fs);
	EXPECT_EQ(display.proc, &proc);
	EXPECT_EQ(display.ss, &ss);
	EXPECT_EQ(display.data, (void *)0x1234);

	END;
}

TEST(display_init_failure_returns_null)
{
	START;

	t_display_reset();
	t_display_init_ret = 1;
	display_t display  = {0};
	fs_t fs		   = {0};
	proc_t proc	   = {0};
	sock_t ss	   = {0};

	EXPECT_EQ(display_init(&display, &t_display_driver, &fs, &proc, &ss, ALLOC_STD), NULL);

	END;
}

TEST(display_init_failure_clears_fields)
{
	START;

	t_display_reset();
	t_display_init_ret = 1;
	display_t display  = {0};
	fs_t fs		   = {0};
	proc_t proc	   = {0};
	sock_t ss	   = {0};

	display_init(&display, &t_display_driver, &fs, &proc, &ss, ALLOC_STD);

	EXPECT_EQ(display.drv, NULL);
	EXPECT_EQ(display.fs, NULL);
	EXPECT_EQ(display.proc, NULL);
	EXPECT_EQ(display.ss, NULL);
	EXPECT_EQ(display.data, NULL);

	END;
}

TEST(display_free_null)
{
	START;

	display_free(NULL);

	END;
}

TEST(display_free_without_driver)
{
	START;

	display_t display = {0};
	display_free(&display);

	END;
}

TEST(display_free_calls_driver)
{
	START;

	t_display_reset();
	display_t display = {
		.drv  = &t_display_driver,
		.data = (void *)0x1234,
	};

	display_free(&display);

	EXPECT_EQ(t_display_free_calls, 1);

	END;
}

TEST(display_free_clears_fields)
{
	START;

	t_display_reset();
	display_t display = {
		.drv  = &t_display_driver,
		.fs   = (fs_t *)0x1,
		.proc = (proc_t *)0x2,
		.ss   = (sock_t *)0x3,
		.data = (void *)0x1234,
	};

	display_free(&display);

	EXPECT_EQ(display.drv, NULL);
	EXPECT_EQ(display.fs, NULL);
	EXPECT_EQ(display.proc, NULL);
	EXPECT_EQ(display.ss, NULL);
	EXPECT_EQ(display.data, NULL);

	END;
}

TEST(display_set_event_callback_null_display)
{
	START;

	EXPECT_EQ(display_set_event_callback(NULL, t_display_event_cb, NULL), 1);

	END;
}

TEST(display_set_event_callback_sets_fields)
{
	START;

	t_display_reset();
	display_t display     = {0};
	display_event_t event = {
		.type = DISPLAY_EVENT_CLOSE,
	};

	EXPECT_EQ(display_set_event_callback(&display, t_display_event_cb, (void *)0x1234), 0);
	EXPECT_EQ(display.event_user, (void *)0x1234);
	display_emit_event(&display, &event);
	EXPECT_EQ(t_display_event_calls, 1);
	EXPECT_EQ(t_display_event.type, DISPLAY_EVENT_CLOSE);

	END;
}

TEST(display_poll_events_null_display)
{
	START;

	EXPECT_EQ(display_poll_events(NULL), 1);

	END;
}

TEST(display_poll_events_without_driver)
{
	START;

	display_t display = {0};

	EXPECT_EQ(display_poll_events(&display), 1);

	END;
}

TEST(display_poll_events_calls_driver)
{
	START;

	t_display_reset();
	display_t display = {
		.drv = &t_display_driver,
	};
	display_set_event_callback(&display, t_display_event_cb, NULL);

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_display_poll_events_calls, 1);
	EXPECT_EQ(t_display_event_calls, 1);
	EXPECT_EQ(t_display_event.type, DISPLAY_EVENT_CLOSE);

	END;
}

TEST(display_poll_events_returns_driver_result)
{
	START;

	t_display_reset();
	t_display_poll_events_ret = 1;
	display_t display	  = {
			.drv = &t_display_driver,
	};

	EXPECT_EQ(display_poll_events(&display), 1);

	END;
}

TEST(display_wait_events_null_display)
{
	START;

	EXPECT_EQ(display_wait_events(NULL), 1);

	END;
}

TEST(display_wait_events_without_driver)
{
	START;

	display_t display = {0};

	EXPECT_EQ(display_wait_events(&display), 1);

	END;
}

TEST(display_wait_events_calls_driver)
{
	START;

	t_display_reset();
	display_t display = {
		.drv = &t_display_driver,
	};
	display_set_event_callback(&display, t_display_event_cb, NULL);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_display_wait_events_calls, 1);
	EXPECT_EQ(t_display_event_calls, 1);
	EXPECT_EQ(t_display_event.type, DISPLAY_EVENT_RESIZE);

	END;
}

TEST(display_wait_events_returns_driver_result)
{
	START;

	t_display_reset();
	t_display_wait_events_ret = 1;
	display_t display	  = {
			.drv = &t_display_driver,
	};

	EXPECT_EQ(display_wait_events(&display), 1);

	END;
}

TEST(display_native_null_display)
{
	START;

	display_native_t native = {0};

	EXPECT_EQ(display_native(NULL, &native), 1);

	END;
}

TEST(display_native_without_driver)
{
	START;

	display_t display	= {0};
	display_native_t native = {0};

	EXPECT_EQ(display_native(&display, &native), 1);

	END;
}

TEST(display_native_null_native)
{
	START;

	display_t display = {
		.drv = &t_display_driver,
	};

	EXPECT_EQ(display_native(&display, NULL), 1);

	END;
}

TEST(display_native_calls_driver)
{
	START;

	t_display_reset();
	display_t display = {
		.drv = &t_display_driver,
	};
	t_display_native = (display_native_t){
		.type	 = DISPLAY_NATIVE_X11,
		.display = (void *)0x1234,
		.screen	 = 7,
	};
	display_native_t native = {0};

	EXPECT_EQ(display_native(&display, &native), 0);
	EXPECT_EQ(t_display_native_calls, 1);
	EXPECT_EQ(native.type, DISPLAY_NATIVE_X11);
	EXPECT_EQ(native.display, (void *)0x1234);
	EXPECT_EQ(native.screen, 7);

	END;
}

TEST(display_native_returns_driver_result)
{
	START;

	t_display_reset();
	display_t display = {
		.drv = &t_display_driver,
	};
	display_native_t native = {0};
	t_display_native_ret	= 1;

	EXPECT_EQ(display_native(&display, &native), 1);

	END;
}

TEST(display_native_free_null_display)
{
	START;

	EXPECT_EQ(display_native_free(NULL, (void *)0x1234), 1);

	END;
}

TEST(display_native_free_without_driver)
{
	START;

	display_t display = {0};

	EXPECT_EQ(display_native_free(&display, (void *)0x1234), 1);

	END;
}

TEST(display_native_free_null_data)
{
	START;

	display_t display = {
		.drv = &t_display_driver,
	};

	EXPECT_EQ(display_native_free(&display, NULL), 1);

	END;
}

TEST(display_native_free_calls_driver)
{
	START;

	t_display_reset();
	display_t display = {
		.drv = &t_display_driver,
	};

	EXPECT_EQ(display_native_free(&display, (void *)0x1234), 0);
	EXPECT_EQ(t_display_native_free_calls, 1);
	EXPECT_EQ(t_display_native_free_data, (void *)0x1234);

	END;
}

TEST(display_native_free_returns_driver_result)
{
	START;

	t_display_reset();
	display_t display = {
		.drv = &t_display_driver,
	};
	t_display_native_free_ret = 1;

	EXPECT_EQ(display_native_free(&display, (void *)0x1234), 1);

	END;
}

TEST(display_driver_find_finds_registered_driver)
{
	START;

	EXPECT_NE(display_driver_find(STRV("none")), NULL);

	END;
}

TEST(display_driver_find_returns_null_for_missing_driver)
{
	START;

	EXPECT_EQ(display_driver_find(STRV("missing")), NULL);

	END;
}

TEST(display_driver_find_skips_non_display_driver)
{
	START;

	EXPECT_EQ(display_driver_find(STRV("t_display_non_display_driver")), NULL);

	END;
}

TEST(display_driver_list_lists_registered_display_drivers)
{
	START;

	u32 count		     = display_driver_list(NULL, 0);
	display_driver_t *drivers[1] = {0};

	EXPECT_GT(count, 0);
	EXPECT_EQ(display_driver_list(drivers, 1), count);
	EXPECT_NE(drivers[0], NULL);
	EXPECT_NE(display_driver_find(strv_cstr(drivers[0]->name)), NULL);

	END;
}

TEST(display_event_type_name_values)
{
	START;

	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_NONE), "none");
	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_CLOSE), "close");
	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_RESIZE), "resize");
	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_KEY_DOWN), "key down");
	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_KEY_UP), "key up");
	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_MOUSE_MOVE), "mouse move");
	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_MOUSE_DOWN), "mouse down");
	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_MOUSE_UP), "mouse up");
	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_FOCUS_GAINED), "focus gained");
	EXPECT_STR(display_event_type_name(DISPLAY_EVENT_FOCUS_LOST), "focus lost");
	EXPECT_STR(display_event_type_name((display_event_type_t)99), "unknown");

	END;
}

TEST(display_key_name_values)
{
	START;

	EXPECT_STR(display_key_name(DISPLAY_KEY_UNKNOWN), "unknown");
	EXPECT_STR(display_key_name(DISPLAY_KEY_A), "a");
	EXPECT_STR(display_key_name(DISPLAY_KEY_B), "b");
	EXPECT_STR(display_key_name(DISPLAY_KEY_C), "c");
	EXPECT_STR(display_key_name(DISPLAY_KEY_D), "d");
	EXPECT_STR(display_key_name(DISPLAY_KEY_E), "e");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F), "f");
	EXPECT_STR(display_key_name(DISPLAY_KEY_G), "g");
	EXPECT_STR(display_key_name(DISPLAY_KEY_H), "h");
	EXPECT_STR(display_key_name(DISPLAY_KEY_I), "i");
	EXPECT_STR(display_key_name(DISPLAY_KEY_J), "j");
	EXPECT_STR(display_key_name(DISPLAY_KEY_K), "k");
	EXPECT_STR(display_key_name(DISPLAY_KEY_L), "l");
	EXPECT_STR(display_key_name(DISPLAY_KEY_M), "m");
	EXPECT_STR(display_key_name(DISPLAY_KEY_N), "n");
	EXPECT_STR(display_key_name(DISPLAY_KEY_O), "o");
	EXPECT_STR(display_key_name(DISPLAY_KEY_P), "p");
	EXPECT_STR(display_key_name(DISPLAY_KEY_Q), "q");
	EXPECT_STR(display_key_name(DISPLAY_KEY_R), "r");
	EXPECT_STR(display_key_name(DISPLAY_KEY_S), "s");
	EXPECT_STR(display_key_name(DISPLAY_KEY_T), "t");
	EXPECT_STR(display_key_name(DISPLAY_KEY_U), "u");
	EXPECT_STR(display_key_name(DISPLAY_KEY_V), "v");
	EXPECT_STR(display_key_name(DISPLAY_KEY_W), "w");
	EXPECT_STR(display_key_name(DISPLAY_KEY_X), "x");
	EXPECT_STR(display_key_name(DISPLAY_KEY_Y), "y");
	EXPECT_STR(display_key_name(DISPLAY_KEY_Z), "z");
	EXPECT_STR(display_key_name(DISPLAY_KEY_0), "0");
	EXPECT_STR(display_key_name(DISPLAY_KEY_1), "1");
	EXPECT_STR(display_key_name(DISPLAY_KEY_2), "2");
	EXPECT_STR(display_key_name(DISPLAY_KEY_3), "3");
	EXPECT_STR(display_key_name(DISPLAY_KEY_4), "4");
	EXPECT_STR(display_key_name(DISPLAY_KEY_5), "5");
	EXPECT_STR(display_key_name(DISPLAY_KEY_6), "6");
	EXPECT_STR(display_key_name(DISPLAY_KEY_7), "7");
	EXPECT_STR(display_key_name(DISPLAY_KEY_8), "8");
	EXPECT_STR(display_key_name(DISPLAY_KEY_9), "9");
	EXPECT_STR(display_key_name(DISPLAY_KEY_GRAVE), "grave");
	EXPECT_STR(display_key_name(DISPLAY_KEY_MINUS), "minus");
	EXPECT_STR(display_key_name(DISPLAY_KEY_EQUAL), "equal");
	EXPECT_STR(display_key_name(DISPLAY_KEY_LEFT_BRACKET), "left bracket");
	EXPECT_STR(display_key_name(DISPLAY_KEY_RIGHT_BRACKET), "right bracket");
	EXPECT_STR(display_key_name(DISPLAY_KEY_BACKSLASH), "backslash");
	EXPECT_STR(display_key_name(DISPLAY_KEY_SEMICOLON), "semicolon");
	EXPECT_STR(display_key_name(DISPLAY_KEY_APOSTROPHE), "apostrophe");
	EXPECT_STR(display_key_name(DISPLAY_KEY_COMMA), "comma");
	EXPECT_STR(display_key_name(DISPLAY_KEY_PERIOD), "period");
	EXPECT_STR(display_key_name(DISPLAY_KEY_SLASH), "slash");
	EXPECT_STR(display_key_name(DISPLAY_KEY_SPACE), "space");
	EXPECT_STR(display_key_name(DISPLAY_KEY_ENTER), "enter");
	EXPECT_STR(display_key_name(DISPLAY_KEY_TAB), "tab");
	EXPECT_STR(display_key_name(DISPLAY_KEY_BACKSPACE), "backspace");
	EXPECT_STR(display_key_name(DISPLAY_KEY_ESCAPE), "escape");
	EXPECT_STR(display_key_name(DISPLAY_KEY_CAPS_LOCK), "caps lock");
	EXPECT_STR(display_key_name(DISPLAY_KEY_NUM_LOCK), "num lock");
	EXPECT_STR(display_key_name(DISPLAY_KEY_SCROLL_LOCK), "scroll lock");
	EXPECT_STR(display_key_name(DISPLAY_KEY_PAUSE), "pause");
	EXPECT_STR(display_key_name(DISPLAY_KEY_PRINT_SCREEN), "print screen");
	EXPECT_STR(display_key_name(DISPLAY_KEY_INSERT), "insert");
	EXPECT_STR(display_key_name(DISPLAY_KEY_DELETE), "delete");
	EXPECT_STR(display_key_name(DISPLAY_KEY_HOME), "home");
	EXPECT_STR(display_key_name(DISPLAY_KEY_END), "end");
	EXPECT_STR(display_key_name(DISPLAY_KEY_PAGE_UP), "page up");
	EXPECT_STR(display_key_name(DISPLAY_KEY_PAGE_DOWN), "page down");
	EXPECT_STR(display_key_name(DISPLAY_KEY_UP), "up");
	EXPECT_STR(display_key_name(DISPLAY_KEY_DOWN), "down");
	EXPECT_STR(display_key_name(DISPLAY_KEY_LEFT), "left");
	EXPECT_STR(display_key_name(DISPLAY_KEY_RIGHT), "right");
	EXPECT_STR(display_key_name(DISPLAY_KEY_LEFT_SHIFT), "left shift");
	EXPECT_STR(display_key_name(DISPLAY_KEY_RIGHT_SHIFT), "right shift");
	EXPECT_STR(display_key_name(DISPLAY_KEY_LEFT_CONTROL), "left control");
	EXPECT_STR(display_key_name(DISPLAY_KEY_RIGHT_CONTROL), "right control");
	EXPECT_STR(display_key_name(DISPLAY_KEY_LEFT_ALT), "left alt");
	EXPECT_STR(display_key_name(DISPLAY_KEY_RIGHT_ALT), "right alt");
	EXPECT_STR(display_key_name(DISPLAY_KEY_LEFT_SUPER), "left super");
	EXPECT_STR(display_key_name(DISPLAY_KEY_RIGHT_SUPER), "right super");
	EXPECT_STR(display_key_name(DISPLAY_KEY_MENU), "menu");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F1), "f1");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F2), "f2");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F3), "f3");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F4), "f4");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F5), "f5");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F6), "f6");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F7), "f7");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F8), "f8");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F9), "f9");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F10), "f10");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F11), "f11");
	EXPECT_STR(display_key_name(DISPLAY_KEY_F12), "f12");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_0), "kp 0");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_1), "kp 1");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_2), "kp 2");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_3), "kp 3");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_4), "kp 4");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_5), "kp 5");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_6), "kp 6");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_7), "kp 7");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_8), "kp 8");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_9), "kp 9");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_DECIMAL), "kp decimal");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_DIVIDE), "kp divide");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_MULTIPLY), "kp multiply");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_SUBTRACT), "kp subtract");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_ADD), "kp add");
	EXPECT_STR(display_key_name(DISPLAY_KEY_KP_ENTER), "kp enter");
	EXPECT_STR(display_key_name((display_key_t)999), "unknown");

	END;
}

TEST(display_mouse_name_values)
{
	START;

	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_UNKNOWN), "unknown");
	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_LEFT), "left");
	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_MIDDLE), "middle");
	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_RIGHT), "right");
	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_WHEEL_UP), "wheel up");
	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_WHEEL_DOWN), "wheel down");
	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_WHEEL_LEFT), "wheel left");
	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_WHEEL_RIGHT), "wheel right");
	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_BACK), "back");
	EXPECT_STR(display_mouse_name(DISPLAY_MOUSE_FORWARD), "forward");
	EXPECT_STR(display_mouse_name((display_mouse_t)99), "unknown");

	END;
}

TEST(display_modifier_name_values)
{
	START;

	EXPECT_STR(display_modifier_name(DISPLAY_MOD_NONE), "none");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_SHIFT), "shift");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_CAPS_LOCK), "caps lock");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_CONTROL), "control");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_ALT), "alt");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_NUM_LOCK), "num lock");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_SUPER), "super");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_MOUSE_LEFT), "mouse left");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_MOUSE_MIDDLE), "mouse middle");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_MOUSE_RIGHT), "mouse right");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_MOUSE_WHEEL_UP), "mouse wheel up");
	EXPECT_STR(display_modifier_name(DISPLAY_MOD_MOUSE_WHEEL_DOWN), "mouse wheel down");
	EXPECT_STR(display_modifier_name((display_modifier_t)0x80000000u), "unknown");

	END;
}

TEST(display_modifiers_format_null_buffer)
{
	START;

	display_modifiers_format(DISPLAY_MOD_SHIFT, NULL, 32);

	END;
}

TEST(display_modifiers_format_zero_size)
{
	START;

	char modifiers[1] = {0};

	display_modifiers_format(DISPLAY_MOD_SHIFT, modifiers, 0);

	EXPECT_EQ(modifiers[0], 0);

	END;
}

TEST(display_modifiers_format_none)
{
	START;

	char modifiers[32] = {0};

	display_modifiers_format(DISPLAY_MOD_NONE, modifiers, sizeof(modifiers));

	EXPECT_STR(modifiers, "none");

	END;
}

TEST(display_modifiers_format_values)
{
	START;

	char modifiers[256] = {0};

	display_modifiers_format(DISPLAY_MOD_SHIFT | DISPLAY_MOD_CAPS_LOCK | DISPLAY_MOD_CONTROL | DISPLAY_MOD_ALT | DISPLAY_MOD_NUM_LOCK |
					 DISPLAY_MOD_SUPER | DISPLAY_MOD_MOUSE_LEFT | DISPLAY_MOD_MOUSE_MIDDLE | DISPLAY_MOD_MOUSE_RIGHT |
					 DISPLAY_MOD_MOUSE_WHEEL_UP | DISPLAY_MOD_MOUSE_WHEEL_DOWN,
				 modifiers,
				 sizeof(modifiers));

	EXPECT_STR(modifiers,
		   "shift|caps lock|control|alt|num lock|super|mouse left|mouse middle|mouse right|mouse wheel up|mouse wheel down");

	END;
}

TEST(display_modifiers_format_unknown)
{
	START;

	char modifiers[32] = {0};

	display_modifiers_format((display_modifier_t)0x80000000u, modifiers, sizeof(modifiers));

	EXPECT_STR(modifiers, "unknown");

	END;
}

TEST(display_event_log_null_event)
{
	START;

	t_display_log_init();

	display_event_log(NULL);

	EXPECT_EQ(t_display_log_calls, 0);

	END;
}

TEST(display_event_log_close)
{
	START;

	t_display_log_init();
	display_event_t event = {
		.type	= DISPLAY_EVENT_CLOSE,
		.window = 42,
	};

	display_event_log(&event);

	EXPECT_EQ(t_display_log_calls, 1);
	EXPECT_EQ(t_display_log_level, LOG_INFO);
	EXPECT_STR(t_display_log_pkg, "cdisplay");
	EXPECT_STR(t_display_log_file, "display");
	EXPECT_STR(t_display_log_message, "event=close window=42");

	END;
}

TEST(display_event_log_resize)
{
	START;

	t_display_log_init();
	display_event_t event = {
		.type	= DISPLAY_EVENT_RESIZE,
		.window = 42,
		.x	= 1,
		.y	= 2,
		.width	= 640,
		.height = 480,
	};

	display_event_log(&event);

	EXPECT_STR(t_display_log_message, "event=resize window=42 pos=1,2 size=640x480");

	END;
}

TEST(display_event_log_key)
{
	START;

	t_display_log_init();
	display_event_t event = {
		.type	   = DISPLAY_EVENT_KEY_DOWN,
		.window	   = 42,
		.x	   = 3,
		.y	   = 4,
		.key	   = DISPLAY_KEY_ESCAPE,
		.modifiers = DISPLAY_MOD_NUM_LOCK,
	};

	display_event_log(&event);

	EXPECT_STR(t_display_log_message, "event=key down window=42 key=escape pos=3,4 mods=num lock");

	event.type = DISPLAY_EVENT_KEY_UP;
	event.key  = DISPLAY_KEY_F11;
	display_event_log(&event);

	EXPECT_STR(t_display_log_message, "event=key up window=42 key=f11 pos=3,4 mods=num lock");

	END;
}

TEST(display_event_log_mouse_move)
{
	START;

	t_display_log_init();
	display_event_t event = {
		.type	   = DISPLAY_EVENT_MOUSE_MOVE,
		.window	   = 42,
		.x	   = 5,
		.y	   = 6,
		.modifiers = DISPLAY_MOD_ALT,
	};

	display_event_log(&event);

	EXPECT_STR(t_display_log_message, "event=mouse move window=42 pos=5,6 mods=alt");

	END;
}

TEST(display_event_log_mouse_button)
{
	START;

	t_display_log_init();
	display_event_t event = {
		.type	   = DISPLAY_EVENT_MOUSE_DOWN,
		.window	   = 42,
		.x	   = 7,
		.y	   = 8,
		.button	   = DISPLAY_MOUSE_BACK,
		.modifiers = DISPLAY_MOD_CONTROL,
	};

	display_event_log(&event);

	EXPECT_STR(t_display_log_message, "event=mouse down window=42 button=back pos=7,8 mods=control");

	event.type   = DISPLAY_EVENT_MOUSE_UP;
	event.button = DISPLAY_MOUSE_FORWARD;
	display_event_log(&event);

	EXPECT_STR(t_display_log_message, "event=mouse up window=42 button=forward pos=7,8 mods=control");

	END;
}

STEST(display)
{
	SSTART;

	RUN(display_init_null_display);
	RUN(display_init_null_driver);
	RUN(display_init_null_fs);
	RUN(display_init_null_proc);
	RUN(display_init_null_sock);
	RUN(display_init_calls_driver);
	RUN(display_init_sets_fields);
	RUN(display_init_failure_returns_null);
	RUN(display_init_failure_clears_fields);
	RUN(display_free_null);
	RUN(display_free_without_driver);
	RUN(display_free_calls_driver);
	RUN(display_free_clears_fields);
	RUN(display_set_event_callback_null_display);
	RUN(display_set_event_callback_sets_fields);
	RUN(display_poll_events_null_display);
	RUN(display_poll_events_without_driver);
	RUN(display_poll_events_calls_driver);
	RUN(display_poll_events_returns_driver_result);
	RUN(display_wait_events_null_display);
	RUN(display_wait_events_without_driver);
	RUN(display_wait_events_calls_driver);
	RUN(display_wait_events_returns_driver_result);
	RUN(display_native_null_display);
	RUN(display_native_without_driver);
	RUN(display_native_null_native);
	RUN(display_native_calls_driver);
	RUN(display_native_returns_driver_result);
	RUN(display_native_free_null_display);
	RUN(display_native_free_without_driver);
	RUN(display_native_free_null_data);
	RUN(display_native_free_calls_driver);
	RUN(display_native_free_returns_driver_result);
	RUN(display_driver_find_finds_registered_driver);
	RUN(display_driver_find_returns_null_for_missing_driver);
	RUN(display_driver_find_skips_non_display_driver);
	RUN(display_driver_list_lists_registered_display_drivers);
	RUN(display_event_type_name_values);
	RUN(display_key_name_values);
	RUN(display_mouse_name_values);
	RUN(display_modifier_name_values);
	RUN(display_modifiers_format_null_buffer);
	RUN(display_modifiers_format_zero_size);
	RUN(display_modifiers_format_none);
	RUN(display_modifiers_format_values);
	RUN(display_modifiers_format_unknown);
	RUN(display_event_log_null_event);
	RUN(display_event_log_close);
	RUN(display_event_log_resize);
	RUN(display_event_log_key);
	RUN(display_event_log_mouse_move);
	RUN(display_event_log_mouse_button);

	SEND;
}
