#include "test.h"

#include "display.h"
#include "display_driver.h"

static display_driver_t *t_none_driver(void)
{
	return display_driver_find(STRV("none"));
}
TEST(display_none_driver_is_registered)
{
	START;

	EXPECT_NOT_NULL(t_none_driver());

	END;
}

TEST(display_none_init_null_display)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->init(NULL), 1);

	END;
}
TEST(display_none_free_null_display)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->free(NULL), 1);

	END;
}

TEST(display_none_window_init_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_init(NULL, &(window_config_t){.width = 640, .height = 480}), 1);

	END;
}

TEST(display_none_poll_events_null_display)
{
	START;

	display_driver_t *drv = t_none_driver();

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->poll_events(NULL), 1);

	END;
}

TEST(display_none_poll_events_empty)
{
	START;

	display_driver_t *drv = t_none_driver();
	display_t display     = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->poll_events(&display), 0);

	END;
}

TEST(display_none_wait_events_empty)
{
	START;

	display_driver_t *drv = t_none_driver();
	display_t display     = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->wait_events(&display), 0);

	END;
}

TEST(display_none_window_free_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_free(NULL), 1);

	END;
}

TEST(display_none_window_id_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_id(NULL), 0);

	END;
}

TEST(display_none_window_id)
{
	START;

	display_driver_t *drv = t_none_driver();
	window_t window	      = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_id(&window), 0);

	END;
}

TEST(display_none_window_set_title_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_title(NULL, STRV("title")), 1);

	END;
}

TEST(display_none_window_set_title)
{
	START;

	display_driver_t *drv = t_none_driver();
	window_t window	      = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_title(&window, STRV("title")), 0);

	END;
}

TEST(display_none_window_set_position_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_position(NULL, 11, 22), 1);

	END;
}

TEST(display_none_window_set_position)
{
	START;

	display_driver_t *drv = t_none_driver();
	window_t window	      = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_position(&window, 11, 22), 0);

	END;
}

TEST(display_none_window_set_size_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_size(NULL, 333, 444), 1);

	END;
}

TEST(display_none_window_set_size)
{
	START;

	display_driver_t *drv = t_none_driver();
	window_t window	      = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_size(&window, 333, 444), 0);

	END;
}

TEST(display_none_window_set_borderless_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_borderless(NULL, 1), 1);

	END;
}

TEST(display_none_window_set_borderless)
{
	START;

	display_driver_t *drv = t_none_driver();
	window_t window	      = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_borderless(&window, 1), 0);

	END;
}

TEST(display_none_window_set_fullscreen_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_fullscreen(NULL, 1), 1);

	END;
}

TEST(display_none_window_set_fullscreen)
{
	START;

	display_driver_t *drv = t_none_driver();
	window_t window	      = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_set_fullscreen(&window, 1), 0);

	END;
}

TEST(display_none_window_show_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_show(NULL), 1);

	END;
}

TEST(display_none_window_show)
{
	START;

	display_driver_t *drv = t_none_driver();
	window_t window	      = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_show(&window), 0);

	END;
}

TEST(display_none_window_hide_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_hide(NULL), 1);

	END;
}

TEST(display_none_window_hide)
{
	START;

	display_driver_t *drv = t_none_driver();
	window_t window	      = {0};

	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->window_hide(&window), 0);

	END;
}

TEST(display_none_init_success)
{
	START;

	display_driver_t *drv = t_none_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};

	EXPECT_PTR(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), &display);

	display_free(&display);

	END;
}

STEST(display_none_window_init_success)
{
	START;

	display_driver_t *drv = t_none_driver();
	fs_t fs		      = {0};
	proc_t proc	      = {0};
	sock_t ss	      = {0};
	display_t display     = {0};
	window_t window	      = {0};

	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);
	EXPECT_EQ(drv->window_init(&window, &(window_config_t){.width = 640, .height = 480}), 0);

	EXPECT_EQ(drv->window_free(&window), 0);
	display_free(&display);

	END;
}
STEST(display_none)
{
	SSTART;

	RUN(display_none_driver_is_registered);
	RUN(display_none_init_null_display);
	RUN(display_none_free_null_display);
	RUN(display_none_window_init_null_window);
	RUN(display_none_poll_events_null_display);
	RUN(display_none_poll_events_empty);
	RUN(display_none_wait_events_empty);
	RUN(display_none_window_free_null_window);
	RUN(display_none_window_id_null_window);
	RUN(display_none_window_id);
	RUN(display_none_window_set_title_null_window);
	RUN(display_none_window_set_title);
	RUN(display_none_window_set_position_null_window);
	RUN(display_none_window_set_position);
	RUN(display_none_window_set_size_null_window);
	RUN(display_none_window_set_size);
	RUN(display_none_window_set_borderless_null_window);
	RUN(display_none_window_set_borderless);
	RUN(display_none_window_set_fullscreen_null_window);
	RUN(display_none_window_set_fullscreen);
	RUN(display_none_window_show_null_window);
	RUN(display_none_window_show);
	RUN(display_none_window_hide_null_window);
	RUN(display_none_window_hide);
	RUN(display_none_init_success);
	RUN(display_none_window_init_success);

	SEND;
}
