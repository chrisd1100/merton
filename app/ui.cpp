#include "ui.h"

#include "deps/imgui/imgui_draw.cpp"
#include "deps/imgui/imgui_widgets.cpp"
#include "deps/imgui/imgui.cpp"

#if defined(_WIN32)
	#include "deps/imgui/imgui_impl_dx11.cpp"
#endif

using namespace ImGui;


/*** FRAMEWORK ***/

struct ui {
	bool init;
	bool impl_init;
	int64_t ts;
	ImDrawData *draw_data;
	OpaqueDevice *device;
	OpaqueContext *context;
	OpaqueTexture *texture;
	float width;
	float height;
	bool mouse[3];
} UI;

void ui_create(void)
{
	if (UI.init)
		return;

	CreateContext();
	ImGuiIO &io = GetIO();

	io.KeyMap[ImGuiKey_Tab] = SCANCODE_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = SCANCODE_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = SCANCODE_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = SCANCODE_UP;
	io.KeyMap[ImGuiKey_DownArrow] = SCANCODE_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = SCANCODE_PAGEUP;
	io.KeyMap[ImGuiKey_PageDown] = SCANCODE_PAGEDOWN;
	io.KeyMap[ImGuiKey_Home] = SCANCODE_HOME;
	io.KeyMap[ImGuiKey_End] = SCANCODE_END;
	io.KeyMap[ImGuiKey_Insert] = SCANCODE_INSERT;
	io.KeyMap[ImGuiKey_Delete] = SCANCODE_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = SCANCODE_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = SCANCODE_ENTER;
	io.KeyMap[ImGuiKey_Enter] = SCANCODE_ENTER;
	io.KeyMap[ImGuiKey_Escape] = SCANCODE_ESCAPE;
	io.KeyMap[ImGuiKey_A] = SCANCODE_A;
	io.KeyMap[ImGuiKey_C] = SCANCODE_C;
	io.KeyMap[ImGuiKey_V] = SCANCODE_V;
	io.KeyMap[ImGuiKey_X] = SCANCODE_X;
	io.KeyMap[ImGuiKey_Y] = SCANCODE_Y;
	io.KeyMap[ImGuiKey_Z] = SCANCODE_Z;

	io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.IniFilename = NULL;
	io.LogFilename = NULL;

	UI.init = true;
}

void ui_input(struct window_msg *wmsg)
{
	ImGuiIO &io = GetIO();

	switch (wmsg->type) {
		case WINDOW_MSG_MOUSE_WHEEL:
			if (wmsg->mouseWheel.y > 0) io.MouseWheel += 1;
			if (wmsg->mouseWheel.y < 0) io.MouseWheel -= 1;
			break;

		case WINDOW_MSG_MOUSE_BUTTON:
			if (wmsg->mouseButton.button == MOUSE_L)      UI.mouse[0] = wmsg->mouseButton.pressed;
			if (wmsg->mouseButton.button == MOUSE_R)      UI.mouse[1] = wmsg->mouseButton.pressed;
			if (wmsg->mouseButton.button == MOUSE_MIDDLE) UI.mouse[2] = wmsg->mouseButton.pressed;

			io.MouseDown[0] = io.MouseDown[0] || UI.mouse[0];
			io.MouseDown[1] = io.MouseDown[1] || UI.mouse[1];
			io.MouseDown[2] = io.MouseDown[2] || UI.mouse[2];
			break;

		case WINDOW_MSG_MOUSE_MOTION:
			if (!wmsg->mouseMotion.relative)
				io.MousePos = ImVec2((float) wmsg->mouseMotion.x * 1.0f, (float) wmsg->mouseMotion.y * 1.0f);
			break;
	}
}

static void ui_impl_destroy(void)
{
	ImGui_ImplDX11_Shutdown();
}

static bool ui_impl_init(OpaqueDevice *device, OpaqueContext *context)
{
	bool r = device != NULL && context != NULL &&
		ImGui_ImplDX11_Init((ID3D11Device *) device, (ID3D11DeviceContext *) context) &&
		ImGui_ImplDX11_CreateDeviceObjects();

	if (!r || !GetIO().Fonts->TexID) {
		ui_impl_destroy();
		r = false;
	}

	return r;
}

bool ui_begin(OpaqueDevice *device, OpaqueContext *context, OpaqueTexture *texture)
{
	if (device != UI.device || context != UI.context) {
		if (UI.impl_init) {
			ui_impl_destroy();
			UI.impl_init = false;
		}

		UI.device = NULL;
		UI.context = NULL;

		if (!ui_impl_init(device, context))
			return false;

		GetIO().Fonts->ClearTexData();
		UI.impl_init = true;
		UI.device = device;
		UI.context = context;
	}

	UI.texture = texture;
	ID3D11Texture2D *d3d11texture = (ID3D11Texture2D *) texture;

	D3D11_TEXTURE2D_DESC desc = {0};
	d3d11texture->GetDesc(&desc);

	UI.width = (float) desc.Width;
	UI.height = (float) desc.Height;

	return true;
}

void ui_draw(void (*callback)(void *opaque), const void *opaque)
{
	ImGuiIO &io = GetIO();
	io.DisplaySize = ImVec2(UI.width, UI.height);

	int64_t now = time_stamp();

	if (UI.ts != 0)
		io.DeltaTime = (float) time_diff(UI.ts, now) / 1000.0f;

	UI.ts = now;

	NewFrame();
	callback((void *) opaque);
	Render();

	UI.draw_data = GetDrawData();

	io.MouseDown[0] = UI.mouse[0];
	io.MouseDown[1] = UI.mouse[1];
	io.MouseDown[2] = UI.mouse[2];
}

void ui_render(bool clear)
{
	if (!UI.device || !UI.context)
		return;

	ID3D11RenderTargetView *rtv = NULL;
	ID3D11Device *device = (ID3D11Device *) UI.device;
	HRESULT e = device->CreateRenderTargetView((ID3D11Texture2D *) UI.texture, NULL, &rtv);

	if (e == S_OK) {
		ID3D11DeviceContext *context = (ID3D11DeviceContext *) UI.context;

		if (clear) {
			FLOAT clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
			context->ClearRenderTargetView(rtv, clear_color);
		}

		context->OMSetRenderTargets(1, &rtv, NULL);

		ImGui_ImplDX11_RenderDrawData(UI.draw_data);
		rtv->Release();
	}
}

void ui_destroy(void)
{
	if (!UI.init)
		return;

	ui_impl_destroy();
	DestroyContext();

	memset(&UI, 0, sizeof(struct ui));
}


/*** COMPONENTS ***/

#define WINDOW_MARGIN_L   30.0f
#define WINDOW_MARGIN_TOP 70.0f
#define POPUP_MARGIN_TOP  30.0f
#define POPUP_MESSAGE_LEN 1024

enum nav {
	NAV_NONE     = 0,
	NAV_OPEN_ROM = 1,
};

struct {
	enum nav nav;
} CMP;

static void ui_open_rom(struct ui_args *args)
{
	args;

	SetNextWindowPos(ImVec2(WINDOW_MARGIN_L, WINDOW_MARGIN_TOP));
	SetNextWindowSize(ImVec2(400.0f, 550.0f));

	if (Begin("OPEN_ROM", NULL, ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings)) {

		if (Button("CLOSE DIALOG"))
			CMP.nav = NAV_NONE;

		/*
		if (ctx->refresh_dir) {
			free(ctx->fi);
			ctx->fi_n = fs_list(ctx->dir, &ctx->fi);
			ctx->refresh_dir = false;
		}

		for (uint32_t x = 0; x < ctx->fi_n; x++) {
			if (Selectable(ctx->fi[x].name)) {
				if (ctx->fi[x].dir) {
					fs_path(ctx->dir, ctx->dir, ctx->fi[x].name);
					ctx->refresh_dir = true;

				} else {
					ctx->cbs.open(ctx->dir, ctx->fi[x].name, ctx->opaque);
					ctx->open_rom = false;
				}
			}
		}
		*/

		End();
	}
}

void ui_root(struct ui_args *args)
{
	args;

	//PushStyleColor(ImGuiCol_Separator, WHITE15);
	//PushStyleColor(ImGuiCol_SeparatorActive, WHITE15);
	//PushStyleColor(ImGuiCol_SeparatorHovered, WHITE15);
	//PushStyleColor(ImGuiCol_ScrollbarGrab, mobile ? TRANS : WHITE15);
	//PushStyleColor(ImGuiCol_ScrollbarGrabHovered, mobile ? TRANS : WHITE15);
	//PushStyleColor(ImGuiCol_ScrollbarGrabActive, mobile ? TRANS : WHITE15);
	//PushStyleColor(ImGuiCol_ScrollbarBg, TRANS);
	PushStyleColor(ImGuiCol_Text, 0xFFDDDDDD);
	PushStyleColor(ImGuiCol_WindowBg, 0xFF333333);
	PushStyleColor(ImGuiCol_MenuBarBg, 0xFF333333);
	PushStyleColor(ImGuiCol_FrameBg, 0xFF333333);
	PushStyleColor(ImGuiCol_Header, 0xFF333333);
	PushStyleColor(ImGuiCol_HeaderHovered, 0xFF333333);
	PushStyleColor(ImGuiCol_HeaderActive, 0xFF333333);
	//PushStyleVar(ImGuiStyleVar_ScrollbarSize, mobile ? 1.0f : X(12));
	//PushStyleVar(ImGuiStyleVar_ScrollbarRounding, X(4));
	PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
	PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0);
	PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	//PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	//PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
	//PushStyleVar(ImGuiStyleVar_IndentSpacing, 0);

	if (BeginMainMenuBar()) {
		if (BeginMenu("FILE", true)) {
			if (MenuItem("OPEN ROM", "CTRL+O"))
				CMP.nav = NAV_OPEN_ROM;

			Separator();

			if (MenuItem("QUIT"))
				printf("QUIT\n");

			ImGui::EndMenu();
		}

		if (BeginMenu("CONSOLE", true)) {
			if (MenuItem("RESET", "CTRL+R"))
				NES_Reset(args->nes, false);

			if (MenuItem("POWER CYCLE", "CTRL+T"))
				NES_Reset(args->nes, true);

			ImGui::EndMenu();
		}

		EndMainMenuBar();
	}

	if (CMP.nav == NAV_OPEN_ROM)
		ui_open_rom(args);

	PopStyleVar(4);
	PopStyleColor(7);
}
