#include "test.h"

#include "display_driver.h"
#include "window.h"

static int t_window_init_calls;
static int t_window_free_calls;
static int t_window_set_title_calls;
static int t_window_get_title_calls;
static int t_window_set_position_calls;
static int t_window_get_position_calls;
static int t_window_set_size_calls;
static int t_window_get_size_calls;
static int t_window_set_borderless_calls;
static int t_window_get_borderless_calls;
static int t_window_set_fullscreen_calls;
static int t_window_get_fullscreen_calls;
static int t_window_show_calls;
static int t_window_hide_calls;
static int t_window_native_calls;
static int t_window_init_ret;
static int t_window_set_title_ret;
static int t_window_get_title_ret;
static int t_window_set_position_ret;
static int t_window_get_position_ret;
static int t_window_set_size_ret;
static int t_window_get_size_ret;
static int t_window_set_borderless_ret;
static int t_window_get_borderless_ret;
static int t_window_set_fullscreen_ret;
static int t_window_get_fullscreen_ret;
static int t_window_show_ret;
static int t_window_hide_ret;
static int t_window_native_ret;
static u32 t_window_id_ret;
static window_native_t t_window_native;
static strv_t t_window_title;
static u16 t_window_x;
static u16 t_window_y;
static u16 t_window_width;
static u16 t_window_height;
static u8 t_window_depth;
static u32 t_window_visual;
static int t_window_borderless;
static int t_window_fullscreen;

static int t_window_display_init(display_t *display)
{
	(void)display;
	return 0;
}

static int t_window_display_free(display_t *display)
{
	(void)display;
	return 0;
}

static int t_window_poll_events(display_t *display)
{
	(void)display;
	return 1;
}

static int t_window_wait_events(display_t *display)
{
	(void)display;
	return 1;
}

static int t_window_driver_init(window_t *window, const window_config_t *config)
{
	t_window_init_calls++;
	t_window_x	= config->x;
	t_window_y	= config->y;
	t_window_width	= config->width;
	t_window_height = config->height;
	t_window_depth	= config->depth;
	t_window_visual = config->visual;
	window->data	= (void *)0x5678;
	return t_window_init_ret;
}

static int t_window_driver_free(window_t *window)
{
	t_window_free_calls++;
	window->data = NULL;
	return 0;
}

static u32 t_window_driver_id(window_t *window)
{
	(void)window;
	return t_window_id_ret;
}

static int t_window_driver_native(window_t *window, window_native_t *native)
{
	(void)window;
	t_window_native_calls++;
	*native = t_window_native;
	return t_window_native_ret;
}

static int t_window_driver_set_title(window_t *window, strv_t title)
{
	(void)window;
	t_window_set_title_calls++;
	t_window_title = title;
	return t_window_set_title_ret;
}

static int t_window_driver_get_title(window_t *window, char *title, size_t size)
{
	(void)window;
	t_window_get_title_calls++;
	if (title != NULL && size > 0) {
		size_t len = t_window_title.len < size - 1 ? t_window_title.len : size - 1;
		for (size_t i = 0; i < len; i++) {
			title[i] = t_window_title.data[i];
		}
		title[len] = 0;
	}
	return t_window_get_title_ret;
}

static int t_window_driver_set_position(window_t *window, u16 x, u16 y)
{
	(void)window;
	t_window_set_position_calls++;
	t_window_x = x;
	t_window_y = y;
	return t_window_set_position_ret;
}

static int t_window_driver_get_position(window_t *window, u16 *x, u16 *y)
{
	(void)window;
	t_window_get_position_calls++;
	if (x != NULL) {
		*x = t_window_x;
	}
	if (y != NULL) {
		*y = t_window_y;
	}
	return t_window_get_position_ret;
}

static int t_window_driver_set_size(window_t *window, u16 width, u16 height)
{
	(void)window;
	t_window_set_size_calls++;
	t_window_width	= width;
	t_window_height = height;
	return t_window_set_size_ret;
}

static int t_window_driver_get_size(window_t *window, u16 *width, u16 *height)
{
	(void)window;
	t_window_get_size_calls++;
	if (width != NULL) {
		*width = t_window_width;
	}
	if (height != NULL) {
		*height = t_window_height;
	}
	return t_window_get_size_ret;
}

static int t_window_driver_set_borderless(window_t *window, int borderless)
{
	(void)window;
	t_window_set_borderless_calls++;
	t_window_borderless = borderless;
	return t_window_set_borderless_ret;
}

static int t_window_driver_get_borderless(window_t *window, int *borderless)
{
	(void)window;
	t_window_get_borderless_calls++;
	if (borderless != NULL) {
		*borderless = t_window_borderless;
	}
	return t_window_get_borderless_ret;
}

static int t_window_driver_set_fullscreen(window_t *window, int fullscreen)
{
	(void)window;
	t_window_set_fullscreen_calls++;
	t_window_fullscreen = fullscreen;
	return t_window_set_fullscreen_ret;
}

static int t_window_driver_get_fullscreen(window_t *window, int *fullscreen)
{
	(void)window;
	t_window_get_fullscreen_calls++;
	if (fullscreen != NULL) {
		*fullscreen = t_window_fullscreen;
	}
	return t_window_get_fullscreen_ret;
}

static int t_window_driver_show(window_t *window)
{
	(void)window;
	t_window_show_calls++;
	return t_window_show_ret;
}

static int t_window_driver_hide(window_t *window)
{
	(void)window;
	t_window_hide_calls++;
	return t_window_hide_ret;
}

static display_driver_t t_window_driver = {
	.name		       = "test",
	.init		       = t_window_display_init,
	.free		       = t_window_display_free,
	.poll_events	       = t_window_poll_events,
	.wait_events	       = t_window_wait_events,
	.window_init	       = t_window_driver_init,
	.window_free	       = t_window_driver_free,
	.window_id	       = t_window_driver_id,
	.window_native	       = t_window_driver_native,
	.window_set_title      = t_window_driver_set_title,
	.window_get_title      = t_window_driver_get_title,
	.window_set_position   = t_window_driver_set_position,
	.window_get_position   = t_window_driver_get_position,
	.window_set_size       = t_window_driver_set_size,
	.window_get_size       = t_window_driver_get_size,
	.window_set_borderless = t_window_driver_set_borderless,
	.window_get_borderless = t_window_driver_get_borderless,
	.window_set_fullscreen = t_window_driver_set_fullscreen,
	.window_get_fullscreen = t_window_driver_get_fullscreen,
	.window_show	       = t_window_driver_show,
	.window_hide	       = t_window_driver_hide,
};

static void t_window_reset(void)
{
	t_window_init_calls	      = 0;
	t_window_free_calls	      = 0;
	t_window_set_title_calls      = 0;
	t_window_get_title_calls      = 0;
	t_window_set_position_calls   = 0;
	t_window_get_position_calls   = 0;
	t_window_set_size_calls	      = 0;
	t_window_get_size_calls	      = 0;
	t_window_set_borderless_calls = 0;
	t_window_get_borderless_calls = 0;
	t_window_set_fullscreen_calls = 0;
	t_window_get_fullscreen_calls = 0;
	t_window_show_calls	      = 0;
	t_window_hide_calls	      = 0;
	t_window_native_calls	      = 0;
	t_window_init_ret	      = 0;
	t_window_set_title_ret	      = 0;
	t_window_get_title_ret	      = 0;
	t_window_set_position_ret     = 0;
	t_window_get_position_ret     = 0;
	t_window_set_size_ret	      = 0;
	t_window_get_size_ret	      = 0;
	t_window_set_borderless_ret   = 0;
	t_window_get_borderless_ret   = 0;
	t_window_set_fullscreen_ret   = 0;
	t_window_get_fullscreen_ret   = 0;
	t_window_show_ret	      = 0;
	t_window_hide_ret	      = 0;
	t_window_native_ret	      = 0;
	t_window_id_ret		      = 0;
	t_window_native		      = (window_native_t){0};
	t_window_title		      = (strv_t){0};
	t_window_x		      = 0;
	t_window_y		      = 0;
	t_window_width		      = 0;
	t_window_height		      = 0;
	t_window_depth		      = 0;
	t_window_visual		      = 0;
	t_window_borderless	      = 0;
	t_window_fullscreen	      = 0;
}

TEST(window_init_null_window)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};

	EXPECT_NULL(window_init(NULL, &display, &(window_config_t){.width = 640, .height = 480}));

	END;
}

TEST(window_init_null_display)
{
	START;

	window_t window = {0};

	EXPECT_NULL(window_init(&window, NULL, &(window_config_t){.width = 640, .height = 480}));

	END;
}

TEST(window_init_display_without_driver)
{
	START;

	display_t display = {0};
	window_t window	  = {0};

	EXPECT_NULL(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}));

	END;
}

TEST(window_init_null_config)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {0};

	EXPECT_NULL(window_init(&window, &display, NULL));
	EXPECT_EQ(t_window_init_calls, 0);

	END;
}

TEST(window_init_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {0};

	EXPECT_PTR(window_init(&window, &display, &(window_config_t){.x = 11, .y = 22, .width = 640, .height = 480}), &window);
	EXPECT_EQ(t_window_init_calls, 1);

	END;
}

TEST(window_init_passes_config)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {0};

	window_init(
		&window, &display, &(window_config_t){.x = 11, .y = 22, .width = 333, .height = 444, .depth = 24, .visual = 0x12345678});

	EXPECT_EQ(t_window_x, 11);
	EXPECT_EQ(t_window_y, 22);
	EXPECT_EQ(t_window_width, 333);
	EXPECT_EQ(t_window_height, 444);
	EXPECT_EQ(t_window_depth, 24);
	EXPECT_EQ(t_window_visual, 0x12345678);

	END;
}

TEST(window_init_sets_fields)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {0};

	window_init(&window, &display, &(window_config_t){.width = 640, .height = 480});

	EXPECT_PTR(window.display, &display);
	EXPECT_PTR(window.data, (void *)0x5678);

	END;
}

TEST(window_init_failure_returns_null)
{
	START;

	t_window_reset();
	t_window_init_ret = 1;
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {0};

	EXPECT_NULL(window_init(&window, &display, &(window_config_t){.width = 640, .height = 480}));

	END;
}

TEST(window_init_failure_clears_fields)
{
	START;

	t_window_reset();
	t_window_init_ret = 1;
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {0};

	window_init(&window, &display, &(window_config_t){.width = 640, .height = 480});

	EXPECT_NULL(window.display);
	EXPECT_NULL(window.data);

	END;
}

TEST(window_free_null)
{
	START;

	window_free(NULL);

	END;
}

TEST(window_free_without_display)
{
	START;

	window_t window = {0};

	window_free(&window);

	END;
}

TEST(window_free_display_without_driver)
{
	START;

	display_t display = {0};

	window_t window = {
		.display = &display,
	};

	window_free(&window);

	END;
}

TEST(window_free_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	window_free(&window);

	EXPECT_EQ(t_window_free_calls, 1);

	END;
}

TEST(window_free_clears_fields)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	window_free(&window);

	EXPECT_NULL(window.display);
	EXPECT_NULL(window.data);

	END;
}

TEST(window_id_null_window)
{
	START;

	EXPECT_EQ(window_id(NULL), 0);

	END;
}

TEST(window_id_without_display)
{
	START;

	window_t window = {0};

	EXPECT_EQ(window_id(&window), 0);

	END;
}

TEST(window_id_display_without_driver)
{
	START;

	display_t display = {0};

	window_t window = {
		.display = &display,
	};

	EXPECT_EQ(window_id(&window), 0);

	END;
}

TEST(window_id_calls_driver)
{
	START;

	t_window_reset();
	t_window_id_ret	  = 0x12345678;
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_id(&window), 0x12345678);

	END;
}

TEST(window_native_null_window)
{
	START;

	window_native_t native = {0};

	EXPECT_EQ(window_native(NULL, &native), 1);

	END;
}

TEST(window_native_without_display)
{
	START;

	window_t window	       = {0};
	window_native_t native = {0};

	EXPECT_EQ(window_native(&window, &native), 1);

	END;
}

TEST(window_native_display_without_driver)
{
	START;

	display_t display = {0};

	window_t window = {
		.display = &display,
	};
	window_native_t native = {0};

	EXPECT_EQ(window_native(&window, &native), 1);

	END;
}

TEST(window_native_null_native)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
	};

	EXPECT_EQ(window_native(&window, NULL), 1);

	END;
}

TEST(window_native_calls_driver)
{
	START;

	t_window_reset();
	t_window_native = (window_native_t){
		.type	= DISPLAY_NATIVE_WINDOWS,
		.window = (void *)0x1234,
	};
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	window_native_t native = {0};

	EXPECT_EQ(window_native(&window, &native), 0);
	EXPECT_EQ(t_window_native_calls, 1);
	EXPECT_EQ(native.type, DISPLAY_NATIVE_WINDOWS);
	EXPECT_PTR(native.window, (void *)0x1234);

	END;
}

TEST(window_native_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_native_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	window_native_t native = {0};

	EXPECT_EQ(window_native(&window, &native), 1);

	END;
}

TEST(window_set_title_null_window)
{
	START;

	EXPECT_EQ(window_set_title(NULL, STRV("title")), 1);

	END;
}

TEST(window_set_title_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_title(&window, STRV("title")), 0);
	EXPECT_EQ(t_window_set_title_calls, 1);
	EXPECT_EQ(t_window_title.len, 5);
	EXPECT_EQ(t_window_title.data[0], 't');

	END;
}

TEST(window_set_title_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_set_title_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_title(&window, STRV("title")), 1);

	END;
}

TEST(window_get_title_null_window)
{
	START;

	char title[8] = {0};

	EXPECT_EQ(window_get_title(NULL, title, sizeof(title)), 1);

	END;
}

TEST(window_get_title_null_title)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
	};

	EXPECT_EQ(window_get_title(&window, NULL, 8), 1);

	END;
}

TEST(window_get_title_zero_size)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
	};
	char title[8] = {0};

	EXPECT_EQ(window_get_title(&window, title, 0), 1);

	END;
}

TEST(window_get_title_calls_driver)
{
	START;

	t_window_reset();
	t_window_title	  = STRV("title");
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	char title[8] = {0};

	window_get_title(&window, title, sizeof(title));

	EXPECT_EQ(t_window_get_title_calls, 1);

	END;
}

TEST(window_get_title_sets_title)
{
	START;

	t_window_reset();
	t_window_title	  = STRV("title");
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	char title[8] = {0};

	window_get_title(&window, title, sizeof(title));

	EXPECT_EQ(title[0], 't');

	END;
}

TEST(window_get_title_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_get_title_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	char title[8] = {0};

	EXPECT_EQ(window_get_title(&window, title, sizeof(title)), 1);

	END;
}

TEST(window_set_position_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_position(&window, 11, 22), 0);
	EXPECT_EQ(t_window_set_position_calls, 1);
	EXPECT_EQ(t_window_x, 11);
	EXPECT_EQ(t_window_y, 22);

	END;
}

TEST(window_set_position_null_window)
{
	START;

	EXPECT_EQ(window_set_position(NULL, 11, 22), 1);

	END;
}

TEST(window_set_position_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_set_position_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_position(&window, 11, 22), 1);

	END;
}

TEST(window_get_position_null_window)
{
	START;

	u16 x = 0;
	u16 y = 0;

	EXPECT_EQ(window_get_position(NULL, &x, &y), 1);

	END;
}

TEST(window_get_position_null_x)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
	};
	u16 y = 0;

	EXPECT_EQ(window_get_position(&window, NULL, &y), 1);

	END;
}

TEST(window_get_position_null_y)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
	};
	u16 x = 0;

	EXPECT_EQ(window_get_position(&window, &x, NULL), 1);

	END;
}

TEST(window_get_position_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	u16 x = 0;
	u16 y = 0;

	window_get_position(&window, &x, &y);

	EXPECT_EQ(t_window_get_position_calls, 1);

	END;
}

TEST(window_get_position_sets_x)
{
	START;

	t_window_reset();
	t_window_x	  = 11;
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	u16 x = 0;
	u16 y = 0;

	window_get_position(&window, &x, &y);

	EXPECT_EQ(x, 11);

	END;
}

TEST(window_get_position_sets_y)
{
	START;

	t_window_reset();
	t_window_y	  = 22;
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	u16 x = 0;
	u16 y = 0;

	window_get_position(&window, &x, &y);

	EXPECT_EQ(y, 22);

	END;
}

TEST(window_get_position_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_get_position_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	u16 x = 0;
	u16 y = 0;

	EXPECT_EQ(window_get_position(&window, &x, &y), 1);

	END;
}

TEST(window_set_size_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_size(&window, 333, 444), 0);
	EXPECT_EQ(t_window_set_size_calls, 1);
	EXPECT_EQ(t_window_width, 333);
	EXPECT_EQ(t_window_height, 444);

	END;
}

TEST(window_set_size_null_window)
{
	START;

	EXPECT_EQ(window_set_size(NULL, 333, 444), 1);

	END;
}

TEST(window_set_size_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_set_size_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_size(&window, 333, 444), 1);

	END;
}

TEST(window_get_size_null_window)
{
	START;

	u16 width  = 0;
	u16 height = 0;

	EXPECT_EQ(window_get_size(NULL, &width, &height), 1);

	END;
}

TEST(window_get_size_null_width)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
	};
	u16 height = 0;

	EXPECT_EQ(window_get_size(&window, NULL, &height), 1);

	END;
}

TEST(window_get_size_null_height)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
	};
	u16 width = 0;

	EXPECT_EQ(window_get_size(&window, &width, NULL), 1);

	END;
}

TEST(window_get_size_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	u16 width  = 0;
	u16 height = 0;

	window_get_size(&window, &width, &height);

	EXPECT_EQ(t_window_get_size_calls, 1);

	END;
}

TEST(window_get_size_sets_width)
{
	START;

	t_window_reset();
	t_window_width	  = 333;
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	u16 width  = 0;
	u16 height = 0;

	window_get_size(&window, &width, &height);

	EXPECT_EQ(width, 333);

	END;
}

TEST(window_get_size_sets_height)
{
	START;

	t_window_reset();
	t_window_height	  = 444;
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	u16 width  = 0;
	u16 height = 0;

	window_get_size(&window, &width, &height);

	EXPECT_EQ(height, 444);

	END;
}

TEST(window_get_size_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_get_size_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	u16 width  = 0;
	u16 height = 0;

	EXPECT_EQ(window_get_size(&window, &width, &height), 1);

	END;
}

TEST(window_set_borderless_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_borderless(&window, 1), 0);
	EXPECT_EQ(t_window_set_borderless_calls, 1);
	EXPECT_EQ(t_window_borderless, 1);

	END;
}

TEST(window_set_borderless_null_window)
{
	START;

	EXPECT_EQ(window_set_borderless(NULL, 1), 1);

	END;
}

TEST(window_set_borderless_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_set_borderless_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_borderless(&window, 1), 1);

	END;
}

TEST(window_get_borderless_null_window)
{
	START;

	int borderless = 0;

	EXPECT_EQ(window_get_borderless(NULL, &borderless), 1);

	END;
}

TEST(window_get_borderless_null_borderless)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
	};

	EXPECT_EQ(window_get_borderless(&window, NULL), 1);

	END;
}

TEST(window_get_borderless_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	int borderless = 0;

	window_get_borderless(&window, &borderless);

	EXPECT_EQ(t_window_get_borderless_calls, 1);

	END;
}

TEST(window_get_borderless_sets_borderless)
{
	START;

	t_window_reset();
	t_window_borderless = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	int borderless = 0;

	window_get_borderless(&window, &borderless);

	EXPECT_EQ(borderless, 1);

	END;
}

TEST(window_get_borderless_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_get_borderless_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	int borderless = 0;

	EXPECT_EQ(window_get_borderless(&window, &borderless), 1);

	END;
}

TEST(window_set_fullscreen_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_fullscreen(&window, 1), 0);
	EXPECT_EQ(t_window_set_fullscreen_calls, 1);
	EXPECT_EQ(t_window_fullscreen, 1);

	END;
}

TEST(window_set_fullscreen_null_window)
{
	START;

	EXPECT_EQ(window_set_fullscreen(NULL, 1), 1);

	END;
}

TEST(window_set_fullscreen_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_set_fullscreen_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_fullscreen(&window, 1), 1);

	END;
}

TEST(window_get_fullscreen_null_window)
{
	START;

	int fullscreen = 0;

	EXPECT_EQ(window_get_fullscreen(NULL, &fullscreen), 1);

	END;
}

TEST(window_get_fullscreen_null_fullscreen)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
	};

	EXPECT_EQ(window_get_fullscreen(&window, NULL), 1);

	END;
}

TEST(window_get_fullscreen_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	int fullscreen = 0;

	window_get_fullscreen(&window, &fullscreen);

	EXPECT_EQ(t_window_get_fullscreen_calls, 1);

	END;
}

TEST(window_get_fullscreen_sets_fullscreen)
{
	START;

	t_window_reset();
	t_window_fullscreen = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	int fullscreen = 0;

	window_get_fullscreen(&window, &fullscreen);

	EXPECT_EQ(fullscreen, 1);

	END;
}

TEST(window_get_fullscreen_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_get_fullscreen_ret = 1;

	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};
	int fullscreen = 0;

	EXPECT_EQ(window_get_fullscreen(&window, &fullscreen), 1);

	END;
}

TEST(window_show_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_show(&window), 0);
	EXPECT_EQ(t_window_show_calls, 1);

	END;
}

TEST(window_show_null_window)
{
	START;

	EXPECT_EQ(window_show(NULL), 1);

	END;
}

TEST(window_show_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_show_ret = 1;
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_show(&window), 1);

	END;
}

TEST(window_hide_calls_driver)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_hide(&window), 0);
	EXPECT_EQ(t_window_hide_calls, 1);

	END;
}

TEST(window_hide_null_window)
{
	START;

	EXPECT_EQ(window_hide(NULL), 1);

	END;
}

TEST(window_hide_returns_driver_result)
{
	START;

	t_window_reset();
	t_window_hide_ret = 1;
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_hide(&window), 1);

	END;
}

STEST(window)
{
	SSTART;

	RUN(window_init_null_window);
	RUN(window_init_null_display);
	RUN(window_init_display_without_driver);
	RUN(window_init_null_config);
	RUN(window_init_calls_driver);
	RUN(window_init_passes_config);
	RUN(window_init_sets_fields);
	RUN(window_init_failure_returns_null);
	RUN(window_init_failure_clears_fields);
	RUN(window_free_null);
	RUN(window_free_without_display);
	RUN(window_free_display_without_driver);
	RUN(window_free_calls_driver);
	RUN(window_free_clears_fields);
	RUN(window_id_null_window);
	RUN(window_id_without_display);
	RUN(window_id_display_without_driver);
	RUN(window_id_calls_driver);
	RUN(window_native_null_window);
	RUN(window_native_without_display);
	RUN(window_native_display_without_driver);
	RUN(window_native_null_native);
	RUN(window_native_calls_driver);
	RUN(window_native_returns_driver_result);
	RUN(window_set_title_null_window);
	RUN(window_set_title_calls_driver);
	RUN(window_set_title_returns_driver_result);
	RUN(window_get_title_null_window);
	RUN(window_get_title_null_title);
	RUN(window_get_title_zero_size);
	RUN(window_get_title_calls_driver);
	RUN(window_get_title_sets_title);
	RUN(window_get_title_returns_driver_result);
	RUN(window_set_position_calls_driver);
	RUN(window_set_position_null_window);
	RUN(window_set_position_returns_driver_result);
	RUN(window_get_position_null_window);
	RUN(window_get_position_null_x);
	RUN(window_get_position_null_y);
	RUN(window_get_position_calls_driver);
	RUN(window_get_position_sets_x);
	RUN(window_get_position_sets_y);
	RUN(window_get_position_returns_driver_result);
	RUN(window_set_size_calls_driver);
	RUN(window_set_size_null_window);
	RUN(window_set_size_returns_driver_result);
	RUN(window_get_size_null_window);
	RUN(window_get_size_null_width);
	RUN(window_get_size_null_height);
	RUN(window_get_size_calls_driver);
	RUN(window_get_size_sets_width);
	RUN(window_get_size_sets_height);
	RUN(window_get_size_returns_driver_result);
	RUN(window_set_borderless_calls_driver);
	RUN(window_set_borderless_null_window);
	RUN(window_set_borderless_returns_driver_result);
	RUN(window_get_borderless_null_window);
	RUN(window_get_borderless_null_borderless);
	RUN(window_get_borderless_calls_driver);
	RUN(window_get_borderless_sets_borderless);
	RUN(window_get_borderless_returns_driver_result);
	RUN(window_set_fullscreen_calls_driver);
	RUN(window_set_fullscreen_null_window);
	RUN(window_set_fullscreen_returns_driver_result);
	RUN(window_get_fullscreen_null_window);
	RUN(window_get_fullscreen_null_fullscreen);
	RUN(window_get_fullscreen_calls_driver);
	RUN(window_get_fullscreen_sets_fullscreen);
	RUN(window_get_fullscreen_returns_driver_result);
	RUN(window_show_calls_driver);
	RUN(window_show_null_window);
	RUN(window_show_returns_driver_result);
	RUN(window_hide_calls_driver);
	RUN(window_hide_null_window);
	RUN(window_hide_returns_driver_result);

	SEND;
}
