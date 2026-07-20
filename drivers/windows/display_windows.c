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
typedef int(WINAPI *get_window_text_lengtha_t)(HWND);
typedef int(WINAPI *get_window_texta_t)(HWND, LPSTR, int);
typedef BOOL(WINAPI *set_window_pos_t)(HWND, HWND, int, int, int, int, UINT);
typedef BOOL(WINAPI *adjust_window_rect_ex_t)(LPRECT, DWORD, BOOL, DWORD);
typedef BOOL(WINAPI *get_window_rect_t)(HWND, LPRECT);
typedef BOOL(WINAPI *get_client_rect_t)(HWND, LPRECT);
typedef HMONITOR(WINAPI *monitor_from_window_t)(HWND, DWORD);
typedef BOOL(WINAPI *get_monitor_infoa_t)(HMONITOR, LPMONITORINFO);
typedef BOOL(WINAPI *screen_to_client_t)(HWND, LPPOINT);
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

#define DISPLAY_WINDOWS_STYLE_NORMAL	   WS_OVERLAPPEDWINDOW
#define DISPLAY_WINDOWS_STYLE_BORDERLESS   WS_POPUP
#define DISPLAY_WINDOWS_EX_STYLE_NORMAL	   0
#define DISPLAY_WINDOWS_WM_INTERNAL_0X0060 0x0060

typedef struct display_windows_s {
	proc_t *proc;
	void *user32;
	HINSTANCE instance;
	ATOM window_class;
	register_class_exa_t RegisterClassExA;
	unregister_classa_t UnregisterClassA;
	create_window_exa_t CreateWindowExA;
	destroy_window_t DestroyWindow;
	show_window_t ShowWindow;
	update_window_t UpdateWindow;
	set_window_texta_t SetWindowTextA;
	get_window_text_lengtha_t GetWindowTextLengthA;
	get_window_texta_t GetWindowTextA;
	set_window_pos_t SetWindowPos;
	adjust_window_rect_ex_t AdjustWindowRectEx;
	get_window_rect_t GetWindowRect;
	get_client_rect_t GetClientRect;
	monitor_from_window_t MonitorFromWindow;
	get_monitor_infoa_t GetMonitorInfoA;
	screen_to_client_t ScreenToClient;
	peek_messagea_t PeekMessageA;
	get_messagea_t GetMessageA;
	translate_message_t TranslateMessage;
	dispatch_messagea_t DispatchMessageA;
	def_window_proca_t DefWindowProcA;
	get_window_long_ptra_t GetWindowLongPtrA;
	set_window_long_ptra_t SetWindowLongPtrA;
	display_windows_wndproc_api_t wndproc_api;
	size_t emitted;
} display_windows_t;

typedef struct window_windows_s {
	HWND handle;
	int visible;
	int borderless;
	int fullscreen;
	DWORD style;
	DWORD ex_style;
	RECT restore_rect;
	display_modifier_t modifiers;
	display_modifier_t lock_modifiers;
	u8 keys[__DISPLAY_KEY_MAX];
} window_windows_t;

static display_windows_wndproc_api_t *s_wndproc_api;

static void display_windows_emit_event(display_t *display, const display_event_t *event)
{
	display_windows_t *dwindows = display == NULL ? NULL : display->data;
	if (dwindows != NULL) {
		dwindows->emitted++;
	}

	display_emit_event(display, event);
}

static void display_windows_wndproc_emit_close(display_t *display, HWND hwnd)
{
	display_event_t event = {
		.type	= DISPLAY_EVENT_CLOSE,
		.window = (u32)(uintptr_t)hwnd,
	};

	display_windows_emit_event(display, &event);
}

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

	if (dwindows != NULL && (message == WM_CLOSE || (message == WM_SYSCOMMAND && (wparam & 0xfff0) == SC_CLOSE))) {
		window_t *wnd = (window_t *)dwindows->GetWindowLongPtrA(hwnd, GWLP_USERDATA);
		if (wnd != NULL) {
			display_windows_wndproc_emit_close(wnd->display, hwnd);
		}
		return 0;
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

static int display_windows_load_proc(display_windows_t *dwindows, void **sym, strv_t name)
{
	if (proc_dlsym(dwindows->proc, dwindows->user32, name, sym)) {
		log_error("cdisplay", "display_windows", NULL, "failed to load user32 symbol: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

static int display_windows_load_user32(display_windows_t *dwindows)
{
	if (proc_dlopen(dwindows->proc, STRV("user32.dll"), &dwindows->user32)) {
		log_error("cdisplay", "display_windows", NULL, "failed to load user32.dll");
		return 1;
	}

	if (display_windows_load_proc(dwindows, (void **)&dwindows->RegisterClassExA, STRV("RegisterClassExA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->UnregisterClassA, STRV("UnregisterClassA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->CreateWindowExA, STRV("CreateWindowExA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->DestroyWindow, STRV("DestroyWindow")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->ShowWindow, STRV("ShowWindow")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->UpdateWindow, STRV("UpdateWindow")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->SetWindowTextA, STRV("SetWindowTextA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->GetWindowTextLengthA, STRV("GetWindowTextLengthA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->GetWindowTextA, STRV("GetWindowTextA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->SetWindowPos, STRV("SetWindowPos")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->AdjustWindowRectEx, STRV("AdjustWindowRectEx")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->GetWindowRect, STRV("GetWindowRect")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->GetClientRect, STRV("GetClientRect")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->MonitorFromWindow, STRV("MonitorFromWindow")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->GetMonitorInfoA, STRV("GetMonitorInfoA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->ScreenToClient, STRV("ScreenToClient")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->PeekMessageA, STRV("PeekMessageA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->GetMessageA, STRV("GetMessageA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->TranslateMessage, STRV("TranslateMessage")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->DispatchMessageA, STRV("DispatchMessageA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->DefWindowProcA, STRV("DefWindowProcA")) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->GetWindowLongPtrA, STRV(DISPLAY_WINDOWS_GET_WINDOW_LONG_PTR)) ||
	    display_windows_load_proc(dwindows, (void **)&dwindows->SetWindowLongPtrA, STRV(DISPLAY_WINDOWS_SET_WINDOW_LONG_PTR))) {
		proc_dlclose(dwindows->proc, dwindows->user32);
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

static window_t *display_windows_window_from_hwnd(display_windows_t *dwindows, HWND hwnd)
{
	if (dwindows == NULL || dwindows->GetWindowLongPtrA == NULL || hwnd == NULL) {
		return NULL;
	}

	return (window_t *)dwindows->GetWindowLongPtrA(hwnd, GWLP_USERDATA);
}

static display_key_t display_windows_key_from_message(const MSG *msg)
{
	UINT code    = (UINT)msg->wParam;
	UINT scan    = (UINT)((msg->lParam >> 16) & 0xff);
	int extended = (msg->lParam & (1l << 24)) != 0;

	if (code >= 'A' && code <= 'Z') {
		return (display_key_t)(DISPLAY_KEY_A + code - 'A');
	}
	if (code >= '0' && code <= '9') {
		return (display_key_t)(DISPLAY_KEY_0 + code - '0');
	}
	if (code >= VK_F1 && code <= VK_F12) {
		return (display_key_t)(DISPLAY_KEY_F1 + code - VK_F1);
	}
	if (code >= VK_NUMPAD0 && code <= VK_NUMPAD9) {
		return (display_key_t)(DISPLAY_KEY_KP_0 + code - VK_NUMPAD0);
	}

	switch (code) {
	case VK_OEM_3:
		return DISPLAY_KEY_GRAVE;
	case VK_OEM_MINUS:
		return DISPLAY_KEY_MINUS;
	case VK_OEM_PLUS:
		return DISPLAY_KEY_EQUAL;
	case VK_OEM_4:
		return DISPLAY_KEY_LEFT_BRACKET;
	case VK_OEM_6:
		return DISPLAY_KEY_RIGHT_BRACKET;
	case VK_OEM_5:
		return DISPLAY_KEY_BACKSLASH;
	case VK_OEM_1:
		return DISPLAY_KEY_SEMICOLON;
	case VK_OEM_7:
		return DISPLAY_KEY_APOSTROPHE;
	case VK_OEM_COMMA:
		return DISPLAY_KEY_COMMA;
	case VK_OEM_PERIOD:
		return DISPLAY_KEY_PERIOD;
	case VK_OEM_2:
		return DISPLAY_KEY_SLASH;
	case VK_SPACE:
		return DISPLAY_KEY_SPACE;
	case VK_RETURN:
		return extended ? DISPLAY_KEY_KP_ENTER : DISPLAY_KEY_ENTER;
	case VK_TAB:
		return DISPLAY_KEY_TAB;
	case VK_BACK:
		return DISPLAY_KEY_BACKSPACE;
	case VK_ESCAPE:
		return DISPLAY_KEY_ESCAPE;
	case VK_CAPITAL:
		return DISPLAY_KEY_CAPS_LOCK;
	case VK_NUMLOCK:
		return DISPLAY_KEY_NUM_LOCK;
	case VK_SCROLL:
		return DISPLAY_KEY_SCROLL_LOCK;
	case VK_PAUSE:
		return DISPLAY_KEY_PAUSE;
	case VK_SNAPSHOT:
		return DISPLAY_KEY_PRINT_SCREEN;
	case VK_CLEAR:
		return DISPLAY_KEY_KP_5;
	case VK_INSERT:
		return DISPLAY_KEY_INSERT;
	case VK_DELETE:
		return DISPLAY_KEY_DELETE;
	case VK_HOME:
		return DISPLAY_KEY_HOME;
	case VK_END:
		return DISPLAY_KEY_END;
	case VK_PRIOR:
		return DISPLAY_KEY_PAGE_UP;
	case VK_NEXT:
		return DISPLAY_KEY_PAGE_DOWN;
	case VK_UP:
		return DISPLAY_KEY_UP;
	case VK_DOWN:
		return DISPLAY_KEY_DOWN;
	case VK_LEFT:
		return DISPLAY_KEY_LEFT;
	case VK_RIGHT:
		return DISPLAY_KEY_RIGHT;
	case VK_SHIFT:
		return scan == 0x36 ? DISPLAY_KEY_RIGHT_SHIFT : DISPLAY_KEY_LEFT_SHIFT;
	case VK_LSHIFT:
		return DISPLAY_KEY_LEFT_SHIFT;
	case VK_RSHIFT:
		return DISPLAY_KEY_RIGHT_SHIFT;
	case VK_CONTROL:
		return extended ? DISPLAY_KEY_RIGHT_CONTROL : DISPLAY_KEY_LEFT_CONTROL;
	case VK_LCONTROL:
		return DISPLAY_KEY_LEFT_CONTROL;
	case VK_RCONTROL:
		return DISPLAY_KEY_RIGHT_CONTROL;
	case VK_MENU:
		return extended ? DISPLAY_KEY_RIGHT_ALT : DISPLAY_KEY_LEFT_ALT;
	case VK_LMENU:
		return DISPLAY_KEY_LEFT_ALT;
	case VK_RMENU:
		return DISPLAY_KEY_RIGHT_ALT;
	case VK_LWIN:
		return DISPLAY_KEY_LEFT_SUPER;
	case VK_RWIN:
		return DISPLAY_KEY_RIGHT_SUPER;
	case VK_APPS:
		return DISPLAY_KEY_MENU;
	case VK_DECIMAL:
		return DISPLAY_KEY_KP_DECIMAL;
	case VK_DIVIDE:
		return DISPLAY_KEY_KP_DIVIDE;
	case VK_MULTIPLY:
		return DISPLAY_KEY_KP_MULTIPLY;
	case VK_SUBTRACT:
		return DISPLAY_KEY_KP_SUBTRACT;
	case VK_ADD:
		return DISPLAY_KEY_KP_ADD;
	default:
		log_warn("cdisplay", "display_windows", NULL, "unknown Windows virtual key: %u", code);
		return DISPLAY_KEY_UNKNOWN;
	}
}

static display_modifier_t display_windows_modifier_from_key(display_key_t key)
{
	switch (key) {
	case DISPLAY_KEY_LEFT_SHIFT:
	case DISPLAY_KEY_RIGHT_SHIFT:
		return DISPLAY_MOD_SHIFT;
	case DISPLAY_KEY_CAPS_LOCK:
		return DISPLAY_MOD_CAPS_LOCK;
	case DISPLAY_KEY_LEFT_CONTROL:
	case DISPLAY_KEY_RIGHT_CONTROL:
		return DISPLAY_MOD_CONTROL;
	case DISPLAY_KEY_LEFT_ALT:
	case DISPLAY_KEY_RIGHT_ALT:
		return DISPLAY_MOD_ALT;
	case DISPLAY_KEY_NUM_LOCK:
		return DISPLAY_MOD_NUM_LOCK;
	case DISPLAY_KEY_LEFT_SUPER:
	case DISPLAY_KEY_RIGHT_SUPER:
		return DISPLAY_MOD_SUPER;
	default:
		return DISPLAY_MOD_NONE;
	}
}

static void display_windows_rebuild_modifiers(window_windows_t *wwindows)
{
	display_modifier_t modifiers = wwindows->lock_modifiers;

	if (wwindows->keys[DISPLAY_KEY_LEFT_SHIFT] || wwindows->keys[DISPLAY_KEY_RIGHT_SHIFT]) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_SHIFT);
	}
	if (wwindows->keys[DISPLAY_KEY_LEFT_CONTROL] || wwindows->keys[DISPLAY_KEY_RIGHT_CONTROL]) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_CONTROL);
	}
	if (wwindows->keys[DISPLAY_KEY_LEFT_ALT] || wwindows->keys[DISPLAY_KEY_RIGHT_ALT]) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_ALT);
	}
	if (wwindows->keys[DISPLAY_KEY_LEFT_SUPER] || wwindows->keys[DISPLAY_KEY_RIGHT_SUPER]) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_SUPER);
	}

	wwindows->modifiers = modifiers;
}

static void display_windows_update_modifiers(window_windows_t *wwindows, display_key_t key, int down, int repeat)
{
	display_modifier_t modifier = display_windows_modifier_from_key(key);
	if (modifier == DISPLAY_MOD_NONE) {
		return;
	}

	if (key == DISPLAY_KEY_CAPS_LOCK || key == DISPLAY_KEY_NUM_LOCK) {
		if (down && !repeat) {
			wwindows->lock_modifiers = (display_modifier_t)(wwindows->lock_modifiers ^ modifier);
			display_windows_rebuild_modifiers(wwindows);
		}
		return;
	}

	if (key > DISPLAY_KEY_UNKNOWN && key < __DISPLAY_KEY_MAX) {
		wwindows->keys[key] = down ? 1 : 0;
		display_windows_rebuild_modifiers(wwindows);
	}
}

static int display_windows_translate_key_message(display_windows_t *dwindows, const MSG *msg, display_event_t *event)
{
	window_t *wnd = display_windows_window_from_hwnd(dwindows, msg->hwnd);
	if (wnd == NULL || wnd->data == NULL) {
		return 1;
	}

	window_windows_t *wwindows = wnd->data;
	int down		   = msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN;
	int repeat		   = (msg->lParam & (1l << 30)) != 0;

	event->type   = down ? DISPLAY_EVENT_KEY_DOWN : DISPLAY_EVENT_KEY_UP;
	event->window = (u32)(uintptr_t)msg->hwnd;
	event->key    = display_windows_key_from_message(msg);
	display_windows_update_modifiers(wwindows, event->key, down, repeat);
	event->modifiers = wwindows->modifiers;

	return 0;
}

static short display_windows_i16_lparam(LPARAM value)
{
	return (short)(value & 0xffff);
}

static int display_windows_mouse_point(display_windows_t *dwindows, const MSG *msg, int screen, u16 *x, u16 *y)
{
	POINT point = {
		.x = display_windows_i16_lparam(msg->lParam),
		.y = display_windows_i16_lparam(msg->lParam >> 16),
	};

	if (screen && !dwindows->ScreenToClient(msg->hwnd, &point)) {
		return 1;
	}

	*x = (u16)point.x;
	*y = (u16)point.y;

	return 0;
}

static display_mouse_t display_windows_mouse_from_message(const MSG *msg)
{
	switch (msg->message) {
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCLBUTTONDBLCLK:
		return DISPLAY_MOUSE_LEFT;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONUP:
	case WM_NCMBUTTONDBLCLK:
		return DISPLAY_MOUSE_MIDDLE;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONUP:
	case WM_NCRBUTTONDBLCLK:
		return DISPLAY_MOUSE_RIGHT;
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_XBUTTONDBLCLK:
	case WM_NCXBUTTONDOWN:
	case WM_NCXBUTTONUP:
	case WM_NCXBUTTONDBLCLK:
		switch (HIWORD(msg->wParam)) {
		case XBUTTON1:
			return DISPLAY_MOUSE_BACK;
		case XBUTTON2:
			return DISPLAY_MOUSE_FORWARD;
		default:
			return DISPLAY_MOUSE_UNKNOWN;
		}
	case WM_MOUSEWHEEL:
		return GET_WHEEL_DELTA_WPARAM(msg->wParam) > 0 ? DISPLAY_MOUSE_WHEEL_UP : DISPLAY_MOUSE_WHEEL_DOWN;
	case WM_MOUSEHWHEEL:
		return GET_WHEEL_DELTA_WPARAM(msg->wParam) > 0 ? DISPLAY_MOUSE_WHEEL_RIGHT : DISPLAY_MOUSE_WHEEL_LEFT;
	default:
		return DISPLAY_MOUSE_UNKNOWN;
	}
}

static int display_windows_mouse_is_down(UINT message)
{
	switch (message) {
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONDBLCLK:
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONDBLCLK:
	case WM_NCXBUTTONDOWN:
	case WM_NCXBUTTONDBLCLK:
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
		return 1;
	default:
		return 0;
	}
}

static int display_windows_mouse_is_nonclient(UINT message)
{
	switch (message) {
	case WM_NCMOUSEMOVE:
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCLBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONUP:
	case WM_NCMBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONUP:
	case WM_NCRBUTTONDBLCLK:
	case WM_NCXBUTTONDOWN:
	case WM_NCXBUTTONUP:
	case WM_NCXBUTTONDBLCLK:
		return 1;
	default:
		return 0;
	}
}

static display_modifier_t display_windows_mouse_modifier(display_mouse_t button)
{
	switch (button) {
	case DISPLAY_MOUSE_LEFT:
		return DISPLAY_MOD_MOUSE_LEFT;
	case DISPLAY_MOUSE_MIDDLE:
		return DISPLAY_MOD_MOUSE_MIDDLE;
	case DISPLAY_MOUSE_RIGHT:
		return DISPLAY_MOD_MOUSE_RIGHT;
	case DISPLAY_MOUSE_WHEEL_UP:
		return DISPLAY_MOD_MOUSE_WHEEL_UP;
	case DISPLAY_MOUSE_WHEEL_DOWN:
		return DISPLAY_MOD_MOUSE_WHEEL_DOWN;
	default:
		return DISPLAY_MOD_NONE;
	}
}

static void display_windows_rebuild_mouse_modifiers(window_windows_t *wwindows, WPARAM state)
{
	display_modifier_t modifiers = wwindows->modifiers;

	modifiers = (display_modifier_t)(modifiers & ~(DISPLAY_MOD_MOUSE_LEFT | DISPLAY_MOD_MOUSE_MIDDLE | DISPLAY_MOD_MOUSE_RIGHT |
						       DISPLAY_MOD_MOUSE_WHEEL_UP | DISPLAY_MOD_MOUSE_WHEEL_DOWN));

	if (state & MK_LBUTTON) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_MOUSE_LEFT);
	}
	if (state & MK_MBUTTON) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_MOUSE_MIDDLE);
	}
	if (state & MK_RBUTTON) {
		modifiers = (display_modifier_t)(modifiers | DISPLAY_MOD_MOUSE_RIGHT);
	}

	wwindows->modifiers = modifiers;
}

static int display_windows_translate_mouse_message(display_windows_t *dwindows, const MSG *msg, display_event_t *event)
{
	window_t *wnd = display_windows_window_from_hwnd(dwindows, msg->hwnd);
	if (wnd == NULL || wnd->data == NULL) {
		return 1;
	}

	window_windows_t *wwindows = wnd->data;
	int wheel		   = msg->message == WM_MOUSEWHEEL || msg->message == WM_MOUSEHWHEEL;
	int nonclient		   = display_windows_mouse_is_nonclient(msg->message);

	event->window = (u32)(uintptr_t)msg->hwnd;
	if (display_windows_mouse_point(dwindows, msg, wheel || nonclient, &event->x, &event->y)) {
		return 1;
	}

	if (msg->message == WM_MOUSEMOVE || msg->message == WM_NCMOUSEMOVE) {
		if (!nonclient) {
			display_windows_rebuild_mouse_modifiers(wwindows, msg->wParam);
		}
		event->type	 = DISPLAY_EVENT_MOUSE_MOVE;
		event->modifiers = wwindows->modifiers;
		return 0;
	}

	event->button = display_windows_mouse_from_message(msg);
	if (event->button == DISPLAY_MOUSE_UNKNOWN) {
		return 1;
	}

	if (wheel) {
		display_modifier_t modifier = display_windows_mouse_modifier(event->button);
		event->type		    = DISPLAY_EVENT_MOUSE_DOWN;
		event->modifiers	    = (display_modifier_t)(wwindows->modifiers | modifier);
		return 0;
	}

	if (nonclient) {
		display_modifier_t modifier = display_windows_mouse_modifier(event->button);
		if (display_windows_mouse_is_down(msg->message)) {
			wwindows->modifiers = (display_modifier_t)(wwindows->modifiers | modifier);
		} else {
			wwindows->modifiers = (display_modifier_t)(wwindows->modifiers & ~modifier);
		}
	} else {
		display_windows_rebuild_mouse_modifiers(wwindows, msg->wParam);
	}
	event->type	 = display_windows_mouse_is_down(msg->message) ? DISPLAY_EVENT_MOUSE_DOWN : DISPLAY_EVENT_MOUSE_UP;
	event->modifiers = wwindows->modifiers;

	return 0;
}

static int display_windows_translate_message(display_windows_t *dwindows, const MSG *msg, display_event_t *event)
{
	*event = (display_event_t){0};

	switch (msg->message) {
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		return display_windows_translate_key_message(dwindows, msg, event);
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_XBUTTONDBLCLK:
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
	case WM_NCMOUSEMOVE:
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCLBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONUP:
	case WM_NCMBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONUP:
	case WM_NCRBUTTONDBLCLK:
	case WM_NCXBUTTONDOWN:
	case WM_NCXBUTTONUP:
	case WM_NCXBUTTONDBLCLK:
		return display_windows_translate_mouse_message(dwindows, msg, event);
	case WM_SYSCOMMAND:
		if ((msg->wParam & 0xfff0) != SC_CLOSE) {
			return 1;
		}
		event->type   = DISPLAY_EVENT_CLOSE;
		event->window = (u32)(uintptr_t)msg->hwnd;
		return 0;
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

static int display_windows_message_is_silent(UINT message)
{
	switch (message) {
	case WM_TIMER:
	case DISPLAY_WINDOWS_WM_INTERNAL_0X0060:
		return 1;
	default:
		return 0;
	}
}

static int display_windows_dispatch_message(display_windows_t *dwindows, const MSG *msg)
{
	dwindows->TranslateMessage(msg);
	dwindows->DispatchMessageA(msg);
	return 0;
}

static void display_windows_log_unhandled_message(const MSG *msg)
{
	log_warn("cdisplay",
		 "display_windows",
		 NULL,
		 "unhandled Windows message: hwnd=%p message=%u wParam=%p lParam=%p",
		 msg->hwnd,
		 msg->message,
		 (void *)(uintptr_t)msg->wParam,
		 (void *)(uintptr_t)msg->lParam);
}

static int display_windows_message_needs_dispatch(UINT message)
{
	switch (message) {
	case WM_CLOSE:
	case WM_SYSCOMMAND:
		return 0;
	default:
		return 1;
	}
}

static int display_windows_set_style(display_windows_t *dwindows, window_windows_t *wwindows, DWORD style)
{
	DWORD native_style = style;
	if (wwindows->visible) {
		native_style |= WS_VISIBLE;
	}

	if (dwindows->SetWindowLongPtrA(wwindows->handle, GWL_STYLE, (LONG_PTR)native_style) == 0) {
		return 1;
	}

	wwindows->style = style;
	return 0;
}

static int display_windows_apply_frame(display_windows_t *dwindows, window_windows_t *wwindows, const RECT *rect, UINT flags)
{
	int x	   = 0;
	int y	   = 0;
	int width  = 0;
	int height = 0;

	flags |= SWP_FRAMECHANGED;

	if (rect == NULL) {
		flags |= SWP_NOMOVE | SWP_NOSIZE;
	} else {
		x      = rect->left;
		y      = rect->top;
		width  = rect->right - rect->left;
		height = rect->bottom - rect->top;
	}

	return dwindows->SetWindowPos(wwindows->handle, NULL, x, y, width, height, flags) ? 0 : 1;
}

static DWORD display_windows_window_style(const window_windows_t *wwindows)
{
	return wwindows->borderless ? DISPLAY_WINDOWS_STYLE_BORDERLESS : DISPLAY_WINDOWS_STYLE_NORMAL;
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
	dwindows->proc		    = display->proc;
	if (proc_dlmain(dwindows->proc, (void **)&dwindows->instance)) {
		mem_free(display->data, sizeof(display_windows_t));
		display->data = NULL;
		return 1;
	}

	if (display_windows_load_user32(dwindows)) {
		if (dwindows->user32 != NULL) {
			proc_dlclose(dwindows->proc, dwindows->user32);
		}
		mem_free(display->data, sizeof(display_windows_t));
		display->data = NULL;
		return 1;
	}

	s_wndproc_api = &dwindows->wndproc_api;

	if (display_windows_register_class(dwindows)) {
		s_wndproc_api = NULL;
		proc_dlclose(dwindows->proc, dwindows->user32);
		mem_free(display->data, sizeof(display_windows_t));
		display->data = NULL;
		return 1;
	}

	return 0;
}

static int display_windows_available(display_driver_t *driver, proc_t *proc)
{
	(void)driver;
	(void)proc;
	return 1;
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
			proc_dlclose(dwindows->proc, dwindows->user32);
		}
		mem_free(dwindows, sizeof(display_windows_t));
	}

	return 0;
}

static int display_windows_poll_events(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	display_windows_t *dwindows = display->data;
	if (dwindows == NULL) {
		return 1;
	}

	MSG msg;
	while (dwindows->PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
		display_event_t event = {0};
		size_t emitted	      = dwindows->emitted;
		if (display_windows_translate_message(dwindows, &msg, &event) == 0) {
			if (display_windows_message_needs_dispatch(msg.message)) {
				display_windows_dispatch_message(dwindows, &msg);
				if (dwindows->emitted != emitted) {
					return 0;
				}
			}
			display_windows_emit_event(display, &event);
			return 0;
		}
		if (!display_windows_message_is_silent(msg.message)) {
			display_windows_log_unhandled_message(&msg);
		}
		display_windows_dispatch_message(dwindows, &msg);
		if (dwindows->emitted != emitted) {
			return 0;
		}
	}

	return 0;
}

static int display_windows_wait_events(display_t *display)
{
	if (display == NULL) {
		return 1;
	}

	display_windows_t *dwindows = display->data;
	if (dwindows == NULL) {
		return 1;
	}

	MSG msg;
	while (dwindows->GetMessageA(&msg, NULL, 0, 0) > 0) {
		display_event_t event = {0};
		size_t emitted	      = dwindows->emitted;
		if (display_windows_translate_message(dwindows, &msg, &event) == 0) {
			if (display_windows_message_needs_dispatch(msg.message)) {
				display_windows_dispatch_message(dwindows, &msg);
				if (dwindows->emitted != emitted) {
					return 0;
				}
			}
			display_windows_emit_event(display, &event);
			return 0;
		}
		if (!display_windows_message_is_silent(msg.message)) {
			display_windows_log_unhandled_message(&msg);
		}
		display_windows_dispatch_message(dwindows, &msg);
		if (dwindows->emitted != emitted) {
			return 0;
		}
	}

	return 1;
}

static int display_windows_window_init(window_t *wnd, const window_config_t *config)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || config == NULL) {
		return 1;
	}

	wnd->data = mem_alloc(sizeof(window_windows_t));
	if (wnd->data == NULL) {
		return 1;
	}
	mem_set(wnd->data, 0, sizeof(window_windows_t));

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;

	wwindows->style	   = DISPLAY_WINDOWS_STYLE_NORMAL;
	wwindows->ex_style = DISPLAY_WINDOWS_EX_STYLE_NORMAL;

	RECT rect = {0, 0, config->width, config->height};
	if (!dwindows->AdjustWindowRectEx(&rect, wwindows->style, FALSE, wwindows->ex_style)) {
		mem_free(wnd->data, sizeof(window_windows_t));
		wnd->data = NULL;
		return 1;
	}

	wwindows->handle = dwindows->CreateWindowExA(wwindows->ex_style,
						     "cdisplay_window",
						     "cdisplay",
						     wwindows->style,
						     config->x,
						     config->y,
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
	window_windows_t *wwindows  = wnd->data;

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

static int display_windows_native(display_t *display, display_native_t *native)
{
	if (display == NULL || display->data == NULL || native == NULL) {
		return 1;
	}

	display_windows_t *dwindows = display->data;
	native->type		    = DISPLAY_NATIVE_WINDOWS;
	native->display		    = dwindows->instance;
	return native->display == NULL;
}

static int display_windows_window_native(window_t *wnd, window_native_t *native)
{
	if (wnd == NULL || wnd->data == NULL || native == NULL) {
		return 1;
	}

	window_windows_t *wwindows = wnd->data;
	native->type		   = DISPLAY_NATIVE_WINDOWS;
	native->window		   = wwindows->handle;
	return native->window == NULL;
}

static int display_windows_window_set_title(window_t *wnd, strv_t title)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL ||
	    (title.data == NULL && title.len > 0)) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;

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

static int display_windows_window_get_title(window_t *wnd, char *title, size_t size)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL || title == NULL || size == 0 ||
	    size > INT_MAX) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;
	int len			    = dwindows->GetWindowTextLengthA(wwindows->handle);
	if (len < 0 || (size_t)len >= size) {
		return 1;
	}

	int copied = dwindows->GetWindowTextA(wwindows->handle, title, (int)size);
	if (copied < 0) {
		return 1;
	}
	title[copied] = 0;
	return 0;
}

static int display_windows_window_set_position(window_t *wnd, u16 x, u16 y)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;

	return dwindows->SetWindowPos(wwindows->handle, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE) ? 0 : 1;
}

static int display_windows_window_get_position(window_t *wnd, u16 *x, u16 *y)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL || x == NULL || y == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;
	RECT rect		    = {0};
	if (!dwindows->GetWindowRect(wwindows->handle, &rect) || rect.left < 0 || rect.top < 0 || rect.left > UINT16_MAX ||
	    rect.top > UINT16_MAX) {
		return 1;
	}

	*x = (u16)rect.left;
	*y = (u16)rect.top;
	return 0;
}

static int display_windows_window_set_size(window_t *wnd, u16 width, u16 height)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;

	RECT rect = {0, 0, width, height};
	if (!dwindows->AdjustWindowRectEx(&rect, wwindows->style, FALSE, wwindows->ex_style)) {
		return 1;
	}

	return dwindows->SetWindowPos(
		       wwindows->handle, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOMOVE)
		       ? 0
		       : 1;
}

static int display_windows_window_get_size(window_t *wnd, u16 *width, u16 *height)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL || width == NULL || height == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;
	RECT rect		    = {0};
	if (!dwindows->GetClientRect(wwindows->handle, &rect) || rect.right < rect.left || rect.bottom < rect.top ||
	    rect.right - rect.left > UINT16_MAX || rect.bottom - rect.top > UINT16_MAX) {
		return 1;
	}

	*width	= (u16)(rect.right - rect.left);
	*height = (u16)(rect.bottom - rect.top);
	return 0;
}

static int display_windows_window_set_borderless(window_t *wnd, int borderless)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;
	int enabled		    = borderless != 0;

	if (wwindows->borderless == enabled) {
		return 0;
	}

	wwindows->borderless = enabled;

	if (wwindows->fullscreen) {
		return 0;
	}

	DWORD old_style = wwindows->style;
	DWORD style	= display_windows_window_style(wwindows);
	if (display_windows_set_style(dwindows, wwindows, style)) {
		wwindows->borderless = !enabled;
		return 1;
	}

	if (display_windows_apply_frame(dwindows, wwindows, NULL, SWP_NOACTIVATE | SWP_NOZORDER)) {
		display_windows_set_style(dwindows, wwindows, old_style);
		wwindows->borderless = !enabled;
		return 1;
	}

	return 0;
}

static int display_windows_window_get_borderless(window_t *wnd, int *borderless)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL || borderless == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;
	LONG_PTR style		    = dwindows->GetWindowLongPtrA(wwindows->handle, GWL_STYLE);
	*borderless		    = ((DWORD)style & DISPLAY_WINDOWS_STYLE_BORDERLESS) == DISPLAY_WINDOWS_STYLE_BORDERLESS;
	return 0;
}

static int display_windows_window_set_fullscreen(window_t *wnd, int fullscreen)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;
	int enabled		    = fullscreen != 0;

	if (wwindows->fullscreen == enabled) {
		return 0;
	}

	if (enabled) {
		if (!dwindows->GetWindowRect(wwindows->handle, &wwindows->restore_rect)) {
			return 1;
		}

		HMONITOR monitor = dwindows->MonitorFromWindow(wwindows->handle, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info = {0};
		info.cbSize	 = sizeof(info);
		if (monitor == NULL || !dwindows->GetMonitorInfoA(monitor, &info)) {
			return 1;
		}

		DWORD old_style = wwindows->style;
		if (display_windows_set_style(dwindows, wwindows, DISPLAY_WINDOWS_STYLE_BORDERLESS)) {
			return 1;
		}

		if (display_windows_apply_frame(dwindows, wwindows, &info.rcMonitor, SWP_SHOWWINDOW)) {
			display_windows_set_style(dwindows, wwindows, old_style);
			return 1;
		}

		wwindows->fullscreen = 1;
		return 0;
	}

	DWORD style = display_windows_window_style(wwindows);
	if (display_windows_set_style(dwindows, wwindows, style)) {
		return 1;
	}

	if (display_windows_apply_frame(dwindows, wwindows, &wwindows->restore_rect, SWP_SHOWWINDOW)) {
		display_windows_set_style(dwindows, wwindows, DISPLAY_WINDOWS_STYLE_BORDERLESS);
		return 1;
	}

	wwindows->fullscreen = 0;
	return 0;
}

static int display_windows_window_get_fullscreen(window_t *wnd, int *fullscreen)
{
	if (wnd == NULL || wnd->data == NULL || fullscreen == NULL) {
		return 1;
	}

	window_windows_t *wwindows = wnd->data;
	*fullscreen		   = wwindows->fullscreen;
	return 0;
}

static int display_windows_window_show(window_t *wnd)
{
	if (wnd == NULL || wnd->display == NULL || wnd->display->data == NULL || wnd->data == NULL) {
		return 1;
	}

	display_windows_t *dwindows = wnd->display->data;
	window_windows_t *wwindows  = wnd->data;

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
	window_windows_t *wwindows  = wnd->data;

	dwindows->ShowWindow(wwindows->handle, SW_HIDE);
	wwindows->visible = 0;

	return 0;
}

static display_driver_t display_windows = {
	.name		       = "windows",
	.available	       = display_windows_available,
	.init		       = display_windows_init,
	.free		       = display_windows_free,
	.poll_events	       = display_windows_poll_events,
	.wait_events	       = display_windows_wait_events,
	.native		       = display_windows_native,
	.window_init	       = display_windows_window_init,
	.window_free	       = display_windows_window_free,
	.window_id	       = display_windows_window_id,
	.window_native	       = display_windows_window_native,
	.window_set_title      = display_windows_window_set_title,
	.window_get_title      = display_windows_window_get_title,
	.window_set_position   = display_windows_window_set_position,
	.window_get_position   = display_windows_window_get_position,
	.window_set_size       = display_windows_window_set_size,
	.window_get_size       = display_windows_window_get_size,
	.window_set_borderless = display_windows_window_set_borderless,
	.window_get_borderless = display_windows_window_get_borderless,
	.window_set_fullscreen = display_windows_window_set_fullscreen,
	.window_get_fullscreen = display_windows_window_get_fullscreen,
	.window_show	       = display_windows_window_show,
	.window_hide	       = display_windows_window_hide,
};

DISPLAY_DRIVER(display_windows, &display_windows);
