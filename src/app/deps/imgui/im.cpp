#include "im.h"

#include "imgui_draw.cpp"
#include "imgui_widgets.cpp"
#include "imgui.cpp"

#if defined(_WIN32)
	#include "impl/im-dx11.h"
#else
	#include "impl/im-mtl.h"
#endif

using namespace ImGui;


// Framework

static struct im {
	bool init;
	bool impl_init;
	int64_t ts;
	#if defined(_WIN32)
	struct im_dx11 *dx11;
	#elif defined(__APPLE__)
	struct im_mtl *mtl;
	#endif
	struct im_draw_data draw_data;
	OpaqueDevice *device;
	OpaqueContext *context;
	OpaqueTexture *texture;
	float dpi_scale;
	float width;
	float height;
	bool mouse[3];
	void *font;
	size_t font_size;
	float font_height;
} IM;

void im_create(const void *font, size_t font_size, float font_height)
{
	if (IM.init)
		return;

	IM.font = malloc(font_size);
	memcpy(IM.font, font, font_size);
	IM.font_size = font_size;
	IM.font_height = font_height;

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

	IM.init = true;
}

void im_input(struct window_msg *wmsg)
{
	ImGuiIO &io = GetIO();

	switch (wmsg->type) {
		case WINDOW_MSG_MOUSE_WHEEL:
			if (wmsg->mouseWheel.y > 0) io.MouseWheel += 1;
			if (wmsg->mouseWheel.y < 0) io.MouseWheel -= 1;
			break;

		case WINDOW_MSG_MOUSE_BUTTON:
			if (wmsg->mouseButton.button == MOUSE_L)      IM.mouse[0] = wmsg->mouseButton.pressed;
			if (wmsg->mouseButton.button == MOUSE_R)      IM.mouse[1] = wmsg->mouseButton.pressed;
			if (wmsg->mouseButton.button == MOUSE_MIDDLE) IM.mouse[2] = wmsg->mouseButton.pressed;

			io.MouseDown[0] = io.MouseDown[0] || IM.mouse[0];
			io.MouseDown[1] = io.MouseDown[1] || IM.mouse[1];
			io.MouseDown[2] = io.MouseDown[2] || IM.mouse[2];
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

static void im_impl_destroy(void)
{
	#if defined(_WIN32)
		im_dx11_destroy(&IM.dx11);
	#elif defined(__APPLE__)
		im_mtl_destroy(&IM.mtl);
	#endif
}

static bool im_impl_init(OpaqueDevice *device, OpaqueContext *context)
{
	ImGuiIO &io = GetIO();

	uint8_t *pixels = NULL;
	int32_t width = 0, height = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	#if defined(_WIN32)
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		bool r = device != NULL && context != NULL &&
			im_dx11_create((ID3D11Device *) device, pixels, width, height, &IM.dx11);
	#elif defined(__APPLE__)
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

		bool r = device != NULL && im_mtl_create(device, pixels, width, height, &IM.mtl);
	#endif

	if (!r) {
		im_impl_destroy();
		r = false;
	}

	return r;
}

bool im_begin(float dpi_scale, OpaqueDevice *device, OpaqueContext *context, OpaqueTexture *texture)
{
	if (device != IM.device || context != IM.context || dpi_scale != IM.dpi_scale) {
		if (IM.impl_init) {
			im_impl_destroy();
			IM.impl_init = false;
		}

		IM.dpi_scale = dpi_scale;

		IM.device = NULL;
		IM.context = NULL;

		ImGuiIO &io = GetIO();
		io.Fonts->AddFontFromMemoryCompressedTTF(IM.font, (int32_t) IM.font_size,
			im_dpi_scale() * IM.font_height);

		if (!im_impl_init(device, context))
			return false;

		io.Fonts->ClearTexData();
		IM.impl_init = true;
		IM.device = device;
		IM.context = context;
	}

	#if defined(_WIN32)
		IM.texture = texture;
		ID3D11Texture2D *d3d11texture = (ID3D11Texture2D *) texture;

		D3D11_TEXTURE2D_DESC desc = {0};
		d3d11texture->GetDesc(&desc);

		IM.width = (float) desc.Width;
		IM.height = (float) desc.Height;

	#elif defined(__APPLE__)
		IM.texture = im_mtl_get_drawable_texture(texture); // this is an id<CAMetalDrawable>
		im_mtl_texture_size(IM.texture, &IM.width, &IM.height);
	#endif

	return true;
}

static bool im_copy_draw_data(struct im_draw_data *dd, ImDrawData *idd)
{
	struct im_draw_data pdd = *dd;

	dd->vtx_len = idd->TotalVtxCount;
	dd->idx_len = idd->TotalIdxCount;

	dd->display_size.x = idd->DisplaySize.x;
	dd->display_size.y = idd->DisplaySize.y;
	dd->display_pos.x = idd->DisplayPos.x;
	dd->display_pos.y = idd->DisplayPos.y;
	dd->framebuffer_scale.x = idd->FramebufferScale.x;
	dd->framebuffer_scale.y = idd->FramebufferScale.y;

	// Command Lists
	if ((uint32_t) idd->CmdListsCount > dd->cmd_list_max_len) {
		dd->cmd_list = (struct im_cmd_list *) realloc(dd->cmd_list, idd->CmdListsCount * sizeof(struct im_cmd_list));
		memset(dd->cmd_list + dd->cmd_list_max_len, 0,
			(idd->CmdListsCount - dd->cmd_list_max_len) * sizeof(struct im_cmd_list));
		dd->cmd_list_max_len = idd->CmdListsCount;
	}
	dd->cmd_list_len = idd->CmdListsCount;

	bool diff = memcmp(dd, &pdd, sizeof(struct im_draw_data));

	for (uint32_t x = 0; x < dd->cmd_list_len; x++) {
		struct im_cmd_list *cmd = &dd->cmd_list[x];
		struct im_cmd_list pcmd = *cmd;
		ImDrawList *icmd = idd->CmdLists[x];

		// Index Buffer
		if ((uint32_t) icmd->IdxBuffer.Size > cmd->idx_max_len) {
			cmd->idx = (uint16_t *) realloc(cmd->idx, icmd->IdxBuffer.Size * sizeof(uint16_t));
			cmd->idx_max_len = icmd->IdxBuffer.Size;
		}
		cmd->idx_len = icmd->IdxBuffer.Size;

		for (uint32_t y = 0; y < cmd->idx_len; y++) {
			diff = diff || cmd->idx[y] != icmd->IdxBuffer[y];
			cmd->idx[y] = icmd->IdxBuffer[y];
		}

		// Vertex Buffer
		if ((uint32_t) icmd->VtxBuffer.Size > cmd->vtx_max_len) {
			cmd->vtx = (struct im_vtx *) realloc(cmd->vtx, icmd->VtxBuffer.Size * sizeof(struct im_vtx));
			cmd->vtx_max_len = icmd->VtxBuffer.Size;
		}
		cmd->vtx_len = icmd->VtxBuffer.Size;

		for (uint32_t y = 0; y < cmd->vtx_len; y++) {
			struct im_vtx *vtx = &cmd->vtx[y];
			struct im_vtx pvtx = *vtx;
			ImDrawVert *ivtx = &icmd->VtxBuffer[y];

			vtx->pos.x = ivtx->pos.x;
			vtx->pos.y = ivtx->pos.y;
			vtx->uv.x = ivtx->uv.x;
			vtx->uv.y = ivtx->uv.y;
			vtx->col = ivtx->col;

			diff = diff || memcmp(vtx, &pvtx, sizeof(struct im_vtx));
		}

		// Command Buffer
		if ((uint32_t) icmd->CmdBuffer.Size > cmd->cmd_max_len) {
			cmd->cmd = (struct im_cmd *) realloc(cmd->cmd, icmd->CmdBuffer.Size * sizeof(struct im_cmd));
			cmd->cmd_max_len = icmd->CmdBuffer.Size;
		}
		cmd->cmd_len = icmd->CmdBuffer.Size;

		for (uint32_t y = 0; y < cmd->cmd_len; y++) {
			struct im_cmd *ccmd = &cmd->cmd[y];
			struct im_cmd pccmd = *ccmd;
			ImDrawCmd *iccmd = &icmd->CmdBuffer[y];

			ccmd->texture_id = iccmd->TextureId; // XXX This must have meaning to graphics context
			ccmd->vtx_offset = iccmd->VtxOffset;
			ccmd->idx_offset = iccmd->IdxOffset;
			ccmd->elem_count = iccmd->ElemCount;
			ccmd->clip_rect.x = iccmd->ClipRect.x;
			ccmd->clip_rect.y = iccmd->ClipRect.y;
			ccmd->clip_rect.z = iccmd->ClipRect.z;
			ccmd->clip_rect.w = iccmd->ClipRect.w;

			diff = diff || memcmp(ccmd, &pccmd, sizeof(struct im_cmd));
		}

		diff = diff || memcmp(cmd, &pcmd, sizeof(struct im_cmd_list));
	}

	return diff;
}

void im_draw(void (*callback)(void *opaque), const void *opaque)
{
	ImGuiIO &io = GetIO();
	io.DisplaySize = ImVec2(IM.width, IM.height);

	#if defined(_WIN32)
		io.Fonts->TexID = im_dx11_font_texture(IM.dx11);
	#elif defined(__APPLE__)
		io.Fonts->TexID = im_mtl_font_texture(IM.mtl);
	#endif

	int64_t now = time_stamp();

	if (IM.ts != 0)
		io.DeltaTime = (float) time_diff(IM.ts, now) / 1000.0f;

	IM.ts = now;

	NewFrame();
	callback((void *) opaque);
	Render();

	im_copy_draw_data(&IM.draw_data, GetDrawData());

	io.MouseDown[0] = IM.mouse[0];
	io.MouseDown[1] = IM.mouse[1];
	io.MouseDown[2] = IM.mouse[2];

	for (uint32_t x = 0; x < IM_ARRAYSIZE(io.KeysDown); x++)
		io.KeysDown[x] = false;

	io.Fonts->TexID = NULL;
}

void im_render(bool clear)
{
	if (!IM.device || !IM.context)
		return;

	#if defined(_WIN32)
		ID3D11RenderTargetView *rtv = NULL;
		ID3D11Device *device = (ID3D11Device *) IM.device;
		HRESULT e = device->CreateRenderTargetView((ID3D11Texture2D *) IM.texture, NULL, &rtv);

		if (e == S_OK) {
			ID3D11DeviceContext *context = (ID3D11DeviceContext *) IM.context;

			if (clear) {
				FLOAT clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
				context->ClearRenderTargetView(rtv, clear_color);
			}

			context->OMSetRenderTargets(1, &rtv, NULL);

			im_dx11_render(IM.dx11, &IM.draw_data, device, context);
			rtv->Release();
		}

	#elif defined(__APPLE__)
		im_mtl_render(IM.mtl, &IM.draw_data, IM.context, IM.texture);
	#endif
}

void im_destroy(void)
{
	if (!IM.init)
		return;

	im_impl_destroy();
	DestroyContext();
	free(IM.font);

	memset(&IM, 0, sizeof(struct im));
}

float im_dpi_scale(void)
{
	return IM.dpi_scale;
}

float im_display_x(void)
{
	return GetIO().DisplaySize.x;
}

float im_display_y(void)
{
	return GetIO().DisplaySize.y;
}

bool im_key(enum scancode key)
{
	return GetIO().KeysDown[key];
}

bool im_ctrl(void)
{
	return GetIO().KeyCtrl;
}


// Drawing

bool im_begin_menu(const char *name, bool b)
{
	return BeginMenu(name, b);
}

void im_end_menu(void)
{
	ImGui::EndMenu();
}

bool im_menu_item(const char *name, const char *key, bool checked)
{
	return MenuItem(name, key, checked);
}

bool im_begin_window(const char *name, uint32_t flags)
{
	return Begin(name, NULL, flags);
}

void im_end_window(void)
{
	End();
}

bool im_begin_frame(uint32_t id, float width, float height,  uint32_t flags)
{
	return BeginChildFrame(id, ImVec2(width, height), flags);
}

void im_end_frame(void)
{
	EndChildFrame();
}

void im_separator(void)
{
	Separator();
}

bool im_begin_main_menu(void)
{
	return BeginMainMenuBar();
}

void im_end_main_menu(void)
{
	EndMainMenuBar();
}

void im_text(const char *text)
{
	TextUnformatted(text);
}

bool im_button(const char *label)
{
	return Button(label);
}

bool im_selectable(const char *label)
{
	return Selectable(label);
}

void im_pop_style(uint32_t n)
{
	PopStyleVar(n);
}

void im_pop_color(uint32_t n)
{
	PopStyleColor(n);
}

void im_push_color(enum ImGuiCol_ color, uint32_t value)
{
	PushStyleColor(color, value);
}

void im_push_style_f(enum ImGuiStyleVar_ style, float v)
{
	PushStyleVar(style, v);
}

void im_push_style_f2(enum ImGuiStyleVar_ style, float x, float y)
{
	PushStyleVar(style, ImVec2(x, y));
}

void im_set_window_size(float x, float y)
{
	SetNextWindowSize(ImVec2(x, y));
}

void im_set_window_pos(float x, float y)
{
	SetNextWindowPos(ImVec2(x, y));
}
