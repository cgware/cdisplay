#include "ctime.h"
#include "display_driver.h"
#include "fs.h"
#include "log.h"
#include "mem.h"
#include "monitor.h"
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

enum {
	EXAMPLE_WINDOWS	    = 2,
	EXAMPLE_MAX_DRIVERS = 8,
};

typedef struct example_display_s {
	display_t display;
	example_window_t windows[EXAMPLE_WINDOWS];
	example_state_t state;
	int initialized;
} example_display_t;

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
		log_error("cdisplay_example", "event", NULL, "failed to set fullscreen for window %u", window->id);
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

static void cleanup_display(display_t *display, example_window_t *windows, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		if (windows[i].wnd.display) {
			window_free(&windows[i].wnd);
		}
	}
	if (display->drv) {
		display_free(display);
	}
}

static void cleanup_example_display(example_display_t *example)
{
	if (example == NULL || !example->initialized) {
		return;
	}

	cleanup_display(&example->display, example->windows, EXAMPLE_WINDOWS);
	example->initialized = 0;
}

static int window_position(u16 *position, s32 origin, u32 offset)
{
	s64 value = (s64)origin + offset;
	if (position == NULL || value < U16_MIN || value > U16_MAX) {
		return 1;
	}

	*position = (u16)value;
	return 0;
}

static int print_monitors(display_t *display, const char *driver_name, display_monitor_t *show_monitor, int *has_monitor)
{
	arr_t monitors = {0};
	if (has_monitor != NULL) {
		*has_monitor = 0;
	}
	if (arr_init(&monitors, 1, sizeof(display_monitor_t), ALLOC_STD) == NULL) {
		return 1;
	}
	if (display_monitors(display, &monitors)) {
		arr_free(&monitors);
		return 1;
	}

	dputf(DST_STD(), "%s monitors:\n", driver_name);
	for (u32 i = 0; i < monitors.cnt; i++) {
		monitor_print(arr_get(&monitors, i), DST_STD());
	}
	if (monitors.cnt > 0 && show_monitor != NULL && has_monitor != NULL) {
		display_monitor_t *monitor = arr_get(&monitors, 2 > monitors.cnt ? 0 : 2);
		*show_monitor		   = *monitor;
		*has_monitor		   = 1;
	}

	arr_free(&monitors);
	return 0;
}

static int open_display_driver(example_display_t *example, display_driver_t *drv, fs_t *fs, proc_t *proc, sock_t *ss, u32 index)
{
	if (example == NULL) {
		return -1;
	}

	if (drv == NULL || drv->window_native == NULL) {
		return 0;
	}

	log_info("cdisplay_example", "init", NULL, "display driver: %s", drv->name);

	if (display_init(&example->display, drv, fs, proc, ss, ALLOC_STD) == NULL) {
		log_error("cdisplay_example", "init", NULL, "failed to initialize display driver: %s", drv->name);
		cleanup_example_display(example);
		return -1;
	}
	example->initialized	       = 1;
	display_monitor_t show_monitor = {0};
	int has_monitor		       = 0;
	if (print_monitors(&example->display, drv->name, &show_monitor, &has_monitor)) {
		log_error("cdisplay_example", "init", NULL, "failed to list monitors for display driver: %s", drv->name);
		cleanup_example_display(example);
		return -1;
	}

	for (size_t i = 0; i < EXAMPLE_WINDOWS; i++) {
		u32 offset = 100 + (index * EXAMPLE_WINDOWS + (u32)i) * 40;
		u16 x	   = 0;
		u16 y	   = 0;
		if (window_position(&x, has_monitor ? show_monitor.x : 0, offset) ||
		    window_position(&y, has_monitor ? show_monitor.y : 0, offset)) {
			log_error("cdisplay_example", "init", NULL, "failed to place window for display driver: %s", drv->name);
			cleanup_example_display(example);
			return -1;
		}

		window_config_t config = {
			.x	= x,
			.y	= y,
			.width	= 640,
			.height = 480,
		};
		if (window_init(&example->windows[i].wnd, &example->display, &config) == NULL) {
			log_error("cdisplay_example", "init", NULL, "failed to create window for display driver: %s", drv->name);
			cleanup_example_display(example);
			return -1;
		}
	}

	for (size_t i = 0; i < EXAMPLE_WINDOWS; i++) {
		if (window_show(&example->windows[i].wnd)) {
			log_error("cdisplay_example", "init", NULL, "failed to show window for display driver: %s", drv->name);
			cleanup_example_display(example);
			return -1;
		}
	}

	for (size_t i = 0; i < EXAMPLE_WINDOWS; i++) {
		example->windows[i].id	       = window_id(&example->windows[i].wnd);
		example->windows[i].open       = 1;
		example->windows[i].fullscreen = 0;
		log_info("cdisplay_example", "init", NULL, "%s window[%zu]=%u", drv->name, i, example->windows[i].id);
	}

	example->state = (example_state_t){
		.windows = example->windows,
		.count	 = EXAMPLE_WINDOWS,
		.open	 = EXAMPLE_WINDOWS,
	};
	display_set_event_callback(&example->display, on_display_event, &example->state);

	return 1;
}

static int poll_display_drivers(example_display_t *examples, u32 count)
{
	int open = 0;
	for (u32 i = 0; i < count; i++) {
		if (!examples[i].initialized || examples[i].state.open <= 0) {
			continue;
		}
		open += examples[i].state.open;
		if (display_poll_events(&examples[i].display)) {
			log_error("cdisplay_example",
				  "event",
				  NULL,
				  "failed to poll events from display driver: %s",
				  examples[i].display.drv->name);
			return -1;
		}
	}

	return open;
}

int main(void)
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
	proc_init(&proc, 0, 0, ALLOC_STD);

	sock_t ss = {0};
	sock_init(&ss, 0, 0, ALLOC_STD);

	display_driver_t *drivers[EXAMPLE_MAX_DRIVERS]	= {0};
	example_display_t displays[EXAMPLE_MAX_DRIVERS] = {0};
	u32 driver_count				= display_driver_list(drivers, sizeof(drivers) / sizeof(drivers[0]));
	if (driver_count > sizeof(drivers) / sizeof(drivers[0])) {
		driver_count = sizeof(drivers) / sizeof(drivers[0]);
	}
	if (driver_count == 0) {
		log_error("cdisplay_example", "init", NULL, "no display drivers found");
		fs_free(&fs);
		proc_free(&proc);
		sock_free(&ss);
		mem_print(DST_STD());
		return 1;
	}

	int ret	  = 0;
	u32 count = 0;
	for (u32 i = 0; i < driver_count; i++) {
		if (!display_driver_available(drivers[i], &proc)) {
			continue;
		}
		int opened = open_display_driver(&displays[count], drivers[i], &fs, &proc, &ss, count);
		if (opened < 0) {
			ret = 1;
			break;
		}
		if (opened > 0) {
			count++;
		}
	}
	if (ret == 0 && count == 0) {
		log_error("cdisplay_example", "init", NULL, "no window display drivers found");
		ret = 1;
	}

	while (ret == 0) {
		int open = poll_display_drivers(displays, count);
		if (open < 0) {
			ret = 1;
			break;
		}
		if (open == 0) {
			break;
		}
		c_sleep(10);
	}

	for (u32 i = 0; i < count; i++) {
		cleanup_example_display(&displays[i]);
	}
	fs_free(&fs);
	proc_free(&proc);
	sock_free(&ss);

	mem_print(DST_STD());

	return ret;
}
