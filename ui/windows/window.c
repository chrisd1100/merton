#include "lib.h"

#include <stdlib.h>

#include <windows.h>
#include <shellapi.h>

#define COBJMACROS
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

	ID3D11Device *device;
	ID3D11DeviceContext *context;
	IDXGISwapChain2 *swap_chain2;
	HANDLE waitable;
	bool first;

	struct window_quad *quad;
};


/*** WINDOW ***/

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
		case WM_SIZE:
			if (lparam > 0) {
				struct window *ctx = (struct window *) GetWindowLongPtr(hwnd, 0);

				if (ctx && ctx->swap_chain2)
					IDXGISwapChain2_ResizeBuffers(ctx->swap_chain2, 0, 0, 0, DXGI_FORMAT_UNKNOWN,
						WINDOW_SWAP_CHAIN_FLAGS);
			}
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
			break;
		case WM_INPUT:
			UINT size = 0;

			GetRawInputData((HRAWINPUT) lparam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
			RAWINPUT *ri = malloc(size);

			if (GetRawInputData((HRAWINPUT) lparam, RID_INPUT, ri, &size, sizeof(RAWINPUTHEADER))) {
				if (ri->header.dwType == RIM_TYPEHID && ri->data.hid.dwCount == 1 && ri->data.hid.dwSizeHid >= 8) {
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

static void window_utf8_to_wchar(const char *src, WCHAR *dst, size_t dst_len)
{
	int32_t n = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, (int32_t) dst_len);

	if (n != strlen(src) + 1)
		memset(dst, 0, dst_len * sizeof(WCHAR));
}

enum lib_status window_create(const char *title, WINDOW_MSG_FUNC msg_func, const void *opaque,
	uint32_t width, uint32_t height, struct window **window)
{
	enum lib_status r = LIB_OK;

	struct window *ctx = *window = calloc(1, sizeof(struct window));
	ctx->msg_func = msg_func;
	ctx->opaque = opaque;
	ctx->first = true;

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
	rect.right = width;
	rect.bottom = height;
	if (AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0)) {
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}

	HWND desktop = GetDesktopWindow();
	int32_t x = CW_USEDEFAULT;
	int32_t y = CW_USEDEFAULT;
	if (desktop && GetWindowRect(desktop, &rect)) {
		x = (rect.right - width) / 2;
		y = (rect.bottom - height) / 2;
	}

	WCHAR titlew[TITLE_MAX];
	window_utf8_to_wchar(title, titlew, TITLE_MAX);

	ctx->hwnd = CreateWindowEx(0, WINDOW_CLASS_NAME, titlew, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		x, y, width, height, NULL, NULL, ctx->instance, ctx);
	if (!ctx->hwnd) {r = LIB_ERR; goto except;}

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

void window_poll(struct window *ctx)
{
	for (MSG msg; PeekMessage(&msg, ctx->hwnd, 0, 0, PM_REMOVE | PM_NOYIELD);) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

uint32_t window_refresh_rate(struct window *ctx)
{
	HMONITOR mon = MonitorFromWindow(ctx->hwnd, MONITOR_DEFAULTTONEAREST);

	if (mon) {
		MONITORINFOEX info = {0};
		info.cbSize = sizeof(MONITORINFOEX);

		if (GetMonitorInfo(mon, (LPMONITORINFO) &info)) {
			DEVMODE mode = {0};
			mode.dmSize = sizeof(DEVMODE);

			if (EnumDisplaySettings(info.szDevice, ENUM_CURRENT_SETTINGS, &mode))
				return mode.dmDisplayFrequency;
		}
	}

	return 60;
}

bool window_is_foreground(struct window *ctx)
{
	return GetForegroundWindow() == ctx->hwnd;
}

void window_present(struct window *ctx, uint32_t num_frames)
{
	// Microsoft says this needs to be called in advance of the first frame
	// https://docs.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains
	if (ctx->first) {
		WaitForSingleObjectEx(ctx->waitable, INFINITE, TRUE);
		ctx->first = false;
	}

	IDXGISwapChain2_Present(ctx->swap_chain2, num_frames, 0);
	WaitForSingleObjectEx(ctx->waitable, INFINITE, TRUE);
}

void window_render_quad(struct window *ctx, const void *image, uint32_t width,
	uint32_t height, float aspect_ratio)
{
	if (!ctx->quad) {
		HRESULT e = window_quad_init(ctx->device, &ctx->quad);
		if (e != S_OK) return;
	}

	ID3D11Texture2D *back_buffer = NULL;
	HRESULT e = IDXGISwapChain2_GetBuffer(ctx->swap_chain2, 0, &IID_ID3D11Texture2D, &back_buffer);

	if (e == S_OK) {
		window_quad_render(ctx->quad, ctx->device, ctx->context, image, width, height,
			back_buffer, aspect_ratio);
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
