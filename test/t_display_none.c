#include "test.h"

#include "display.h"
#include "display_driver.h"

static display_driver_t *t_none_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != DISPLAY_DRIVER_TYPE) {
			continue;
		}

		display_driver_t *drv = i->data;
		if (strv_eq(strv_cstr(drv->name), STRV("none"))) {
			return drv;
		}
	}

	return NULL;
}
TEST(display_none_driver_is_registered)
{
	START;

	EXPECT_NE(t_none_driver(), NULL);

	END;
}

TEST(display_none_init_null_display)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->init(NULL), 1);

	END;
}
TEST(display_none_free_null_display)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->free(NULL), 1);

	END;
}

TEST(display_none_window_init_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_init(NULL, 0, 0), 1);

	END;
}

TEST(display_none_window_free_null_window)
{
	START;

	display_driver_t *drv = t_none_driver();
	EXPECT_NE(drv, NULL);
	EXPECT_EQ(drv->window_free(NULL), 1);

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

	EXPECT_EQ(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD), &display);

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
	EXPECT_EQ(drv->window_init(&window, 0, 0), 0);

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
	RUN(display_none_window_free_null_window);
	RUN(display_none_init_success);
	RUN(display_none_window_init_success);

	SEND;
}
