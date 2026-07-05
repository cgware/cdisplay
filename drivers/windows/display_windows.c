#include "display_driver.h"

#include "log.h"
#include "mem.h"

#include <windows.h>

typedef ATOM(WINAPI *register_class_exa_t)(const WNDCLASSEXA *);
typedef BOOL(WINAPI *unregister_classa_t)(LPCSTR, HINSTANCE);
typedef HWND(WINAPI *create_window_exa_t)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef BOOL(WINAPI *destroy_window_t)(HWND);
typedef BOOL(WINAPI *show_window_t)(HWND, int);
typedef BOOL(WINAPI *update_window_t)(HWND);
typedef BOOL(WINAPI *set_window_texta_t)(HWND, LPCSTR);
typedef BOOL(WINAPI *set_window_pos_t)(HWND, HWND, int, int, int, int, UINT);
typedef BOOL(WINAPI *adjust_window_rect_ex_t)(LPRECT, DWORD, BOOL, DWORD);
typedef BOOL(WINAPI *peek_messagea_t)(LPMSG, HWND, UINT, UINT, UINT);
typedef BOOL(WINAPI *get_messagea_t)(LPMSG, HWND, UINT, UINT);
typedef BOOL(WINAPI *translate_message_t)(const MSG *);
typedef LRESULT(WINAPI *dispatch_messagea_t)(const MSG *);
typedef LRESULT(WINAPI *def_window_proca_t)(HWND, UINT, WPARAM, LPARAM);
typedef LONG_PTR(WINAPI *get_window_long_ptra_t)(HWND, int);
typedef LONG_PTR(WINAPI *set_window_long_ptra_t)(HWND, int, LONG_PTR);

typedef struct display_windows_wndproc_api_s {
	get_window_long_ptra_t GetWindowLongPtrA;
	def_window_proca_t DefWindowProcA;
} display_windows_wndproc_api_t;

#if defined(_WIN64)
	#define DISPLAY_WINDOWS_GET_WINDOW_LONG_PTR "GetWindowLongPtrA"
	#define DISPLAY_WINDOWS_SET_WINDOW_LONG_PTR "SetWindowLongPtrA"
#else
	#define DISPLAY_WINDOWS_GET_WINDOW_LONG_PTR "GetWindowLongA"
	#define DISPLAY_WINDOWS_SET_WINDOW_LONG_PTR "SetWindowLongA"
#endif

typedef struct display_windows_s {
	HMODULE user32;
	HINSTANCE instance;
	ATOM window_class;
	register_class_exa_t RegisterClassExA;
	unregister_classa_t UnregisterClassA;
	create_window_exa_t CreateWindowExA;
	destroy_window_t DestroyWindow;
	show_window_t ShowWindow;
	update_window_t UpdateWindow;
	set_window_texta_t SetWindowTextA;
	set_window_pos_t SetWindowPos;
	adjust_window_rect_ex_t AdjustWindowRectEx;
	peek_messagea_t PeekMessageA;
	get_messagea_t GetMessageA;
	translate_message_t TranslateMessage;
	dispatch_messagea_t DispatchMessageA;
	def_window_proca_t DefWindowProcA;
	get_window_long_ptra_t GetWindowLongPtrA;
	set_window_long_ptra_t SetWindowLongPtrA;
	display_windows_wndproc_api_t wndproc_api;
} display_windows_t;

typedef struct window_windows_s {
	HWND handle;
	int visible;
} window_windows_t;

static display_windows_wndproc_api_t *s_wndproc_api;

static LRESULT CALLBACK display_windows_wndproc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	display_windows_t *dwindows = NULL;

	if (message == WM_NCCREATE) {
		CREATESTRUCTA *create = (CREATESTRUCTA *)lparam;
		window_t *wnd	      = create == NULL ? NULL : (window_t *)create->lpCreateParams;

		if (wnd != NULL && wnd->display != NULL) {
			dwindows = wnd->display->data;
			if (dwindows != NULL && dwindows->SetWindowLongPtrA != NULL) {
				dwindows->SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)wnd);
			}
		}
	} else if (s_wndproc_api != NULL && s_wndproc_api->GetWindowLongPtrA != NULL) {
		window_t *wnd = (window_t *)s_wndproc_api->GetWindowLongPtrA(hwnd, GWLP_USERDATA);
		if (wnd != NULL && wnd->display != NULL) {
			dwindows = wnd->display->data;
		}
	}

	if (message == WM_NCDESTROY && dwindows != NULL && dwindows->SetWindowLongPtrA != NULL) {
		dwindows->SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
	}

	if (dwindows != NULL && dwindows->DefWindowProcA != NULL) {
		return dwindows->DefWindowProcA(hwnd, message, wparam, lparam);
	}
	if (s_wndproc_api != NULL && s_wndproc_api->DefWindowProcA != NULL) {
		return s_wndproc_api->DefWindowProcA(hwnd, message, wparam, lparam);
	}

	return 0;
}

static int display_windows_load_proc(display_windows_t *dwindows, FARPROC *proc, const char *name)
{
	*proc = GetProcAddress(dwindows->user32, name);
	if (*proc == NULL) {
		log_error("cdisplay", "display_windows", NULL, "failed to load user32 symbol: %s", name);
		return 1;
	}

	return 0;
}

static int display_windows_load_user32(display_windows_t *dwindows)
{
	dwindows->user32 = LoadLibraryA("user32.dll");
	if (dwindows->user32 == NULL) {
		log_error("cdisplay", "display_windows", NULL, "failed to load user32.dll");
		return 1;
	}

	if (display_windows_load_proc(dwindows, (FARPROC *)&dwindows->RegisterClassExA, "RegisterClassExA") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->UnregisterClassA, "UnregisterClassA") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->CreateWindowExA, "CreateWindowExA") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->DestroyWindow, "DestroyWindow") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->ShowWindow, "ShowWindow") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->UpdateWindow, "UpdateWindow") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->SetWindowTextA, "SetWindowTextA") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->SetWindowPos, "SetWindowPos") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->AdjustWindowRectEx, "AdjustWindowRectEx") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->PeekMessageA, "PeekMessageA") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->GetMessageA, "GetMessageA") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->TranslateMessage, "TranslateMessage") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->DispatchMessageA, "DispatchMessageA") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->DefWindowProcA, "DefWindowProcA") ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->GetWindowLongPtrA, DISPLAY_WINDOWS_GET_WINDOW_LONG_PTR) ||
	    display_windows_load_proc(dwindows, (FARPROC *)&dwindows->SetWindowLongPtrA, DISPLAY_WINDOWS_SET_WINDOW_LONG_PTR)) {
		FreeLibrary(dwindows->user32);
		dwindows->user32 = NULL;
		return 1;
	}

	dwindows->wndproc_api.GetWindowLongPtrA = dwindows->GetWindowLongPtrA;
	dwindows->wndproc_api.DefWindowProcA	= dwindows->DefWindowProcA;

	return 0;
}

static int display_windows_register_class(display_windows_t *dwindows)
{
	WNDCLASSEXA cls = {0};

	cls.cbSize	  = sizeof(cls);
	cls.style	  = CS_HREDRAW | CS_VREDRAW;
	cls.lpfnWndProc	  = display_windows_wndproc;
	cls.hInstance	  = dwindows->instance;
	cls.lpszClassName = "cdisplay_window";

	dwindows->window_class = dwindows->RegisterClassExA(&cls);
	if (dwindows->window_class == 0) {
		log_error("cdisplay", "display_windows", NULL, "failed to register window class");
		return 1;
	}

	return 0;
}

static int display_windows_translate_message(const MSG *msg, display_event_t *event)
{
	*event = (display_event_t){0};

	switch (msg->message) {
	case WM_CLOSE:
		event->type   = DISPLAY_EVENT_CLOSE;
		event->window = (u32)(uintptr_t)msg->hwnd;
		return 0;
	case WM_SIZE:
		event->type   = DISPLAY_EVENT_RESIZE;
		event->window = (u32)(uintptr_t)msg->hwnd;
		event->width  = LOWORD(msg->lParam);
		event->height = HIWORD(msg->lParam);
		return 0;
	default:
		return 1;
	}
}

static int display_windows_dispatch_message(display_windows_t *dwindows, const MSG *msg)
{
	dwindows->TranslateMessage(msg);
	dwindows->DispatchMessageA(msg);
	return 0;
}

static int display_windows_init(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	display->data = mem_alloc(sizeof(display_windows_t));
	if (display->data == NULL) {
		return 1;
	}
	mem_set(display->data, 0, sizeof(display_windows_t));

	display_windows_t *dwindows = display->data;
	dwindows->instance	    = GetModuleHandleA(NULL);

	if (display_windows_load_user32(dwindows)) {
		if (dwindows->user32 != NULL) {
			FreeLibrary(dwindows->user32);
		}
		mem_free(display->data, sizeof(display_windows_t));
		display->data = NULL;
		return 1;
	}

	s_wndproc_api = &dwindows->wndproc_api;

	if (display_windows_register_class(dwindows)) {
		s_wndproc_api = NULL;
		FreeLibrary(dwindows->user32);
		mem_free(display->data, sizeof(display_windows_t));
		display->data = NULL;
		return 1;
	}

	return 0;
}

static int display_windows_free(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	display_windows_t *dwindows = display->data;
	if (dwindows != NULL) {
		if (dwindows->window_class != 0) {
			dwindows->UnregisterClassA("cdisplay_window", dwindows->instance);
		}
		if (s_wndproc_api == &dwindows->wndproc_api) {
			s_wndproc_api = NULL;
		}
		if (dwindows->user32 != NULL) {
			FreeLibrary(dwindows->user32);
		}
		mem_free(dwindows, sizeof(display_windows_t));
	}

	return 0;
}

static int display_windows_poll_event(display_t *display, display_event_t *event)
{
	if (display == NULL || event == NULL) {
		return 1;
	}

	display_windows_t *dwindows = display->data;
	if (dwindows == NULL) {
		return 1;
	}

	MSG msg;
	while (dwindows->PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (display_windows_translate_message(&msg, event) == 0) {
			return 0;
		}
		display_windows_dispatch_message(dwindows, &msg);
	}

	*event = (display_event_t){0};
	return 1;
}

static int display_windows_wait_event(display_t *display, display_event_t *event)
{
	if (display == NULL || event == NULL) {
		return 1;
	}

	display_windows_t *dwindows = display->data;
	if (dwindows == NULL) {
		return 1;
	}

	MSG msg;
	while (dwindows->GetMessageA(&msg, NULL, 0, 0) > 0) {
		if (display_windows_translate_message(&msg, event) == 0) {
			return 0;
		}
		display_windows_dispatch_message(dwindows, &msg);
	}

	*event = (display_event_t){0};
	return 1;
}

static int display_windows_window_init(window_t *wnd, u16 x, u16 y, u16 width, u16 height)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL) {
		return 1;
	}

	wnd->data = mem_alloc(sizeof(window_windows_t));
	if (wnd->data == NULL) {
		return 1;
	}
	mem_set(wnd->data, 0, sizeof(window_windows_t));

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows = wnd->data;

	DWORD style	= WS_OVERLAPPEDWINDOW;
	DWORD ex_style = 0;
	RECT rect	= {0, 0, width, height};
	if (!dwindows->AdjustWindowRectEx(&rect, style, FALSE, ex_style)) {
		mem_free(wnd->data, sizeof(window_windows_t));
		wnd->data = NULL;
		return 1;
	}

	wwindows->handle = dwindows->CreateWindowExA(ex_style,
						     "cdisplay_window",
						     "cdisplay",
						     style,
						     x,
						     y,
						     rect.right - rect.left,
						     rect.bottom - rect.top,
						     NULL,
						     NULL,
						     dwindows->instance,
						     wnd);
	if (wwindows->handle == NULL) {
		log_error("cdisplay", "display_windows", NULL, "failed to create window");
		mem_free(wnd->data, sizeof(window_windows_t));
		wnd->data = NULL;
		return 1;
	}

	return 0;
}

static int display_windows_window_free(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows = wnd->data;

	if (wwindows->handle != NULL && !dwindows->DestroyWindow(wwindows->handle)) {
		return 1;
	}

	mem_free(wnd->data, sizeof(window_windows_t));
	wnd->data = NULL;

	return 0;
}

static u32 display_windows_window_id(window_t *wnd)
{
	if (wnd == NULL || wnd->data == NULL) {
		return 0;
	}

	window_windows_t *wwindows = wnd->data;
	return (u32)(uintptr_t)wwindows->handle;
}

static int display_windows_window_set_title(window_t *wnd, strv_t title)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL || (title.data == NULL && title.len > 0)) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows = wnd->data;

	char *buf = mem_alloc((size_t)title.len + 1);
	if (buf == NULL) {
		return 1;
	}
	if (title.len > 0) {
		mem_copy(buf, (size_t)title.len + 1, title.data, title.len);
	}
	buf[title.len] = 0;

	int ret = dwindows->SetWindowTextA(wwindows->handle, buf) ? 0 : 1;
	mem_free(buf, (size_t)title.len + 1);

	return ret;
}

static int display_windows_window_set_position(window_t *wnd, u16 x, u16 y)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows = wnd->data;

	return dwindows->SetWindowPos(wwindows->handle, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE) ? 0 : 1;
}

static int display_windows_window_set_size(window_t *wnd, u16 width, u16 height)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows = wnd->data;

	DWORD style	= WS_OVERLAPPEDWINDOW;
	DWORD ex_style = 0;
	RECT rect	= {0, 0, width, height};
	if (!dwindows->AdjustWindowRectEx(&rect, style, FALSE, ex_style)) {
		return 1;
	}

	return dwindows->SetWindowPos(
		       wwindows->handle, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOMOVE)
		       ? 0
		       : 1;
}

static int display_windows_window_set_borderless(window_t *wnd, int borderless)
{
	(void)borderless;

	if (wnd == NULL || wnd->data == NULL) {
		return 1;
	}

	return 0;
}

static int display_windows_window_set_fullscreen(window_t *wnd, int fullscreen)
{
	(void)fullscreen;

	if (wnd == NULL || wnd->data == NULL) {
		return 1;
	}

	return 0;
}

static int display_windows_window_show(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows = wnd->data;

	dwindows->ShowWindow(wwindows->handle, SW_SHOW);
	dwindows->UpdateWindow(wwindows->handle);
	wwindows->visible = 1;

	return 0;
}

static int display_windows_window_hide(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows = wnd->data;

	dwindows->ShowWindow(wwindows->handle, SW_HIDE);
	wwindows->visible = 0;

	return 0;
}

static display_driver_t display_windows = {
	.name		       = "windows",
	.init		       = display_windows_init,
	.free		       = display_windows_free,
	.poll_event	       = display_windows_poll_event,
	.wait_event	       = display_windows_wait_event,
	.window_init	       = display_windows_window_init,
	.window_free	       = display_windows_window_free,
	.window_id	       = display_windows_window_id,
	.window_set_title      = display_windows_window_set_title,
	.window_set_position   = display_windows_window_set_position,
	.window_set_size       = display_windows_window_set_size,
	.window_set_borderless = display_windows_window_set_borderless,
	.window_set_fullscreen = display_windows_window_set_fullscreen,
	.window_show	       = display_windows_window_show,
	.window_hide	       = display_windows_window_hide,
};

DISPLAY_DRIVER(display_windows, &display_windows);
