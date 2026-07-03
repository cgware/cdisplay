
#include "display_driver.h"
#include "fs.h"
#include "log.h"
#include "mem.h"
#include "proc.h"
#include "sock.h"
#include "window.h"

#include <unistd.h>

int main()
{
	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_WARN, 1, 1);

	fs_t fs = {0};
	fs_init(&fs, 0, 0, ALLOC_STD);

	proc_t proc = {0};
	proc_init(&proc, 0, 0);

	sock_t ss = {0};
	sock_init(&ss, 0, 0, ALLOC_STD);

	display_driver_t *drv;
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != DISPLAY_DRIVER_TYPE) {
			continue;
		}

		drv = i->data;
		if (strv_eq(strv_cstr(drv->name), STRV("X11"))) {
			break;
		}
	}

	display_t display = {0};
	display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD);

	window_t wnd  = {0};
	window_t wnd2 = {0};
	window_init(&wnd, &display, 0, 0);
	window_init(&wnd2, &display, 100, 100);
	sleep(1);

	window_free(&wnd);
	window_free(&wnd2);
	display_free(&display);

	fs_free(&fs);
	proc_free(&proc);
	sock_free(&ss);

	mem_print(DST_STD());

	return 0;
}
