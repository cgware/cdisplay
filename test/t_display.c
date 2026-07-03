#include "test.h"

#include "display.h"
#include "display_driver.h"

static int t_display_init_calls;
static int t_display_free_calls;
static int t_display_init_ret;

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

static int t_display_window_init(window_t *window, u16 x, u16 y)
{
	(void)window;
	(void)x;
	(void)y;
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
	.window_init = t_display_window_init,
	.window_free = t_display_window_free,
};

static void t_display_reset(void)
{
	t_display_init_calls = 0;
	t_display_free_calls = 0;
	t_display_init_ret   = 0;
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

	SEND;
}
