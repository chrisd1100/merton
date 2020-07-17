#include "im.h"

#include "imgui_draw.cpp"
#include "imgui_widgets.cpp"
#include "imgui.cpp"

#if defined(_WIN32)
	#include "impl/im-dx11.h"
#elif defined(__APPLE__)
	#include "impl/im-mtl.h"
#else
	#include "impl/im-gl.h"
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
	#else
	struct im_gl *gl;
	#endif
	struct im_draw_data draw_data;
	MTY_Device *device;
	MTY_Context *context;
	MTY_Texture *texture;
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

	io.KeyMap[ImGuiKey_Tab] = MTY_SCANCODE_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = MTY_SCANCODE_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = MTY_SCANCODE_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = MTY_SCANCODE_UP;
	io.KeyMap[ImGuiKey_DownArrow] = MTY_SCANCODE_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = MTY_SCANCODE_PAGEUP;
	io.KeyMap[ImGuiKey_PageDown] = MTY_SCANCODE_PAGEDOWN;
	io.KeyMap[ImGuiKey_Home] = MTY_SCANCODE_HOME;
	io.KeyMap[ImGuiKey_End] = MTY_SCANCODE_END;
	io.KeyMap[ImGuiKey_Insert] = MTY_SCANCODE_INSERT;
	io.KeyMap[ImGuiKey_Delete] = MTY_SCANCODE_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = MTY_SCANCODE_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = MTY_SCANCODE_ENTER;
	io.KeyMap[ImGuiKey_Enter] = MTY_SCANCODE_ENTER;
	io.KeyMap[ImGuiKey_Escape] = MTY_SCANCODE_ESCAPE;
	io.KeyMap[ImGuiKey_A] = MTY_SCANCODE_A;
	io.KeyMap[ImGuiKey_C] = MTY_SCANCODE_C;
	io.KeyMap[ImGuiKey_V] = MTY_SCANCODE_V;
	io.KeyMap[ImGuiKey_X] = MTY_SCANCODE_X;
	io.KeyMap[ImGuiKey_Y] = MTY_SCANCODE_Y;
	io.KeyMap[ImGuiKey_Z] = MTY_SCANCODE_Z;

	io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.IniFilename = NULL;
	io.LogFilename = NULL;

	IM.init = true;
}

void im_input(MTY_WindowMsg *wmsg)
{
	ImGuiIO &io = GetIO();

	switch (wmsg->type) {
		case MTY_WINDOW_MSG_MOUSE_WHEEL:
			if (wmsg->mouseWheel.y > 0) io.MouseWheel += 1;
			if (wmsg->mouseWheel.y < 0) io.MouseWheel -= 1;
			break;

		case MTY_WINDOW_MSG_MOUSE_BUTTON:
			if (wmsg->mouseButton.button == MTY_MOUSE_BUTTON_L)      IM.mouse[0] = wmsg->mouseButton.pressed;
			if (wmsg->mouseButton.button == MTY_MOUSE_BUTTON_R)      IM.mouse[1] = wmsg->mouseButton.pressed;
			if (wmsg->mouseButton.button == MTY_MOUSE_BUTTON_MIDDLE) IM.mouse[2] = wmsg->mouseButton.pressed;

			io.MouseDown[0] = io.MouseDown[0] || IM.mouse[0];
			io.MouseDown[1] = io.MouseDown[1] || IM.mouse[1];
			io.MouseDown[2] = io.MouseDown[2] || IM.mouse[2];
			break;

		case MTY_WINDOW_MSG_MOUSE_MOTION:
			if (!wmsg->mouseMotion.relative)
				io.MousePos = ImVec2((float) wmsg->mouseMotion.x, (float) wmsg->mouseMotion.y);
			break;

		case MTY_WINDOW_MSG_KEYBOARD: {
			MTY_Scancode sc = wmsg->keyboard.scancode;

			if (wmsg->keyboard.pressed && sc < IM_ARRAYSIZE(io.KeysDown))
				io.KeysDown[sc] = true;

			if (sc == MTY_SCANCODE_LSHIFT || sc == MTY_SCANCODE_RSHIFT)
				io.KeyShift = wmsg->keyboard.pressed;

			if (sc == MTY_SCANCODE_LCTRL || sc == MTY_SCANCODE_RCTRL)
				io.KeyCtrl =  wmsg->keyboard.pressed;

			if (sc == MTY_SCANCODE_LALT || sc == MTY_SCANCODE_RALT)
				io.KeyAlt = wmsg->keyboard.pressed;

			if (sc == MTY_SCANCODE_LGUI || sc == MTY_SCANCODE_RGUI)
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
	#else
		im_gl_destroy(&IM.gl);
	#endif
}

static bool im_impl_init(MTY_Device *device, MTY_Context *context)
{
	ImGuiIO &io = GetIO();

	uint8_t *pixels = NULL;
	int32_t width = 0, height = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	#if defined(_WIN32)
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		bool r = device && context &&
			im_dx11_create((ID3D11Device *) device, pixels, width, height, &IM.dx11);
	#elif defined(__APPLE__)
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

		bool r = device && im_mtl_create((MTL_Device *) device, pixels, width, height, &IM.mtl);
	#else
		// FIXME GLES needs version 100 here
		bool r = im_gl_create("#version 110", pixels, width, height, &IM.gl);
	#endif

	if (!r) {
		im_impl_destroy();
		r = false;
	}

	return r;
}

bool im_begin(float dpi_scale, MTY_Device *device, MTY_Context *context, MTY_Texture *texture)
{
	if (!IM.impl_init || device != IM.device || context != IM.context || dpi_scale != IM.dpi_scale) {
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

	IM.texture = texture;

	#if defined(_WIN32)
		im_dx11_texture_size((ID3D11Texture2D *) texture, &IM.width, &IM.height);

	#elif defined(__APPLE__)
		im_mtl_texture_size((MTL_Texture *) IM.texture, &IM.width, &IM.height);

	#else
		im_gl_texture_size(IM.gl, (GL_Uint) (size_t) texture, &IM.width, &IM.height);
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
	#else
		io.Fonts->TexID = (void *) im_gl_font_texture(IM.gl);
	#endif

	int64_t now = MTY_Timestamp();

	if (IM.ts != 0)
		io.DeltaTime = (float) MTY_TimeDiff(IM.ts, now) / 1000.0f;

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
	#if defined(_WIN32)
		if (!IM.device || !IM.context || !IM.texture)
			return;

		im_dx11_render(IM.dx11, &IM.draw_data, clear,
			(ID3D11Device *) IM.device, (ID3D11DeviceContext *) IM.context, (ID3D11Texture2D *) IM.texture);

	#elif defined(__APPLE__)
		if (!IM.context || !IM.texture)
			return;

		im_mtl_render(IM.mtl, &IM.draw_data, clear, (MTL_CommandQueue *) IM.context, (MTL_Texture *) IM.texture);

	#else
		if (!IM.texture)
			return;

		im_gl_render(IM.gl, &IM.draw_data, clear, (GL_Uint) (size_t) IM.texture);
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

bool im_key(MTY_Scancode key)
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
