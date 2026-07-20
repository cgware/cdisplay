#include "test.h"

#include "display_driver.h"
#include "fs.h"
#include "mem.h"
#include "proc.h"
#include "sock.h"
#include "window.h"

#include <windows.h>

typedef struct t_windows_state_s {
	WNDPROC wndproc;
	HWND hwnd;
	LONG_PTR userdata;
	LONG_PTR style;
	RECT rect;
	RECT client;
	RECT monitor;
	MSG messages[16];
	size_t message_count;
	size_t message_index;
	int register_class_calls;
	int unregister_class_calls;
	int create_window_calls;
	int destroy_window_calls;
	int show_window_calls;
	int update_window_calls;
	int set_window_text_calls;
	int get_window_text_length_calls;
	int get_window_text_calls;
	int set_window_pos_calls;
	int adjust_window_rect_calls;
	int get_window_rect_calls;
	int get_client_rect_calls;
	int monitor_from_window_calls;
	int get_monitor_info_calls;
	int screen_to_client_calls;
	int peek_message_calls;
	int get_message_calls;
	int translate_message_calls;
	int dispatch_message_calls;
	int def_window_proc_calls;
	DWORD create_style;
	DWORD create_ex_style;
	int create_x;
	int create_y;
	int create_width;
	int create_height;
	const char *create_title;
	char title[64];
	int pos_x;
	int pos_y;
	int pos_width;
	int pos_height;
	UINT pos_flags;
} t_windows_state_t;

static t_windows_state_t t_windows;
static int t_windows_event_calls;
static display_event_t t_windows_event;

static void t_windows_reset(void)
{
	t_windows = (t_windows_state_t){
		.hwnd	 = (HWND)0x1234,
		.style	 = WS_OVERLAPPEDWINDOW,
		.rect	 = {11, 22, 333, 444},
		.client	 = {0, 0, 640, 480},
		.monitor = {0, 0, 1920, 1080},
	};
	t_windows_event_calls = 0;
	t_windows_event	      = (display_event_t){0};
}

static void t_windows_event_cb(display_t *display, const display_event_t *event, void *user)
{
	(void)display;
	(void)user;
	t_windows_event_calls++;
	t_windows_event = *event;
}

static display_driver_t *t_windows_driver(void)
{
	return display_driver_find(STRV("windows"));
}

static ATOM WINAPI t_RegisterClassExA(const WNDCLASSEXA *cls)
{
	t_windows.register_class_calls++;
	t_windows.wndproc = cls->lpfnWndProc;
	return 1;
}

static BOOL WINAPI t_UnregisterClassA(LPCSTR name, HINSTANCE instance)
{
	(void)name;
	(void)instance;
	t_windows.unregister_class_calls++;
	return TRUE;
}

static HWND WINAPI t_CreateWindowExA(DWORD ex_style, LPCSTR class_name, LPCSTR title, DWORD style, int x, int y, int width, int height,
				     HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
	(void)class_name;
	(void)parent;
	(void)menu;
	(void)instance;

	t_windows.create_window_calls++;
	t_windows.create_ex_style = ex_style;
	t_windows.create_style	  = style;
	t_windows.create_x	  = x;
	t_windows.create_y	  = y;
	t_windows.create_width	  = width;
	t_windows.create_height	  = height;
	t_windows.create_title	  = title;
	t_windows.style		  = style;

	if (t_windows.wndproc != NULL) {
		CREATESTRUCTA create = {
			.lpCreateParams = param,
		};
		t_windows.wndproc(t_windows.hwnd, WM_NCCREATE, 0, (LPARAM)&create);
	}

	return t_windows.hwnd;
}

static BOOL WINAPI t_DestroyWindow(HWND hwnd)
{
	(void)hwnd;
	t_windows.destroy_window_calls++;
	return TRUE;
}

static BOOL WINAPI t_ShowWindow(HWND hwnd, int cmd)
{
	(void)hwnd;
	(void)cmd;
	t_windows.show_window_calls++;
	return TRUE;
}

static BOOL WINAPI t_UpdateWindow(HWND hwnd)
{
	(void)hwnd;
	t_windows.update_window_calls++;
	return TRUE;
}

static BOOL WINAPI t_SetWindowTextA(HWND hwnd, LPCSTR title)
{
	(void)hwnd;
	t_windows.set_window_text_calls++;
	c_sprintf(t_windows.title, sizeof(t_windows.title), 0, "%s", title);
	return TRUE;
}

static int WINAPI t_GetWindowTextLengthA(HWND hwnd)
{
	(void)hwnd;
	t_windows.get_window_text_length_calls++;

	int len = 0;
	while (t_windows.title[len] != 0) {
		len++;
	}
	return len;
}

static int WINAPI t_GetWindowTextA(HWND hwnd, LPSTR title, int size)
{
	(void)hwnd;
	t_windows.get_window_text_calls++;

	int i = 0;
	if (size <= 0) {
		return 0;
	}
	while (i + 1 < size && t_windows.title[i] != 0) {
		title[i] = t_windows.title[i];
		i++;
	}
	title[i] = 0;
	return i;
}

static BOOL WINAPI t_SetWindowPos(HWND hwnd, HWND after, int x, int y, int width, int height, UINT flags)
{
	(void)hwnd;
	(void)after;
	t_windows.set_window_pos_calls++;
	t_windows.pos_x	     = x;
	t_windows.pos_y	     = y;
	t_windows.pos_width  = width;
	t_windows.pos_height = height;
	t_windows.pos_flags  = flags;
	return TRUE;
}

static BOOL WINAPI t_AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL menu, DWORD ex_style)
{
	(void)style;
	(void)menu;
	(void)ex_style;
	t_windows.adjust_window_rect_calls++;
	rect->right += 16;
	rect->bottom += 39;
	return TRUE;
}

static BOOL WINAPI t_GetWindowRect(HWND hwnd, LPRECT rect)
{
	(void)hwnd;
	t_windows.get_window_rect_calls++;
	*rect = t_windows.rect;
	return TRUE;
}

static BOOL WINAPI t_GetClientRect(HWND hwnd, LPRECT rect)
{
	(void)hwnd;
	t_windows.get_client_rect_calls++;
	*rect = t_windows.client;
	return TRUE;
}

static HMONITOR WINAPI t_MonitorFromWindow(HWND hwnd, DWORD flags)
{
	(void)hwnd;
	(void)flags;
	t_windows.monitor_from_window_calls++;
	return (HMONITOR)0x5678;
}

static BOOL WINAPI t_GetMonitorInfoA(HMONITOR monitor, LPMONITORINFO info)
{
	(void)monitor;
	t_windows.get_monitor_info_calls++;
	info->rcMonitor = t_windows.monitor;
	return TRUE;
}

static BOOL WINAPI t_ScreenToClient(HWND hwnd, LPPOINT point)
{
	(void)hwnd;
	t_windows.screen_to_client_calls++;
	point->x -= 10;
	point->y -= 20;
	return TRUE;
}

static BOOL WINAPI t_PeekMessageA(LPMSG msg, HWND hwnd, UINT min, UINT max, UINT remove)
{
	(void)hwnd;
	(void)min;
	(void)max;
	(void)remove;
	t_windows.peek_message_calls++;
	if (t_windows.message_index >= t_windows.message_count) {
		return FALSE;
	}
	*msg = t_windows.messages[t_windows.message_index++];
	return TRUE;
}

static BOOL WINAPI t_GetMessageA(LPMSG msg, HWND hwnd, UINT min, UINT max)
{
	(void)hwnd;
	(void)min;
	(void)max;
	t_windows.get_message_calls++;
	if (t_windows.message_index >= t_windows.message_count) {
		return FALSE;
	}
	*msg = t_windows.messages[t_windows.message_index++];
	return TRUE;
}

static BOOL WINAPI t_TranslateMessage(const MSG *msg)
{
	(void)msg;
	t_windows.translate_message_calls++;
	return TRUE;
}

static LRESULT WINAPI t_DispatchMessageA(const MSG *msg)
{
	t_windows.dispatch_message_calls++;
	if (t_windows.wndproc != NULL) {
		return t_windows.wndproc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
	}
	return 0;
}

static LRESULT WINAPI t_DefWindowProcA(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	(void)lparam;
	t_windows.def_window_proc_calls++;
	if (message == WM_NCLBUTTONDOWN && wparam == HTCLOSE && t_windows.wndproc != NULL) {
		return t_windows.wndproc(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
	}
	return 0;
}

static LONG_PTR WINAPI t_GetWindowLongPtrA(HWND hwnd, int index)
{
	(void)hwnd;
	switch (index) {
	case GWLP_USERDATA:
		return t_windows.userdata;
	case GWL_STYLE:
		return t_windows.style;
	default:
		return 0;
	}
}

static LONG_PTR WINAPI t_SetWindowLongPtrA(HWND hwnd, int index, LONG_PTR value)
{
	(void)hwnd;
	switch (index) {
	case GWLP_USERDATA: {
		LONG_PTR old	   = t_windows.userdata;
		t_windows.userdata = value;
		return old;
	}
	case GWL_STYLE: {
		LONG_PTR old	= t_windows.style;
		t_windows.style = value;
		return old;
	}
	default:
		return 0;
	}
}

static void t_windows_set_symbols(proc_t *proc)
{
	proc_setdlmain(proc, STRV("cdisplay_test.exe"), (void *)0x9876);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("RegisterClassExA"), t_RegisterClassExA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("UnregisterClassA"), t_UnregisterClassA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("CreateWindowExA"), t_CreateWindowExA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("DestroyWindow"), t_DestroyWindow);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("ShowWindow"), t_ShowWindow);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("UpdateWindow"), t_UpdateWindow);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("SetWindowTextA"), t_SetWindowTextA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("GetWindowTextLengthA"), t_GetWindowTextLengthA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("GetWindowTextA"), t_GetWindowTextA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("SetWindowPos"), t_SetWindowPos);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("AdjustWindowRectEx"), t_AdjustWindowRectEx);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("GetWindowRect"), t_GetWindowRect);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("GetClientRect"), t_GetClientRect);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("MonitorFromWindow"), t_MonitorFromWindow);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("GetMonitorInfoA"), t_GetMonitorInfoA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("ScreenToClient"), t_ScreenToClient);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("PeekMessageA"), t_PeekMessageA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("GetMessageA"), t_GetMessageA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("TranslateMessage"), t_TranslateMessage);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("DispatchMessageA"), t_DispatchMessageA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("DefWindowProcA"), t_DefWindowProcA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("GetWindowLongPtrA"), t_GetWindowLongPtrA);
	proc_setdlsym(proc, STRV("user32.dll"), STRV("SetWindowLongPtrA"), t_SetWindowLongPtrA);
}

static void t_windows_env_init(fs_t *fs, proc_t *proc, sock_t *ss)
{
	fs_init(fs, 0, 1, ALLOC_STD);
	proc_init(proc, 256, 1, ALLOC_STD);
	sock_init(ss, 0, 1, ALLOC_STD);
	t_windows_set_symbols(proc);
}

static void t_windows_env_free(fs_t *fs, proc_t *proc, sock_t *ss)
{
	fs_free(fs);
	proc_free(proc);
	sock_free(ss);
}

static int t_windows_open(display_t *display, window_t *window, fs_t *fs, proc_t *proc, sock_t *ss)
{
	display_driver_t *drv = t_windows_driver();
	if (drv == NULL) {
		return 1;
	}
	if (display_init(display, drv, fs, proc, ss, ALLOC_STD) == NULL) {
		return 1;
	}
	display_set_event_callback(display, t_windows_event_cb, NULL);
	if (window != NULL && window_init(window, display, &(window_config_t){.x = 10, .y = 20, .width = 640, .height = 480}) == NULL) {
		display_free(display);
		return 1;
	}
	return 0;
}

static LPARAM t_windows_point(short x, short y)
{
	return ((LPARAM)(u16)y << 16) | (u16)x;
}

static void t_windows_push(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	t_windows.messages[t_windows.message_count++] = (MSG){
		.hwnd	 = hwnd,
		.message = message,
		.wParam	 = wparam,
		.lParam	 = lparam,
	};
}

TEST(display_windows_driver_is_registered)
{
	START;

	EXPECT_NOT_NULL(t_windows_driver());

	END;
}

TEST(display_windows_init_registers_class)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};

	EXPECT_EQ(t_windows_open(&display, NULL, &fs, &proc, &ss), 0);
	EXPECT_EQ(t_windows.register_class_calls, 1);

	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_init_fails_without_symbols)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	fs_init(&fs, 0, 1, ALLOC_STD);
	proc_init(&proc, 256, 1, ALLOC_STD);
	sock_init(&ss, 0, 1, ALLOC_STD);
	display_t display     = {0};
	display_driver_t *drv = t_windows_driver();

	EXPECT_NOT_NULL(drv);
	EXPECT_NULL(display_init(&display, drv, &fs, &proc, &ss, ALLOC_STD));

	fs_free(&fs);
	proc_free(&proc);
	sock_free(&ss);

	END;
}

TEST(display_windows_free_unregisters_class)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};

	EXPECT_EQ(t_windows_open(&display, NULL, &fs, &proc, &ss), 0);
	display_free(&display);
	EXPECT_EQ(t_windows.unregister_class_calls, 1);

	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_init_creates_window)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(t_windows.create_window_calls, 1);
	EXPECT_EQ(t_windows.create_style, WS_OVERLAPPEDWINDOW);
	EXPECT_EQ(t_windows.create_width, 656);
	EXPECT_EQ(t_windows.create_height, 519);
	EXPECT_EQ(window_id(&window), (u32)(uintptr_t)t_windows.hwnd);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_free_destroys_window)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	window_free(&window);
	EXPECT_EQ(t_windows.destroy_window_calls, 1);

	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_set_title_copies_counted_string)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};
	char title[]	  = {'t', 'i', 't', 'l', 'e', 'x'};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_set_title(&window, STRVN(title, 5)), 0);
	EXPECT_EQ(t_windows.set_window_text_calls, 1);
	EXPECT_STR(t_windows.title, "title");

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_get_title_returns_title)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};
	char title[64]	  = {0};

	c_sprintf(t_windows.title, sizeof(t_windows.title), 0, "%s", "title");

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_get_title(&window, title, sizeof(title)), 0);
	EXPECT_EQ(t_windows.get_window_text_length_calls, 1);
	EXPECT_EQ(t_windows.get_window_text_calls, 1);
	EXPECT_STR(title, "title");

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_set_position_moves_window)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_set_position(&window, 33, 44), 0);
	EXPECT_EQ(t_windows.pos_x, 33);
	EXPECT_EQ(t_windows.pos_y, 44);
	EXPECT_EQ(t_windows.pos_flags, SWP_NOZORDER | SWP_NOSIZE);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_get_position_returns_x)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};
	u16 x		  = 0;
	u16 y		  = 0;

	t_windows.rect.left = 33;

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_get_position(&window, &x, &y), 0);
	EXPECT_EQ(x, 33);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_get_position_returns_y)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};
	u16 x		  = 0;
	u16 y		  = 0;

	t_windows.rect.top = 44;

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_get_position(&window, &x, &y), 0);
	EXPECT_EQ(y, 44);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_set_size_resizes_window)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_set_size(&window, 800, 600), 0);
	EXPECT_EQ(t_windows.pos_width, 816);
	EXPECT_EQ(t_windows.pos_height, 639);
	EXPECT_EQ(t_windows.pos_flags, SWP_NOZORDER | SWP_NOMOVE);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_get_size_returns_width)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};
	u16 width	  = 0;
	u16 height	  = 0;

	t_windows.client.right = 800;

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_get_size(&window, &width, &height), 0);
	EXPECT_EQ(width, 800);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_get_size_returns_height)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};
	u16 width	  = 0;
	u16 height	  = 0;

	t_windows.client.bottom = 600;

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_get_size(&window, &width, &height), 0);
	EXPECT_EQ(height, 600);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_set_borderless_sets_popup_style)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_set_borderless(&window, 1), 0);
	EXPECT_EQ(t_windows.style, WS_POPUP);
	EXPECT_EQ(t_windows.pos_flags, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_get_borderless_returns_borderless)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};
	int borderless	  = 0;

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows.style = WS_POPUP;
	EXPECT_EQ(window_get_borderless(&window, &borderless), 0);
	EXPECT_EQ(borderless, 1);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_set_fullscreen_uses_monitor)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_set_fullscreen(&window, 1), 0);
	EXPECT_EQ(t_windows.get_window_rect_calls, 1);
	EXPECT_EQ(t_windows.monitor_from_window_calls, 1);
	EXPECT_EQ(t_windows.get_monitor_info_calls, 1);
	EXPECT_EQ(t_windows.style, WS_POPUP);
	EXPECT_EQ(t_windows.pos_width, 1920);
	EXPECT_EQ(t_windows.pos_height, 1080);
	EXPECT_EQ(t_windows.pos_flags, SWP_SHOWWINDOW | SWP_FRAMECHANGED);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_get_fullscreen_returns_fullscreen)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};
	int fullscreen	  = 0;

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_set_fullscreen(&window, 1), 0);
	EXPECT_EQ(window_get_fullscreen(&window, &fullscreen), 0);
	EXPECT_EQ(fullscreen, 1);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_window_show_calls_user32)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display = {0};
	window_t window	  = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	EXPECT_EQ(window_show(&window), 0);
	EXPECT_EQ(t_windows.show_window_calls, 1);
	EXPECT_EQ(t_windows.update_window_calls, 1);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_poll_event_returns_key)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows_push(t_windows.hwnd, WM_KEYDOWN, VK_F11, 0);

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_KEY_DOWN);
	EXPECT_EQ(t_windows_event.key, DISPLAY_KEY_F11);
	EXPECT_EQ(t_windows.dispatch_message_calls, 1);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_poll_event_returns_mouse_move)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows_push(t_windows.hwnd, WM_MOUSEMOVE, MK_LBUTTON, t_windows_point(7, 8));

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_MOUSE_MOVE);
	EXPECT_EQ(t_windows_event.x, 7);
	EXPECT_EQ(t_windows_event.y, 8);
	EXPECT_EQ(t_windows_event.modifiers, DISPLAY_MOD_MOUSE_LEFT);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_poll_event_returns_mouse_button)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows_push(t_windows.hwnd, WM_RBUTTONDOWN, MK_RBUTTON, t_windows_point(9, 10));

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_windows_event.button, DISPLAY_MOUSE_RIGHT);
	EXPECT_EQ(t_windows_event.modifiers, DISPLAY_MOD_MOUSE_RIGHT);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_poll_event_returns_mouse_wheel)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows_push(t_windows.hwnd, WM_MOUSEWHEEL, ((WPARAM)WHEEL_DELTA << 16), t_windows_point(30, 50));

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_windows_event.button, DISPLAY_MOUSE_WHEEL_UP);
	EXPECT_EQ(t_windows_event.x, 20);
	EXPECT_EQ(t_windows_event.y, 30);
	EXPECT_EQ(t_windows.screen_to_client_calls, 1);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_poll_event_returns_nonclient_mouse_move)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows_push(t_windows.hwnd, WM_NCMOUSEMOVE, HTCLOSE, t_windows_point(30, 50));

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_MOUSE_MOVE);
	EXPECT_EQ(t_windows_event.x, 20);
	EXPECT_EQ(t_windows_event.y, 30);
	EXPECT_EQ(t_windows.screen_to_client_calls, 1);
	EXPECT_EQ(t_windows.dispatch_message_calls, 1);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_poll_event_returns_nonclient_mouse_button)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows_push(t_windows.hwnd, WM_NCLBUTTONDOWN, HTCAPTION, t_windows_point(30, 50));

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_MOUSE_DOWN);
	EXPECT_EQ(t_windows_event.button, DISPLAY_MOUSE_LEFT);
	EXPECT_EQ(t_windows_event.x, 20);
	EXPECT_EQ(t_windows_event.y, 30);
	EXPECT_EQ(t_windows_event.modifiers, DISPLAY_MOD_MOUSE_LEFT);
	EXPECT_EQ(t_windows.dispatch_message_calls, 1);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_poll_event_returns_close_from_nonclient_close_button)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	int def_window_proc_calls = t_windows.def_window_proc_calls;
	t_windows_push(t_windows.hwnd, WM_NCLBUTTONDOWN, HTCLOSE, t_windows_point(30, 50));

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_CLOSE);
	EXPECT_EQ(t_windows_event.window, (u32)(uintptr_t)t_windows.hwnd);
	EXPECT_EQ(t_windows.dispatch_message_calls, 1);
	EXPECT_EQ(t_windows.def_window_proc_calls, def_window_proc_calls + 1);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_poll_event_skips_silent_system_messages)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows_push(t_windows.hwnd, 0x0060, 6, 0);
	t_windows_push(t_windows.hwnd, WM_TIMER, 1, 0);
	t_windows_push(t_windows.hwnd, WM_CLOSE, 0, 0);

	EXPECT_EQ(display_poll_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_CLOSE);
	EXPECT_EQ(t_windows.dispatch_message_calls, 2);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_wait_event_returns_close_without_dispatch)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows_push(t_windows.hwnd, WM_CLOSE, 0, 0);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_CLOSE);
	EXPECT_EQ(t_windows.dispatch_message_calls, 0);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

TEST(display_windows_wait_event_returns_syscommand_close_without_dispatch)
{
	START;

	t_windows_reset();
	fs_t fs	    = {0};
	proc_t proc = {0};
	sock_t ss   = {0};
	t_windows_env_init(&fs, &proc, &ss);
	display_t display     = {0};
	window_t window	      = {0};
	display_event_t event = {0};

	EXPECT_EQ(t_windows_open(&display, &window, &fs, &proc, &ss), 0);
	t_windows_push(t_windows.hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);

	EXPECT_EQ(display_wait_events(&display), 0);
	EXPECT_EQ(t_windows_event.type, DISPLAY_EVENT_CLOSE);
	EXPECT_EQ(t_windows_event.window, (u32)(uintptr_t)t_windows.hwnd);
	EXPECT_EQ(t_windows.dispatch_message_calls, 0);

	window_free(&window);
	display_free(&display);
	t_windows_env_free(&fs, &proc, &ss);

	END;
}

STEST(display_windows)
{
	SSTART;

	RUN(display_windows_driver_is_registered);
	RUN(display_windows_init_registers_class);
	RUN(display_windows_init_fails_without_symbols);
	RUN(display_windows_free_unregisters_class);
	RUN(display_windows_window_init_creates_window);
	RUN(display_windows_window_free_destroys_window);
	RUN(display_windows_window_set_title_copies_counted_string);
	RUN(display_windows_window_get_title_returns_title);
	RUN(display_windows_window_set_position_moves_window);
	RUN(display_windows_window_get_position_returns_x);
	RUN(display_windows_window_get_position_returns_y);
	RUN(display_windows_window_set_size_resizes_window);
	RUN(display_windows_window_get_size_returns_width);
	RUN(display_windows_window_get_size_returns_height);
	RUN(display_windows_window_set_borderless_sets_popup_style);
	RUN(display_windows_window_get_borderless_returns_borderless);
	RUN(display_windows_window_set_fullscreen_uses_monitor);
	RUN(display_windows_window_get_fullscreen_returns_fullscreen);
	RUN(display_windows_window_show_calls_user32);
	RUN(display_windows_poll_event_returns_key);
	RUN(display_windows_poll_event_returns_mouse_move);
	RUN(display_windows_poll_event_returns_mouse_button);
	RUN(display_windows_poll_event_returns_mouse_wheel);
	RUN(display_windows_poll_event_returns_nonclient_mouse_move);
	RUN(display_windows_poll_event_returns_nonclient_mouse_button);
	RUN(display_windows_poll_event_returns_close_from_nonclient_close_button);
	RUN(display_windows_poll_event_skips_silent_system_messages);
	RUN(display_windows_wait_event_returns_close_without_dispatch);
	RUN(display_windows_wait_event_returns_syscommand_close_without_dispatch);

	SEND;
}
