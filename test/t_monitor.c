#include "test.h"

#include "display_driver.h"
#include "monitor.h"

static int t_monitors_calls;
static int t_monitors_ret;
static display_monitor_t t_monitor = {
	.id		 = 7,
	.name		 = "test monitor",
	.x		 = -10,
	.y		 = 20,
	.width		 = 1920,
	.height		 = 1080,
	.physical_width	 = 600,
	.physical_height = 340,
	.refresh_rate	 = 60,
	.scale		 = 100,
	.primary	 = 1,
	.native		 = (void *)0x5678,
};
static const char t_monitor_print[] =
	"monitor id=7 name=\"test monitor\" pos=-10,20 size=1920x1080 physical=600x340 refresh=60 scale=100 primary=1 "
	"native=0x5678\n";

static int t_monitors_fn(display_t *display, arr_t *monitors)
{
	(void)display;
	t_monitors_calls++;
	if (arr_resize(monitors, 1)) {
		return 1;
	}
	monitors->cnt		   = 1;
	display_monitor_t *monitor = arr_get(monitors, 0);
	*monitor		   = t_monitor;
	return t_monitors_ret;
}

static display_driver_t t_monitor_driver = {
	.name	  = "test",
	.monitors = t_monitors_fn,
};

static void t_monitor_reset(void)
{
	t_monitors_calls = 0;
	t_monitors_ret	 = 0;
}

TEST(display_monitors_null_display)
{
	START;

	arr_t monitors = {0};

	EXPECT_EQ(display_monitors(NULL, &monitors), 1);

	END;
}

TEST(display_monitors_without_driver)
{
	START;

	display_t display = {0};
	arr_t monitors	  = {0};

	EXPECT_EQ(display_monitors(&display, &monitors), 1);

	END;
}

TEST(display_monitors_null_monitors)
{
	START;

	display_t display = {
		.drv = &t_monitor_driver,
	};

	EXPECT_EQ(display_monitors(&display, NULL), 1);

	END;
}

TEST(display_monitors_rejects_wrong_array_type)
{
	START;

	display_t display = {
		.drv = &t_monitor_driver,
	};
	arr_t monitors;
	EXPECT_NOT_NULL(arr_init(&monitors, 1, sizeof(u32), ALLOC_STD));

	EXPECT_EQ(display_monitors(&display, &monitors), 1);

	arr_free(&monitors);

	END;
}

TEST(display_monitors_calls_driver)
{
	START;

	t_monitor_reset();
	display_t display = {
		.drv = &t_monitor_driver,
	};
	arr_t monitors;
	EXPECT_NOT_NULL(arr_init(&monitors, 1, sizeof(display_monitor_t), ALLOC_STD));

	EXPECT_EQ(display_monitors(&display, &monitors), 0);
	EXPECT_EQ(t_monitors_calls, 1);

	arr_free(&monitors);

	END;
}

TEST(display_monitors_returns_driver_result)
{
	START;

	t_monitor_reset();
	t_monitors_ret	  = 1;
	display_t display = {
		.drv = &t_monitor_driver,
	};
	arr_t monitors;
	EXPECT_NOT_NULL(arr_init(&monitors, 1, sizeof(display_monitor_t), ALLOC_STD));

	EXPECT_EQ(display_monitors(&display, &monitors), 1);

	arr_free(&monitors);

	END;
}

TEST(display_monitors_keeps_driver_data)
{
	START;

	t_monitor_reset();
	display_t display = {
		.drv = &t_monitor_driver,
	};
	arr_t monitors;
	EXPECT_NOT_NULL(arr_init(&monitors, 1, sizeof(display_monitor_t), ALLOC_STD));

	EXPECT_EQ(display_monitors(&display, &monitors), 0);
	EXPECT_EQ(monitors.cnt, 1);

	display_monitor_t *monitor = arr_get(&monitors, 0);
	EXPECT_NOT_NULL(monitor);
	EXPECT_EQ(monitor->id, 7);
	EXPECT_EQ(monitor->x, -10);
	EXPECT_EQ(monitor->y, 20);
	EXPECT_EQ(monitor->width, 1920);
	EXPECT_EQ(monitor->height, 1080);
	EXPECT_EQ(monitor->physical_width, 600);
	EXPECT_EQ(monitor->physical_height, 340);
	EXPECT_EQ(monitor->refresh_rate, 60);
	EXPECT_EQ(monitor->scale, 100);
	EXPECT_EQ(monitor->primary, 1);
	EXPECT_PTR(monitor->native, (void *)0x5678);

	arr_free(&monitors);

	END;
}

TEST(monitor_print_null_monitor)
{
	START;

	EXPECT_EQ(monitor_print(NULL, DST_NONE()), 0);

	END;
}

TEST(monitor_print_returns_written_size)
{
	START;

	char buf[256] = {0};

	EXPECT_EQ(monitor_print(&t_monitor, DST_BUF(buf)), sizeof(t_monitor_print) - 1);

	END;
}

TEST(monitor_print_writes_specs)
{
	START;

	char buf[256] = {0};

	monitor_print(&t_monitor, DST_BUF(buf));
	EXPECT_STR(buf, t_monitor_print);

	END;
}

STEST(monitor)
{
	SSTART;
	RUN(display_monitors_null_display);
	RUN(display_monitors_without_driver);
	RUN(display_monitors_null_monitors);
	RUN(display_monitors_rejects_wrong_array_type);
	RUN(display_monitors_calls_driver);
	RUN(display_monitors_returns_driver_result);
	RUN(display_monitors_keeps_driver_data);
	RUN(monitor_print_null_monitor);
	RUN(monitor_print_returns_written_size);
	RUN(monitor_print_writes_specs);
	SEND;
}
