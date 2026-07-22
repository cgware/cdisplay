#include "log.h"
#include "mem.h"
#include "test.h"

STEST(display);
STEST(display_ext);
STEST(monitor);
STEST(display_none);
STEST(display_windows);
STEST(display_x11_direct);
STEST(display_x11_dynamic);
STEST(display_wayland_dynamic);
STEST(window);

TEST(cdisplay)
{
	SSTART;
	RUN(display);
	RUN(display_ext);
	RUN(monitor);
	RUN(display_none);
	RUN(display_windows);
	RUN(display_x11_direct);
	RUN(display_x11_dynamic);
	RUN(display_wayland_dynamic);
	RUN(window);
	SEND;
}

int main(int argc, char **argv)
{
	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_WARN, 1, 1);

	if (t_init(argc, argv)) {
		return 0;
	}

	t_run(test_cdisplay, 1);

	int ret = t_finish();

	mem_print(DST_STD());

	if (mem_check()) {
		ret = 1;
	}

	return ret;
}
