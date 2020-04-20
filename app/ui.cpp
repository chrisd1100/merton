#include "ui.h"

#include "deps/imgui/imgui_draw.cpp"
#include "deps/imgui/imgui_widgets.cpp"
#include "deps/imgui/imgui.cpp"

#if defined(_WIN32)
	#include "deps/imgui/imgui_impl_dx11.cpp"
#endif

#include "assets/font/ponderosa.h"

using namespace ImGui;


/*** FRAMEWORK ***/

#define X(v) (UI.dpi_scale * (float) (v))
#define VEC(x, y) ImVec2(X(x), X(y))

struct ui {
	bool init;
	bool impl_init;
	int64_t ts;
	ImDrawData *draw_data;
	OpaqueDevice *device;
	OpaqueContext *context;
	OpaqueTexture *texture;
	float dpi_scale;
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
				io.MousePos = ImVec2((float) wmsg->mouseMotion.x, (float) wmsg->mouseMotion.y);
			break;

		case WINDOW_MSG_KEYBOARD: {
			enum scancode sc = wmsg->keyboard.scancode;

			if (wmsg->keyboard.pressed && sc < IM_ARRAYSIZE(io.KeysDown))
				io.KeysDown[sc] = true;

			if (sc == SCANCODE_LSHIFT || sc == SCANCODE_RSHIFT)
				io.KeyShift = wmsg->keyboard.pressed;

			if (sc == SCANCODE_LCTRL || sc == SCANCODE_RCTRL)
				io.KeyCtrl =  wmsg->keyboard.pressed;

			if (sc == SCANCODE_LALT || sc == SCANCODE_RALT)
				io.KeyAlt = wmsg->keyboard.pressed;

			if (sc == SCANCODE_LGUI || sc == SCANCODE_RGUI)
				io.KeySuper = wmsg->keyboard.pressed;

			break;
		}
		default:
			break;
	}
}

static void ui_impl_destroy(void)
{
	#if defined(_WIN32)
		ImGui_ImplDX11_Shutdown();
	#elif defined(__APPLE__)
	#endif
}

static bool ui_impl_init(OpaqueDevice *device, OpaqueContext *context)
{
	#if defined(_WIN32)
		bool r = device != NULL && context != NULL &&
			ImGui_ImplDX11_Init((ID3D11Device *) device, (ID3D11DeviceContext *) context) &&
			ImGui_ImplDX11_CreateDeviceObjects();
	#elif defined(__APPLE__)
		bool r = false;
	#endif

	if (!r || !GetIO().Fonts->TexID) {
		ui_impl_destroy();
		r = false;
	}

	return r;
}

bool ui_begin(float dpi_scale, OpaqueDevice *device, OpaqueContext *context, OpaqueTexture *texture)
{
	if (device != UI.device || context != UI.context || dpi_scale != UI.dpi_scale) {
		if (UI.impl_init) {
			ui_impl_destroy();
			UI.impl_init = false;
		}

		UI.dpi_scale = dpi_scale;

		UI.device = NULL;
		UI.context = NULL;

		ImGuiIO &io = GetIO();
		io.Fonts->AddFontFromMemoryCompressedTTF(ponderosa_compressed_data,
			ponderosa_compressed_size, X(12));

		if (!ui_impl_init(device, context))
			return false;

		io.Fonts->ClearTexData();
		UI.impl_init = true;
		UI.device = device;
		UI.context = context;
	}

	UI.texture = texture;

	#if defined(_WIN32)
		ID3D11Texture2D *d3d11texture = (ID3D11Texture2D *) texture;

		D3D11_TEXTURE2D_DESC desc = {0};
		d3d11texture->GetDesc(&desc);

		UI.width = (float) desc.Width;
		UI.height = (float) desc.Height;
	#elif defined(__APPLE__)
	#endif

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

	for (uint32_t x = 0; x < IM_ARRAYSIZE(io.KeysDown); x++)
		io.KeysDown[x] = false;
}

void ui_render(bool clear)
{
	if (!UI.device || !UI.context)
		return;

	#if defined(_WIN32)
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
	#elif defined(__APPLE__)
	#endif
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

#define COLOR_TEXT    0xFFDDDDDD
#define COLOR_LABEL   0xFF999999
#define COLOR_BUTTON  0xFF444444
#define COLOR_BORDER  0xF6333333
#define COLOR_DARK_BG 0xF6222222
#define COLOR_HOVER   0xF6666666

#define PACK_ASPECT(x, y) (((x) << 8) | (y))

enum nav {
	NAV_NONE     = 0x0000,
	NAV_MENU     = 0x0100,
	NAV_OPEN_ROM = 0x0001,
};

struct component_state {
	uint32_t nav;
	struct finfo *fi;
	uint32_t fi_n;
	bool refreshed;
	const char *dir;
} CMP;

static void ui_open_rom(struct ui_event *event)
{
	ImGuiIO &io = GetIO();

	float padding_h = X(18);
	float padding_v = X(30);
	SetNextWindowPos(ImVec2(padding_h, padding_v + X(14)));
	SetNextWindowSize(ImVec2(io.DisplaySize.x - padding_h * 2.0f, io.DisplaySize.y - padding_v * 2.0f));

	if (Begin("OPEN_ROM", NULL, ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings)) {

		if (Button("Close"))
			CMP.nav = NAV_NONE;

		if (!CMP.refreshed) {
			struct finfo *fi = NULL;
			uint32_t n = fs_list(CMP.dir ? CMP.dir : ".", &fi);

			fs_free_list(&CMP.fi, CMP.fi_n);
			CMP.fi = fi;
			CMP.fi_n = n;

			CMP.refreshed = true;
		}

		for (uint32_t x = 0; x < CMP.fi_n; x++) {
			if (Selectable(CMP.fi[x].name)) {
				if (CMP.fi[x].dir) {
					CMP.dir = CMP.fi[x].path;
					CMP.refreshed = false;

				} else {
					event->type = UI_EVENT_OPEN_ROM;
					event->rom_name = CMP.fi[x].path;
					CMP.nav = NAV_NONE;
				}
			}
		}

		End();
	}
}

static void ui_menu(const struct ui_args *args, struct ui_event *event)
{
	if (BeginMainMenuBar()) {
		if (BeginMenu("System", true)) {
			if (MenuItem("Load ROM", "Ctrl+O"))
				CMP.nav ^= NAV_OPEN_ROM;

			if (MenuItem("Unload ROM"))
				NES_LoadCart(args->nes, NULL, 0, NULL, 0, NULL);

			Separator();

			if (MenuItem(args->paused ? "Unpause" : "Pause", "Ctrl+P"))
				event->type = UI_EVENT_PAUSE;

			if (MenuItem("Reset", "Ctrl+R"))
				NES_Reset(args->nes, false);

			if (MenuItem("Power Cycle", "Ctrl+T"))
				NES_Reset(args->nes, true);

			Separator();

			if (MenuItem("Background Pause", "", args->cfg->bg_pause))
				event->cfg.bg_pause = !event->cfg.bg_pause;

			Separator();

			if (MenuItem("Quit"))
				event->type = UI_EVENT_QUIT;

			ImGui::EndMenu();
		}

		if (BeginMenu("Video", true)) {
			if (BeginMenu("Window", true)) {
				if (MenuItem("Fullscreen", "Ctrl+W", args->cfg->fullscreen, true))
					event->cfg.fullscreen = !event->cfg.fullscreen;

				MenuItem("Reset Size");
				ImGui::EndMenu();
			}
			if (BeginMenu("Frame Size", true)) {
				if (MenuItem("1x", "", args->cfg->frame_size == 1, true))
					event->cfg.frame_size = 1;

				if (MenuItem("2x", "", args->cfg->frame_size == 2, true))
					event->cfg.frame_size = 2;

				if (MenuItem("3x", "", args->cfg->frame_size == 3, true))
					event->cfg.frame_size = 3;

				if (MenuItem("4x", "", args->cfg->frame_size == 4, true))
					event->cfg.frame_size = 4;

				if (MenuItem("Fill", "", args->cfg->frame_size == 0, true))
					event->cfg.frame_size = 0;

				ImGui::EndMenu();
			}

			uint32_t aspect = PACK_ASPECT(args->cfg->aspect_ratio.x, args->cfg->aspect_ratio.y);

			if (BeginMenu("Aspect Ratio", true)) {
				if (MenuItem("127:105", "", aspect == PACK_ASPECT(127, 105), true))
					event->cfg.aspect_ratio.x = 127, event->cfg.aspect_ratio.y = 105;

				if (MenuItem("16:15", "", aspect == PACK_ASPECT(16, 15), true))
					event->cfg.aspect_ratio.x = 16, event->cfg.aspect_ratio.y = 15;

				if (MenuItem("8:7", "", aspect == PACK_ASPECT(8, 7), true))
					event->cfg.aspect_ratio.x = 8, event->cfg.aspect_ratio.y = 7;

				if (MenuItem("4:3", "", aspect == PACK_ASPECT(4, 3), true))
					event->cfg.aspect_ratio.x = 4, event->cfg.aspect_ratio.y = 3;

				ImGui::EndMenu();
			}
			if (BeginMenu("Filter", true)) {
				if (MenuItem("Nearest", "", args->cfg->filter == FILTER_NEAREST, true))
					event->cfg.filter = FILTER_NEAREST;

				if (MenuItem("Linear", "", args->cfg->filter == FILTER_LINEAR, true))
					event->cfg.filter = FILTER_LINEAR;

				ImGui::EndMenu();
			}
			if (BeginMenu("Shader", true)) {
				MenuItem("None", "", true, true);
				ImGui::EndMenu();
			}
			if (BeginMenu("Clear Overscan", true)) {
				if (MenuItem("Top", "", args->cfg->overscan.top, true))
					event->cfg.overscan.top = !event->cfg.overscan.top;

				if (MenuItem("Right", "", args->cfg->overscan.right, true))
					event->cfg.overscan.right = !event->cfg.overscan.right;

				if (MenuItem("Bottom", "", args->cfg->overscan.bottom, true))
					event->cfg.overscan.bottom = !event->cfg.overscan.bottom;

				if (MenuItem("Left", "", args->cfg->overscan.left, true))
					event->cfg.overscan.left = !event->cfg.overscan.left;

				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (BeginMenu("Audio", true)) {
			if (MenuItem(args->cfg->mute ? "Unmute" : "Mute", "Ctrl+M"))
				event->cfg.mute = !event->cfg.mute;

			if (MenuItem("Stereo", "", args->cfg->stereo, true)) {
				event->cfg.stereo = !event->cfg.stereo;
				NES_SetStereo(args->nes, event->cfg.stereo);
			}

			if (BeginMenu("Sample Rate", true)) {
				int32_t sample_rate = 0;

				if (MenuItem("48000", "", args->cfg->sample_rate == 48000, true))
					sample_rate = 48000;

				if (MenuItem("44100", "", args->cfg->sample_rate == 44100, true))
					sample_rate = 44100;

				if (MenuItem("22050", "", args->cfg->sample_rate == 22050, true))
					sample_rate = 22050;

				if (MenuItem("16000", "", args->cfg->sample_rate == 16000, true))
					sample_rate = 16000;

				if (MenuItem("11025", "", args->cfg->sample_rate == 11025, true))
					sample_rate = 11025;

				if (MenuItem("8000", "", args->cfg->sample_rate == 8000, true))
					sample_rate = 8000;

				if (sample_rate != 0) {
					event->cfg.sample_rate = sample_rate;
					NES_SetSampleRate(args->nes, sample_rate);
				}

				ImGui::EndMenu();
			}

			if (BeginMenu("Channels", true)) {
				uint32_t channels = event->cfg.channels;

				if (MenuItem("Square 1", "", args->cfg->channels & NES_CHANNEL_PULSE_0, true))
					event->cfg.channels ^= NES_CHANNEL_PULSE_0;

				if (MenuItem("Square 2", "", args->cfg->channels & NES_CHANNEL_PULSE_1, true))
					event->cfg.channels ^= NES_CHANNEL_PULSE_1;

				if (MenuItem("Triangle", "", args->cfg->channels & NES_CHANNEL_TRIANGLE, true))
					event->cfg.channels ^= NES_CHANNEL_TRIANGLE;

				if (MenuItem("Noise", "", args->cfg->channels & NES_CHANNEL_NOISE, true))
					event->cfg.channels ^= NES_CHANNEL_NOISE;

				if (MenuItem("DMC", "", args->cfg->channels & NES_CHANNEL_DMC, true))
					event->cfg.channels ^= NES_CHANNEL_DMC;

				if (MenuItem("Mapper 1", "", args->cfg->channels & NES_CHANNEL_EXT_0, true))
					event->cfg.channels ^= NES_CHANNEL_EXT_0;

				if (MenuItem("Mapper 2", "", args->cfg->channels & NES_CHANNEL_EXT_1, true))
					event->cfg.channels ^= NES_CHANNEL_EXT_1;

				if (MenuItem("Mapper 3", "", args->cfg->channels & NES_CHANNEL_EXT_2, true))
					event->cfg.channels ^= NES_CHANNEL_EXT_2;

				if (channels != event->cfg.channels)
					NES_SetChannels(args->nes, event->cfg.channels);

				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		EndMainMenuBar();
	}
}

void ui_component_root(const struct ui_args *args,
	void (*event_callback)(struct ui_event *event, void *opaque), const void *opaque)
{
	ImGuiIO &io = GetIO();

	struct ui_event event = {};
	event.type = UI_EVENT_NONE;
	event.cfg = *args->cfg;

	PushStyleColor(ImGuiCol_Separator,        COLOR_BORDER);
	PushStyleColor(ImGuiCol_SeparatorActive,  COLOR_BORDER);
	PushStyleColor(ImGuiCol_SeparatorHovered, COLOR_BORDER);
	PushStyleColor(ImGuiCol_Border,           COLOR_BORDER);
	PushStyleColor(ImGuiCol_Text,             COLOR_TEXT);
	PushStyleColor(ImGuiCol_TextDisabled,     COLOR_LABEL);
	PushStyleColor(ImGuiCol_WindowBg,         COLOR_DARK_BG);
	PushStyleColor(ImGuiCol_PopupBg,          COLOR_DARK_BG);
	PushStyleColor(ImGuiCol_MenuBarBg,        COLOR_DARK_BG);
	PushStyleColor(ImGuiCol_FrameBg,          COLOR_DARK_BG);
	PushStyleColor(ImGuiCol_FrameBgHovered,   COLOR_HOVER);
	PushStyleColor(ImGuiCol_FrameBgActive,    COLOR_DARK_BG);
	PushStyleColor(ImGuiCol_Header,           COLOR_DARK_BG);
	PushStyleColor(ImGuiCol_HeaderHovered,    COLOR_HOVER);
	PushStyleColor(ImGuiCol_HeaderActive,     COLOR_DARK_BG);
	PushStyleColor(ImGuiCol_Button,           COLOR_BUTTON);
	PushStyleColor(ImGuiCol_ButtonHovered,    COLOR_HOVER);
	PushStyleColor(ImGuiCol_ButtonActive,     COLOR_BUTTON);

	PushStyleVar(ImGuiStyleVar_ScrollbarSize,    X(12));
	PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);
	PushStyleVar(ImGuiStyleVar_ChildBorderSize,  1);
	PushStyleVar(ImGuiStyleVar_PopupBorderSize,  1);
	PushStyleVar(ImGuiStyleVar_WindowRounding,   0);
	PushStyleVar(ImGuiStyleVar_ItemSpacing,      VEC(10, 8));
	PushStyleVar(ImGuiStyleVar_FramePadding,     VEC(10, 6));
	PushStyleVar(ImGuiStyleVar_WindowPadding,    VEC(10, 10));

	if (io.KeysDown[SCANCODE_ESCAPE])
		CMP.nav ^= NAV_MENU;

	if (!(CMP.nav & NAV_MENU))
		CMP.nav = NAV_NONE;

	if (CMP.nav & NAV_MENU)
		ui_menu(args, &event);

	if (CMP.nav & NAV_OPEN_ROM)
		ui_open_rom(&event);

	PopStyleVar(8);
	PopStyleColor(18);

	if (io.KeysDown[SCANCODE_W] && io.KeyCtrl)
		event.cfg.fullscreen = !event.cfg.fullscreen;

	if (memcmp(args->cfg, &event.cfg, sizeof(struct config)))
		event.type = UI_EVENT_CONFIG;

	if (event.type != UI_EVENT_NONE)
		event_callback(&event, (void *) opaque);
}

void ui_component_destroy(void)
{
	fs_free_list(&CMP.fi, CMP.fi_n);
	CMP.fi_n = 0;

	memset(&CMP, 0, sizeof(struct component_state));
}
