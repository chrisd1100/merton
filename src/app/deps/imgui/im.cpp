#include "im.h"

#include "imgui_draw.cpp"
#include "imgui_widgets.cpp"
#include "imgui.cpp"

#include "matoya.h"


using namespace ImGui;


// Framework

static struct im {
	bool init;
	int64_t ts;
	MTY_DrawData dd;
	float scale;
	bool mouse[3];
} IM;

void im_create(void)
{
	if (IM.init)
		return;

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

void im_input(const MTY_Msg *wmsg)
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

void *im_get_font(const void *font, size_t size, float lheight, float scale, int32_t *width, int32_t *height)
{
	ImGuiIO &io = GetIO();
	io.Fonts->AddFontFromMemoryCompressedTTF(font, (int32_t) size, scale * lheight);

	uint8_t *pixels = NULL;
	io.Fonts->GetTexDataAsRGBA32(&pixels, width, height);

	void *rgba = MTY_Alloc(*width * *height, 4);
	memcpy(rgba, pixels, *width * *height * 4);

	io.Fonts->ClearTexData();

	return rgba;
}

static bool im_copy_draw_data(MTY_DrawData *dd, ImDrawData *idd)
{
	MTY_DrawData pdd = *dd;

	dd->vtxTotalLength = idd->TotalVtxCount;
	dd->idxTotalLength = idd->TotalIdxCount;

	dd->displaySize.x = idd->DisplaySize.x;
	dd->displaySize.y = idd->DisplaySize.y;
	dd->displayPos.x = idd->DisplayPos.x;
	dd->displayPos.y = idd->DisplayPos.y;
	dd->fbScale.x = idd->FramebufferScale.x;
	dd->fbScale.y = idd->FramebufferScale.y;

	// Command Lists
	if ((uint32_t) idd->CmdListsCount > pdd.cmdListLength) {
		dd->cmdList = (MTY_CmdList *) realloc(dd->cmdList, idd->CmdListsCount * sizeof(MTY_CmdList));
		memset(dd->cmdList + pdd.cmdListLength, 0,
			(idd->CmdListsCount - pdd.cmdListLength) * sizeof(MTY_CmdList));
	}
	dd->cmdListLength = idd->CmdListsCount;

	bool diff = memcmp(dd, &pdd, sizeof(MTY_DrawData));

	for (uint32_t x = 0; x < dd->cmdListLength; x++) {
		MTY_CmdList *cmd = &dd->cmdList[x];
		MTY_CmdList pcmd = *cmd;
		ImDrawList *icmd = idd->CmdLists[x];

		// Index Buffer
		if ((uint32_t) icmd->IdxBuffer.Size > pcmd.idxLength)
			cmd->idx = (uint16_t *) realloc(cmd->idx, icmd->IdxBuffer.Size * sizeof(uint16_t));
		cmd->idxLength = icmd->IdxBuffer.Size;

		for (uint32_t y = 0; y < cmd->idxLength; y++) {
			diff = diff || cmd->idx[y] != icmd->IdxBuffer[y];
			cmd->idx[y] = icmd->IdxBuffer[y];
		}

		// Vertex Buffer
		if ((uint32_t) icmd->VtxBuffer.Size > pcmd.vtxLength)
			cmd->vtx = (MTY_Vtx *) realloc(cmd->vtx, icmd->VtxBuffer.Size * sizeof(MTY_Vtx));
		cmd->vtxLength = icmd->VtxBuffer.Size;

		for (uint32_t y = 0; y < cmd->vtxLength; y++) {
			MTY_Vtx *vtx = &cmd->vtx[y];
			MTY_Vtx pvtx = *vtx;
			ImDrawVert *ivtx = &icmd->VtxBuffer[y];

			vtx->pos.x = ivtx->pos.x;
			vtx->pos.y = ivtx->pos.y;
			vtx->uv.x = ivtx->uv.x;
			vtx->uv.y = ivtx->uv.y;
			vtx->col = ivtx->col;

			diff = diff || memcmp(vtx, &pvtx, sizeof(MTY_Vtx));
		}

		// Command Buffer
		if ((uint32_t) icmd->CmdBuffer.Size > pcmd.cmdLength)
			cmd->cmd = (MTY_Cmd *) realloc(cmd->cmd, icmd->CmdBuffer.Size * sizeof(MTY_Cmd));
		cmd->cmdLength = icmd->CmdBuffer.Size;

		for (uint32_t y = 0; y < cmd->cmdLength; y++) {
			MTY_Cmd *ccmd = &cmd->cmd[y];
			MTY_Cmd pccmd = *ccmd;
			ImDrawCmd *iccmd = &icmd->CmdBuffer[y];

			// This is nothing more than a lookup id for the graphics context.
			// the textures should be managed by the graphics layer
			ccmd->texture = (uint32_t) (uintptr_t) iccmd->TextureId;
			ccmd->vtxOffset = iccmd->VtxOffset;
			ccmd->idxOffset = iccmd->IdxOffset;
			ccmd->elemCount = iccmd->ElemCount;
			ccmd->clip.x = iccmd->ClipRect.x;
			ccmd->clip.y = iccmd->ClipRect.y;
			ccmd->clip.z = iccmd->ClipRect.z;
			ccmd->clip.w = iccmd->ClipRect.w;

			diff = diff || memcmp(ccmd, &pccmd, sizeof(MTY_Cmd));
		}

		diff = diff || memcmp(cmd, &pcmd, sizeof(MTY_CmdList));
	}

	return diff;
}

const MTY_DrawData *im_draw(uint32_t width, uint32_t height, float scale,
	bool clear, void (*callback)(void *opaque), const void *opaque)
{
	ImGuiIO &io = GetIO();
	io.Fonts->TexID = (void *) (uintptr_t) IM_FONT_ID;
	io.DisplaySize = ImVec2((float) width, (float) height);

	int64_t now = MTY_Timestamp();

	if (IM.ts != 0)
		io.DeltaTime = (float) MTY_TimeDiff(IM.ts, now) / 1000.0f;

	IM.ts = now;
	IM.scale = scale;

	NewFrame();
	callback((void *) opaque);
	Render();

	im_copy_draw_data(&IM.dd, GetDrawData());
	IM.dd.clear = clear;

	io.MouseDown[0] = IM.mouse[0];
	io.MouseDown[1] = IM.mouse[1];
	io.MouseDown[2] = IM.mouse[2];

	for (uint32_t x = 0; x < IM_ARRAYSIZE(io.KeysDown); x++)
		io.KeysDown[x] = false;

	return &IM.dd;
}

void im_destroy(void)
{
	if (!IM.init)
		return;

	DestroyContext();

	memset(&IM, 0, sizeof(struct im));
}

float im_scale(void)
{
	return IM.scale;
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
