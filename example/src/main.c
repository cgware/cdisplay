#include "display_driver.h"
#include "fs.h"
#include "log.h"
#include "mem.h"
#include "print.h"
#include "proc.h"
#include "sock.h"
#include "window.h"

typedef struct example_window_s {
	window_t wnd;
	u32 id;
	int open;
} example_window_t;

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

static const char *event_name(display_event_type_t type)
{
	switch (type) {
	case DISPLAY_EVENT_NONE:
		return "none";
	case DISPLAY_EVENT_CLOSE:
		return "close";
	case DISPLAY_EVENT_RESIZE:
		return "resize";
	case DISPLAY_EVENT_KEY_DOWN:
		return "key down";
	case DISPLAY_EVENT_KEY_UP:
		return "key up";
	case DISPLAY_EVENT_MOUSE_MOVE:
		return "mouse move";
	case DISPLAY_EVENT_MOUSE_DOWN:
		return "mouse down";
	case DISPLAY_EVENT_MOUSE_UP:
		return "mouse up";
	case DISPLAY_EVENT_FOCUS_GAINED:
		return "focus gained";
	case DISPLAY_EVENT_FOCUS_LOST:
		return "focus lost";
	default:
		return "unknown";
	}
}

static void print_event(const display_event_t *event)
{
	c_printf("event=%s window=%u", event_name(event->type), event->window);

	switch (event->type) {
	case DISPLAY_EVENT_RESIZE: {
		c_printf(" pos=%u,%u size=%ux%u", event->x, event->y, event->width, event->height);
		break;
	}
	case DISPLAY_EVENT_KEY_DOWN:
	case DISPLAY_EVENT_KEY_UP: {
		c_printf(" key=%u pos=%u,%u mods=%u", event->key, event->x, event->y, event->modifiers);
		break;
	}
	case DISPLAY_EVENT_MOUSE_MOVE: {
		c_printf(" pos=%u,%u mods=%u", event->x, event->y, event->modifiers);
		break;
	}
	case DISPLAY_EVENT_MOUSE_DOWN:
	case DISPLAY_EVENT_MOUSE_UP: {
		c_printf(" button=%u pos=%u,%u mods=%u", event->button, event->x, event->y, event->modifiers);
		break;
	}
	default: {
		break;
	}
	}

	c_printf("\n");
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
	example_window_t windows[2] = {0};

	display_driver_t *drv = find_display_driver(STRV("X11"));
	if (drv == NULL) {
		c_printf("X11 display driver not found\n");
		cleanup(&display, windows, 2, &fs, &proc, &ss);
		mem_print(DST_STD());
		return 1;
	}

	if (display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD) == NULL) {
		c_printf("failed to initialize display\n");
		cleanup(&display, windows, 2, &fs, &proc, &ss);
		mem_print(DST_STD());
		return 1;
	}

	if (window_init(&windows[0].wnd, &display, 0, 0) == NULL || window_init(&windows[1].wnd, &display, 100, 100) == NULL) {
		c_printf("failed to create windows\n");
		cleanup(&display, windows, 2, &fs, &proc, &ss);
		mem_print(DST_STD());
		return 1;
	}

	for (size_t i = 0; i < 2; i++) {
		windows[i].id	= window_id(&windows[i].wnd);
		windows[i].open = 1;
		c_printf("window[%zu]=%u\n", i, windows[i].id);
	}

	int open = 2;
	while (open > 0) {
		display_event_t event = {0};
		if (display_wait_event(&display, &event)) {
			break;
		}

		print_event(&event);

		example_window_t *window = find_window(windows, 2, event.window);
		if (window == NULL) {
			continue;
		}

		if (event.type == DISPLAY_EVENT_CLOSE && window->open) {
			window->open = 0;
			open--;
			window_free(&window->wnd);
			continue;
		}

		if (event.type == DISPLAY_EVENT_KEY_DOWN) {
			break;
		}
	}

	cleanup(&display, windows, 2, &fs, &proc, &ss);

	mem_print(DST_STD());

	return 0;
}
