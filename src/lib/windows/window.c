#include "lib/lib.h"

#include <stdlib.h>
#include <stdio.h>

#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <d3d11.h>
#include <dxgi1_3.h>

#include "window-quad.h"

#define WINDOW_CLASS_NAME L"LIBWindowClass"
#define WINDOW_SWAP_CHAIN_FLAGS DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT

struct window {
	HWND hwnd;
	ATOM class;
	HINSTANCE instance;
	WINDOW_MSG_FUNC msg_func;
	const void *opaque;

	uint32_t width;
	uint32_t height;
	ID3D11Device *device;
	ID3D11DeviceContext *context;
	IDXGISwapChain2 *swap_chain2;
	HANDLE waitable;

	struct window_quad *quad;
};

static __declspec(thread) char WINDOW_DRAG[MAX_PATH];

/*** WINDOW ***/

static void window_utf8_to_wchar(const char *src, WCHAR *dst, uint32_t dst_len)
{
	int32_t n = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, (int32_t) dst_len);

	if (n != strlen(src) + 1)
		memset(dst, 0, dst_len * sizeof(WCHAR));
}

static void window_wchar_to_utf8(WCHAR *src, char *dst, size_t dst_len)
{
	int32_t n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, src, -1, dst,
		(int32_t) dst_len, NULL, NULL);

	if (n > 0) {
		dst[n] = '\0';

	} else {
		memset(dst, 0, dst_len);
	}
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct window_msg wmsg = {0};
	bool custom_return = false;
	LRESULT r = 0;

	switch (msg) {
		case WM_NCCREATE:
			SetWindowLongPtr(hwnd, 0, (LONG_PTR) ((CREATESTRUCT *) lparam)->lpCreateParams);
			break;
		case WM_CLOSE:
			wmsg.type = WINDOW_MSG_CLOSE;
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_KEYUP:
		case WM_KEYDOWN:
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
			if (GetCursor())
				SetCursor(NULL);

			wmsg.type = WINDOW_MSG_KEYBOARD;
			wmsg.keyboard.pressed = !(lparam >> 31);
			wmsg.keyboard.scancode = lparam >> 16 & 0x00FF;
			if (lparam >> 24 & 0x01)
				wmsg.keyboard.scancode |= 0x0100;
			break;
		case WM_MOUSEMOVE:
			if (!GetCursor())
				SetCursor(LoadCursor(NULL, IDC_ARROW));

			wmsg.type = WINDOW_MSG_MOUSE_MOTION;
			wmsg.mouseMotion.relative = false;
			wmsg.mouseMotion.x = GET_X_LPARAM(lparam);
			wmsg.mouseMotion.y = GET_Y_LPARAM(lparam);
			break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			wmsg.type = WINDOW_MSG_MOUSE_BUTTON;
			wmsg.mouseButton.button = MOUSE_L;
			wmsg.mouseButton.pressed = msg == WM_LBUTTONDOWN;
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			wmsg.type = WINDOW_MSG_MOUSE_BUTTON;
			wmsg.mouseButton.button = MOUSE_R;
			wmsg.mouseButton.pressed = msg == WM_RBUTTONDOWN;
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			wmsg.type = WINDOW_MSG_MOUSE_BUTTON;
			wmsg.mouseButton.button = MOUSE_MIDDLE;
			wmsg.mouseButton.pressed = msg == WM_MBUTTONDOWN;
			break;
		case WM_MOUSEWHEEL:
			wmsg.type = WINDOW_MSG_MOUSE_WHEEL;
			wmsg.mouseWheel.x = 0;
			wmsg.mouseWheel.y = GET_WHEEL_DELTA_WPARAM(wparam);
			break;
		case WM_INPUT:
			UINT size = 0;

			GetRawInputData((HRAWINPUT) lparam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
			RAWINPUT *ri = malloc(size);

			if (GetRawInputData((HRAWINPUT) lparam, RID_INPUT, ri, &size, sizeof(RAWINPUTHEADER))) {
				if (ri->header.dwType == RIM_TYPEHID && ri->data.hid.dwCount == 1 && ri->data.hid.dwSizeHid >= 8) {
					if (GetCursor())
						SetCursor(NULL);

					uint8_t *hid = ri->data.hid.bRawData;
					wmsg.type = WINDOW_MSG_GAMEPAD;

					// Buttons
					wmsg.gamepad.a             = hid[5] & 0x10;
					wmsg.gamepad.b             = hid[5] & 0x20;
					wmsg.gamepad.x             = hid[5] & 0x80;
					wmsg.gamepad.y             = hid[6] & 0x01;
					wmsg.gamepad.leftShoulder  = hid[6] & 0x04;
					wmsg.gamepad.rightShoulder = hid[6] & 0x08;
					wmsg.gamepad.back          = hid[6] & 0x40;
					wmsg.gamepad.start         = hid[6] & 0x80;

					// Axis
					wmsg.gamepad.leftThumbX = hid[1] - 0x80;
					wmsg.gamepad.leftThumbY = hid[2] - 0x80;
				}
			}

			free(ri);
			custom_return = true;
			r = DefRawInputProc(&ri, 1, sizeof(RAWINPUTHEADER));
			break;
		case WM_DROPFILES:
			WCHAR name[MAX_PATH];

			if (DragQueryFile((HDROP) wparam, 0, name, MAX_PATH)) {
				SetForegroundWindow(hwnd);

				window_wchar_to_utf8(name, WINDOW_DRAG, MAX_PATH);
				wmsg.type = WINDOW_MSG_DRAG;
				wmsg.drag.name = WINDOW_DRAG;
			}
			break;
		default:
			break;
	}

	if (wmsg.type != WINDOW_MSG_NONE) {
		struct window *ctx = (struct window *) GetWindowLongPtr(hwnd, 0);

		if (ctx)
			ctx->msg_func(&wmsg, ctx->opaque);

		return 0;
	}

	return custom_return ? r : DefWindowProc(hwnd, msg, wparam, lparam);
}

static void window_calc_client_area(uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	rect.right = *width;
	rect.bottom = *height;
	if (AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0)) {
		*width = rect.right - rect.left;
		*height = rect.bottom - rect.top;
	}
}

enum lib_status window_create(const char *title, WINDOW_MSG_FUNC msg_func, const void *opaque,
	uint32_t width, uint32_t height, bool fullscreen, struct window **window)
{
	enum lib_status r = LIB_OK;

	struct window *ctx = *window = calloc(1, sizeof(struct window));
	ctx->msg_func = msg_func;
	ctx->opaque = opaque;

	IDXGIDevice2 *device2 = NULL;
	IUnknown *unknown = NULL;
	IDXGIAdapter *adapter = NULL;
	IDXGIFactory2 *factory2 = NULL;
	IDXGISwapChain1 *swap_chain1 = NULL;

	ctx->instance = GetModuleHandle(NULL);
	if (!ctx->instance) {r = LIB_ERR; goto except;}

	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.cbWndExtra = sizeof(struct window *);
	wc.lpfnWndProc = window_proc;
	wc.hInstance = ctx->instance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = WINDOW_CLASS_NAME;

	WCHAR path[MAX_PATH];
	GetModuleFileName(ctx->instance, path, MAX_PATH);
	ExtractIconEx(path, 0, &wc.hIcon, &wc.hIconSm, 1);

	ctx->class = RegisterClassEx(&wc);
	if (ctx->class == 0) {r = LIB_ERR; goto except;}

	RECT rect = {0};
	DWORD style = WS_OVERLAPPEDWINDOW;
	HWND desktop = GetDesktopWindow();
	int32_t x = CW_USEDEFAULT;
	int32_t y = CW_USEDEFAULT;

	if (fullscreen) {
		style = WS_POPUP;

		if (desktop && GetWindowRect(desktop, &rect)) {
			x = rect.left;
			y = rect.top;
			width = rect.right - rect.left;
			height = rect.bottom - rect.top;
		}
	} else {
		window_calc_client_area(&width, &height);

		if (desktop && GetWindowRect(desktop, &rect)) {
			x = (rect.right - width) / 2;
			y = (rect.bottom - height) / 2;
		}
	}

	WCHAR titlew[TITLE_MAX];
	window_utf8_to_wchar(title, titlew, TITLE_MAX);

	ctx->hwnd = CreateWindowEx(0, WINDOW_CLASS_NAME, titlew, WS_VISIBLE | style,
		x, y, width, height, NULL, NULL, ctx->instance, ctx);
	if (!ctx->hwnd) {r = LIB_ERR; goto except;}

	DragAcceptFiles(ctx->hwnd, TRUE);

	RAWINPUTDEVICE rid[3] = {0};
	// Joystick
	rid[0].usUsagePage = 0x01;
	rid[0].usUsage = 0x04;
	rid[0].hwndTarget = ctx->hwnd;

	// Gamepad
	rid[1].usUsagePage = 0x01;
	rid[1].usUsage = 0x05;
	rid[1].hwndTarget = ctx->hwnd;

	// Multi Axis
	rid[2].usUsagePage = 0x01;
	rid[2].usUsage = 0x08;
	rid[2].hwndTarget = ctx->hwnd;

	if (!RegisterRawInputDevices(rid, 3, sizeof(RAWINPUTDEVICE))) {r = LIB_ERR; goto except;}

	DXGI_SWAP_CHAIN_DESC1 sd = {0};
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.SampleDesc.Count = 1;
	sd.BufferCount = 2;
	sd.Flags = WINDOW_SWAP_CHAIN_FLAGS;

	D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
	HRESULT e = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, levels,
		sizeof(levels) / sizeof(D3D_FEATURE_LEVEL), D3D11_SDK_VERSION, &ctx->device, NULL, &ctx->context);
	if (e != S_OK) {r = LIB_ERR; goto except;}

	e = ID3D11Device_QueryInterface(ctx->device, &IID_IDXGIDevice2, &device2);
	if (e != S_OK) {r = LIB_ERR; goto except;}

	e = ID3D11Device_QueryInterface(ctx->device, &IID_IUnknown, &unknown);
	if (e != S_OK) {r = LIB_ERR; goto except;}

	e = IDXGIDevice2_GetParent(device2, &IID_IDXGIAdapter, &adapter);
	if (e != S_OK) {r = LIB_ERR; goto except;}

	e = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2, &factory2);
	if (e != S_OK) {r = LIB_ERR; goto except;}

	e = IDXGIFactory2_CreateSwapChainForHwnd(factory2, unknown, ctx->hwnd, &sd, NULL, NULL, &swap_chain1);
	if (e != S_OK) {r = LIB_ERR; goto except;}

	e = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain2, &ctx->swap_chain2);
	if (e != S_OK) {r = LIB_ERR; goto except;}

	ctx->waitable = IDXGISwapChain2_GetFrameLatencyWaitableObject(ctx->swap_chain2);
	if (!ctx->waitable) {r = LIB_ERR; goto except;}

	e = IDXGISwapChain2_SetMaximumFrameLatency(ctx->swap_chain2, 1);
	if (e != S_OK) {r = LIB_ERR; goto except;}

	e = IDXGIFactory2_MakeWindowAssociation(factory2, ctx->hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
	if (e != S_OK) {r = LIB_ERR; goto except;}

	except:

	if (swap_chain1)
		IDXGISwapChain1_Release(swap_chain1);

	if (factory2)
		IDXGIFactory2_Release(factory2);

	if (adapter)
		IDXGIAdapter_Release(adapter);

	if (unknown)
		IUnknown_Release(unknown);

	if (device2)
		IDXGIDevice2_Release(device2);

	if (r != LIB_OK)
		window_destroy(window);

	return r;
}

void window_set_title(struct window *ctx, const char *title, const char *subtitle)
{
	WCHAR titlew[TITLE_MAX];
	window_utf8_to_wchar(title, titlew, TITLE_MAX);

	WCHAR full[TITLE_MAX];

	if (subtitle) {
		WCHAR subtitlew[TITLE_MAX];
		window_utf8_to_wchar(subtitle, subtitlew, TITLE_MAX);

		_snwprintf_s(full, TITLE_MAX, _TRUNCATE, L"%s - %s", titlew, subtitlew);

	} else {
		_snwprintf_s(full, TITLE_MAX, _TRUNCATE, L"%s", titlew);
	}

	SetWindowText(ctx->hwnd, full);
}

static bool window_get_size(struct window *ctx, uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	if (GetClientRect(ctx->hwnd, &rect)) {
		*width = rect.right - rect.left;
		*height = rect.bottom - rect.top;
		return true;
	}

	return false;
}

void window_poll(struct window *ctx)
{
	for (MSG msg; PeekMessage(&msg, ctx->hwnd, 0, 0, PM_REMOVE | PM_NOYIELD);) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	uint32_t width = 0;
	uint32_t height = 0;
	if (window_get_size(ctx, &width, &height) && (width != ctx->width || height != ctx->height)) {
		IDXGISwapChain2_ResizeBuffers(ctx->swap_chain2, 0, 0, 0,
			DXGI_FORMAT_UNKNOWN, WINDOW_SWAP_CHAIN_FLAGS);
		ctx->width = width;
		ctx->height = height;
	}
}

static bool window_get_monitor_info(HWND hwnd, MONITORINFOEX *info)
{
	HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

	if (mon) {
		memset(info, 0, sizeof(MONITORINFOEX));
		info->cbSize = sizeof(MONITORINFOEX);

		return GetMonitorInfo(mon, (LPMONITORINFO) info);
	}

	return false;
}

uint32_t window_refresh_rate(struct window *ctx)
{
	MONITORINFOEX info = {0};
	if (window_get_monitor_info(ctx->hwnd, &info)) {
		DEVMODE mode = {0};
		mode.dmSize = sizeof(DEVMODE);

		if (EnumDisplaySettings(info.szDevice, ENUM_CURRENT_SETTINGS, &mode))
			return mode.dmDisplayFrequency;
	}

	return 60;
}

float window_get_dpi_scale(struct window *ctx)
{
	HMONITOR mon = MonitorFromWindow(ctx->hwnd, MONITOR_DEFAULTTONEAREST);

	if (mon) {
		UINT x = 0;
		UINT y = 0;

		if (GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y) == S_OK)
			return (float) x / 96.0f;
	}

	return 1.0f;
}

void window_set_fullscreen(struct window *ctx)
{
	MONITORINFOEX info = {0};
	if (window_get_monitor_info(ctx->hwnd, &info)) {
		uint32_t x = info.rcMonitor.left;
		uint32_t y = info.rcMonitor.top;
		uint32_t w = info.rcMonitor.right - info.rcMonitor.left;
		uint32_t h = info.rcMonitor.bottom - info.rcMonitor.top;

		SetWindowLongPtr(ctx->hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
		SetWindowPos(ctx->hwnd, HWND_TOP, x, y, w, h, SWP_FRAMECHANGED);
	}
}

void window_set_windowed(struct window *ctx, uint32_t width, uint32_t height)
{
	window_calc_client_area(&width, &height);

	int32_t x = CW_USEDEFAULT;
	int32_t y = CW_USEDEFAULT;

	MONITORINFOEX info = {0};
	if (window_get_monitor_info(ctx->hwnd, &info)) {
		x = (info.rcMonitor.right - width) / 2;
		y = (info.rcMonitor.bottom - height) / 2;
	}

	SetWindowLongPtr(ctx->hwnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW);
	SetWindowPos(ctx->hwnd, NULL, x, y, width, height, SWP_FRAMECHANGED);

	PostMessage(ctx->hwnd, WM_SETICON, ICON_BIG, GetClassLongPtr(ctx->hwnd, GCLP_HICON));
	PostMessage(ctx->hwnd, WM_SETICON, ICON_SMALL, GetClassLongPtr(ctx->hwnd, GCLP_HICONSM));
}

bool window_is_fullscreen(struct window *ctx)
{
	return GetWindowLongPtr(ctx->hwnd, GWL_STYLE) & WS_POPUP;
}

bool window_is_foreground(struct window *ctx)
{
	return GetForegroundWindow() == ctx->hwnd;
}

void window_present(struct window *ctx, uint32_t num_frames)
{
	if (WaitForSingleObjectEx(ctx->waitable, INFINITE, TRUE) == WAIT_OBJECT_0)
		IDXGISwapChain2_Present(ctx->swap_chain2, num_frames, 0);
}

OpaqueDevice *window_get_device(struct window *ctx)
{
	return ctx->device;
}

OpaqueContext *window_get_context(struct window *ctx)
{
	return ctx->context;
}

OpaqueTexture *window_get_back_buffer(struct window *ctx)
{
	ID3D11Texture2D *back_buffer = NULL;
	HRESULT e = IDXGISwapChain2_GetBuffer(ctx->swap_chain2, 0, &IID_ID3D11Texture2D, &back_buffer);

	if (e == S_OK)
		return back_buffer;

	return NULL;
}

void window_release_back_buffer(OpaqueTexture *texture)
{
	if (!texture)
		return;

	ID3D11Texture2D_Release((ID3D11Texture2D *) texture);
}

void window_render_quad(struct window *ctx, const void *image, uint32_t width,
	uint32_t height, uint32_t constrain_w, uint32_t constrain_h, float aspect_ratio,
	enum filter filter, enum effect effect)
{
	if (!ctx->quad) {
		HRESULT e = window_quad_init(ctx->device, &ctx->quad);
		if (e != S_OK) return;
	}

	ID3D11Texture2D *back_buffer = NULL;
	HRESULT e = IDXGISwapChain2_GetBuffer(ctx->swap_chain2, 0, &IID_ID3D11Texture2D, &back_buffer);

	if (e == S_OK) {
		window_quad_render(ctx->quad, ctx->device, ctx->context, image, width, height,
			constrain_w, constrain_h, back_buffer, aspect_ratio, filter, effect);
		ID3D11Texture2D_Release(back_buffer);
	}
}

void window_destroy(struct window **window)
{
	if (!window || !*window)
		return;

	struct window *ctx = *window;

	window_quad_destroy(&ctx->quad);

	if (ctx->waitable)
		CloseHandle(ctx->waitable);

	if (ctx->swap_chain2)
		IDXGISwapChain2_Release(ctx->swap_chain2);

	if (ctx->context)
		ID3D11DeviceContext_Release(ctx->context);

	if (ctx->device)
		ID3D11Device_Release(ctx->device);

	if (ctx->hwnd)
		DestroyWindow(ctx->hwnd);

	if (ctx->instance && ctx->class != 0)
		UnregisterClass(WINDOW_CLASS_NAME, ctx->instance);

	free(ctx);
	*window = NULL;
}
