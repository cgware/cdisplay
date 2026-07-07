#include "test.h"

#include "display_driver.h"
#include "window.h"

static int t_window_init_calls;
static int t_window_free_calls;
static int t_window_set_title_calls;
static int t_window_set_position_calls;
static int t_window_set_size_calls;
static int t_window_set_borderless_calls;
static int t_window_set_fullscreen_calls;
static int t_window_show_calls;
static int t_window_hide_calls;
static int t_window_init_ret;
static int t_window_set_title_ret;
static int t_window_set_position_ret;
static int t_window_set_size_ret;
static int t_window_set_borderless_ret;
static int t_window_set_fullscreen_ret;
static int t_window_show_ret;
static int t_window_hide_ret;
static u32 t_window_id_ret;
static strv_t t_window_title;
static u16 t_window_x;
static u16 t_window_y;
static u16 t_window_width;
static u16 t_window_height;
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

static int t_window_driver_init(window_t *window, u16 x, u16 y, u16 width, u16 height)
{
	t_window_init_calls++;
	t_window_x	= x;
	t_window_y	= y;
	t_window_width	= width;
	t_window_height = height;
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

static int t_window_driver_set_title(window_t *window, strv_t title)
{
	(void)window;
	t_window_set_title_calls++;
	t_window_title = title;
	return t_window_set_title_ret;
}

static int t_window_driver_set_position(window_t *window, u16 x, u16 y)
{
	(void)window;
	t_window_set_position_calls++;
	t_window_x = x;
	t_window_y = y;
	return t_window_set_position_ret;
}

static int t_window_driver_set_size(window_t *window, u16 width, u16 height)
{
	(void)window;
	t_window_set_size_calls++;
	t_window_width	= width;
	t_window_height = height;
	return t_window_set_size_ret;
}

static int t_window_driver_set_borderless(window_t *window, int borderless)
{
	(void)window;
	t_window_set_borderless_calls++;
	t_window_borderless = borderless;
	return t_window_set_borderless_ret;
}

static int t_window_driver_set_fullscreen(window_t *window, int fullscreen)
{
	(void)window;
	t_window_set_fullscreen_calls++;
	t_window_fullscreen = fullscreen;
	return t_window_set_fullscreen_ret;
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
	.window_set_title      = t_window_driver_set_title,
	.window_set_position   = t_window_driver_set_position,
	.window_set_size       = t_window_driver_set_size,
	.window_set_borderless = t_window_driver_set_borderless,
	.window_set_fullscreen = t_window_driver_set_fullscreen,
	.window_show	       = t_window_driver_show,
	.window_hide	       = t_window_driver_hide,
};

static void t_window_reset(void)
{
	t_window_init_calls	      = 0;
	t_window_free_calls	      = 0;
	t_window_set_title_calls      = 0;
	t_window_set_position_calls   = 0;
	t_window_set_size_calls	      = 0;
	t_window_set_borderless_calls = 0;
	t_window_set_fullscreen_calls = 0;
	t_window_show_calls	      = 0;
	t_window_hide_calls	      = 0;
	t_window_init_ret	      = 0;
	t_window_set_title_ret	      = 0;
	t_window_set_position_ret     = 0;
	t_window_set_size_ret	      = 0;
	t_window_set_borderless_ret   = 0;
	t_window_set_fullscreen_ret   = 0;
	t_window_show_ret	      = 0;
	t_window_hide_ret	      = 0;
	t_window_id_ret		      = 0;
	t_window_title		      = (strv_t){0};
	t_window_x		      = 0;
	t_window_y		      = 0;
	t_window_width		      = 0;
	t_window_height		      = 0;
	t_window_borderless	      = 0;
	t_window_fullscreen	      = 0;
}

TEST(window_init_null_window)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};

	EXPECT_EQ(window_init(NULL, &display, 0, 0, 640, 480), NULL);

	END;
}

TEST(window_init_null_display)
{
	START;

	window_t window = {0};

	EXPECT_EQ(window_init(&window, NULL, 0, 0, 640, 480), NULL);

	END;
}

TEST(window_init_display_without_driver)
{
	START;

	display_t display = {0};
	window_t window	  = {0};

	EXPECT_EQ(window_init(&window, &display, 0, 0, 640, 480), NULL);

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

	EXPECT_EQ(window_init(&window, &display, 11, 22, 640, 480), &window);
	EXPECT_EQ(t_window_init_calls, 1);

	END;
}

TEST(window_init_passes_geometry)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {0};

	window_init(&window, &display, 11, 22, 333, 444);

	EXPECT_EQ(t_window_x, 11);
	EXPECT_EQ(t_window_y, 22);
	EXPECT_EQ(t_window_width, 333);
	EXPECT_EQ(t_window_height, 444);

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

	window_init(&window, &display, 0, 0, 640, 480);

	EXPECT_EQ(window.display, &display);
	EXPECT_EQ(window.data, (void *)0x5678);

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

	EXPECT_EQ(window_init(&window, &display, 0, 0, 640, 480), NULL);

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

	window_init(&window, &display, 0, 0, 640, 480);

	EXPECT_EQ(window.display, NULL);
	EXPECT_EQ(window.data, NULL);

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
	window_t window	  = {
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

	EXPECT_EQ(window.display, NULL);
	EXPECT_EQ(window.data, NULL);

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
	window_t window	  = {
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
	display_t display      = {
		     .drv = &t_window_driver,
	     };
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_title(&window, STRV("title")), 1);

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
	display_t display	  = {
			.drv = &t_window_driver,
	};
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_position(&window, 11, 22), 1);

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
	display_t display     = {
		    .drv = &t_window_driver,
	    };
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_size(&window, 333, 444), 1);

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
	display_t display	    = {
			  .drv = &t_window_driver,
	  };
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_borderless(&window, 1), 1);

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
	display_t display	    = {
			  .drv = &t_window_driver,
	  };
	window_t window = {
		.display = &display,
		.data	 = (void *)0x5678,
	};

	EXPECT_EQ(window_set_fullscreen(&window, 1), 1);

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
	RUN(window_init_calls_driver);
	RUN(window_init_passes_geometry);
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
	RUN(window_set_title_null_window);
	RUN(window_set_title_calls_driver);
	RUN(window_set_title_returns_driver_result);
	RUN(window_set_position_calls_driver);
	RUN(window_set_position_null_window);
	RUN(window_set_position_returns_driver_result);
	RUN(window_set_size_calls_driver);
	RUN(window_set_size_null_window);
	RUN(window_set_size_returns_driver_result);
	RUN(window_set_borderless_calls_driver);
	RUN(window_set_borderless_null_window);
	RUN(window_set_borderless_returns_driver_result);
	RUN(window_set_fullscreen_calls_driver);
	RUN(window_set_fullscreen_null_window);
	RUN(window_set_fullscreen_returns_driver_result);
	RUN(window_show_calls_driver);
	RUN(window_show_null_window);
	RUN(window_show_returns_driver_result);
	RUN(window_hide_calls_driver);
	RUN(window_hide_null_window);
	RUN(window_hide_returns_driver_result);

	SEND;
}
