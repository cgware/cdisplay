#include "display_driver.h"
#include "fs.h"
#include "log.h"
#include "mem.h"
#include "platform.h"
#include "print.h"
#include "proc.h"
#include "sock.h"
#include "window.h"

typedef struct example_window_s {
	window_t wnd;
	u32 id;
	int open;
	int fullscreen;
} example_window_t;

typedef struct example_state_s {
	example_window_t *windows;
	size_t count;
	int open;
} example_state_t;

static display_driver_t *find_display_driver(strv_t name)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != DISPLAY_DRIVER_TYPE) {
			continue;
		}

		display_driver_t *drv = i->data;
		if (strv_eq(strv_cstr(drv->name), name)) {
			return drv;
		}
	}

	return NULL;
}

static example_window_t *find_window(example_window_t *windows, size_t count, u32 id)
{
	for (size_t i = 0; i < count; i++) {
		if (windows[i].id == id) {
			return &windows[i];
		}
	}

	return NULL;
}

static void close_window(example_window_t *window, int *open)
{
	if (window == NULL || !window->open) {
		return;
	}

	window->open = 0;
	(*open)--;
	window_free(&window->wnd);
}

static void toggle_fullscreen(example_window_t *window)
{
	if (window == NULL || !window->open) {
		return;
	}

	int fullscreen = !window->fullscreen;
	if (window_set_fullscreen(&window->wnd, fullscreen)) {
		c_printf("failed to set fullscreen for window %u\n", window->id);
		return;
	}

	window->fullscreen = fullscreen;
}

static void on_display_event(display_t *display, const display_event_t *event, void *user)
{
	(void)display;

	example_state_t *state = user;
	if (state == NULL || event == NULL) {
		return;
	}

	display_event_log(event);

	example_window_t *window = find_window(state->windows, state->count, event->window);
	if (window == NULL) {
		return;
	}

	switch (event->type) {
	case DISPLAY_EVENT_CLOSE: {
		close_window(window, &state->open);
		return;
	}
	case DISPLAY_EVENT_KEY_DOWN: {
		switch (event->key) {
		case DISPLAY_KEY_ESCAPE: {
			close_window(window, &state->open);
			return;
		}
		case DISPLAY_KEY_F11: {
			toggle_fullscreen(window);
			break;
		}
		default:
			break;
		}
		break;
	}
	default:
		break;
	}
}

static void cleanup(display_t *display, example_window_t *windows, size_t count, fs_t *fs, proc_t *proc, sock_t *ss)
{
	for (size_t i = 0; i < count; i++) {
		if (windows[i].wnd.display) {
			window_free(&windows[i].wnd);
		}
	}
	if (display->drv) {
		display_free(display);
	}

	fs_free(fs);
	proc_free(proc);
	sock_free(ss);
}

int main()
{
	mem_stats_t mem_stats = {0};
	mem_stats_set(&mem_stats);

	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_INFO, 1, 1);

	fs_t fs = {0};
	fs_init(&fs, 0, 0, ALLOC_STD);

	proc_t proc = {0};
	proc_init(&proc, 0, 0);

	sock_t ss = {0};
	sock_init(&ss, 0, 0, ALLOC_STD);

	display_t display	    = {0};
	size_t windows_cnt	    = 2;
	example_window_t windows[2] = {0};

#if defined(C_WIN)
	strv_t driver_name = STRV("windows");
#else
	strv_t driver_name = STRV("X11");
#endif

	display_driver_t *drv = find_display_driver(driver_name);
	if (drv == NULL) {
		c_printf("X11 display driver not found\n");
		cleanup(&display, windows, windows_cnt, &fs, &proc, &ss);
		mem_print(DST_STD());
		return 1;
	}

	if (display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD) == NULL) {
		c_printf("failed to initialize display\n");
		cleanup(&display, windows, windows_cnt, &fs, &proc, &ss);
		mem_print(DST_STD());
		return 1;
	}

	for (size_t i = 0; i < windows_cnt; i++) {
		if (window_init(&windows[i].wnd, &display, 100 * (u16)i, 100 * (u16)i, 640, 480) == NULL) {
			c_printf("failed to create window\n");
			cleanup(&display, windows, windows_cnt, &fs, &proc, &ss);
			mem_print(DST_STD());
			return 1;
		}
	}

	for (size_t i = 0; i < windows_cnt; i++) {
		if (window_show(&windows[i].wnd)) {
			c_printf("failed to show window\n");
			cleanup(&display, windows, windows_cnt, &fs, &proc, &ss);
			mem_print(DST_STD());
			return 1;
		}
	}

	for (size_t i = 0; i < windows_cnt; i++) {
		windows[i].id	      = window_id(&windows[i].wnd);
		windows[i].open	      = 1;
		windows[i].fullscreen = 0;
		c_printf("window[%zu]=%u\n", i, windows[i].id);
	}

	example_state_t state = {
		.windows = windows,
		.count	 = windows_cnt,
		.open	 = (int)windows_cnt,
	};
	display_set_event_callback(&display, on_display_event, &state);

	while (state.open > 0) {
		if (display_wait_events(&display)) {
			break;
		}
	}

	cleanup(&display, windows, windows_cnt, &fs, &proc, &ss);

	mem_print(DST_STD());

	return 0;
}
