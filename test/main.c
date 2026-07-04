#include "log.h"
#include "mem.h"
#include "test.h"

STEST(display);
STEST(display_none);
STEST(display_x11);
STEST(window);

TEST(cdisplay)
{
	SSTART;
	RUN(display);
	RUN(display_none);
	RUN(display_x11);
	RUN(window);
	SEND;
}

int main()
{
	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_WARN, 1, 1);

	t_init();

	t_run(test_cdisplay, 1);

	int ret = t_finish();

	mem_print(DST_STD());

	if (mem_check()) {
		ret = 1;
	}

	return ret;
}
