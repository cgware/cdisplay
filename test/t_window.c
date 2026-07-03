#include "test.h"

#include "display_driver.h"
#include "window.h"

static int t_window_init_calls;
static int t_window_free_calls;
static int t_window_init_ret;
static u16 t_window_x;
static u16 t_window_y;

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

static int t_window_driver_init(window_t *window, u16 x, u16 y)
{
	t_window_init_calls++;
	t_window_x   = x;
	t_window_y   = y;
	window->data = (void *)0x5678;
	return t_window_init_ret;
}

static int t_window_driver_free(window_t *window)
{
	t_window_free_calls++;
	window->data = NULL;
	return 0;
}

static display_driver_t t_window_driver = {
	.name	     = "test",
	.init	     = t_window_display_init,
	.free	     = t_window_display_free,
	.window_init = t_window_driver_init,
	.window_free = t_window_driver_free,
};

static void t_window_reset(void)
{
	t_window_init_calls = 0;
	t_window_free_calls = 0;
	t_window_init_ret   = 0;
	t_window_x	    = 0;
	t_window_y	    = 0;
}

TEST(window_init_null_window)
{
	START;

	display_t display = {
		.drv = &t_window_driver,
	};

	EXPECT_EQ(window_init(NULL, &display, 0, 0), NULL);

	END;
}

TEST(window_init_null_display)
{
	START;

	window_t window = {0};

	EXPECT_EQ(window_init(&window, NULL, 0, 0), NULL);

	END;
}

TEST(window_init_display_without_driver)
{
	START;

	display_t display = {0};
	window_t window   = {0};

	EXPECT_EQ(window_init(&window, &display, 0, 0), NULL);

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

	EXPECT_EQ(window_init(&window, &display, 11, 22), &window);
	EXPECT_EQ(t_window_init_calls, 1);

	END;
}

TEST(window_init_passes_position)
{
	START;

	t_window_reset();
	display_t display = {
		.drv = &t_window_driver,
	};
	window_t window = {0};

	window_init(&window, &display, 11, 22);

	EXPECT_EQ(t_window_x, 11);
	EXPECT_EQ(t_window_y, 22);

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

	window_init(&window, &display, 0, 0);

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

	EXPECT_EQ(window_init(&window, &display, 0, 0), NULL);

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

	window_init(&window, &display, 0, 0);

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
	window_t window   = {
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

STEST(window)
{
	SSTART;

	RUN(window_init_null_window);
	RUN(window_init_null_display);
	RUN(window_init_display_without_driver);
	RUN(window_init_calls_driver);
	RUN(window_init_passes_position);
	RUN(window_init_sets_fields);
	RUN(window_init_failure_returns_null);
	RUN(window_init_failure_clears_fields);
	RUN(window_free_null);
	RUN(window_free_without_display);
	RUN(window_free_display_without_driver);
	RUN(window_free_calls_driver);
	RUN(window_free_clears_fields);

	SEND;
}
