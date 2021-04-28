// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "im.h"

#define COLOR_TEXT    0xFEEEEEEE
#define COLOR_LABEL   0xFFAAAAAA
#define COLOR_BUTTON  0xFF555555
#define COLOR_BORDER  0xF84F4F4F
#define COLOR_DARK_BG 0xF7333333
#define COLOR_MSG_BG  0xD1333333
#define COLOR_HOVER   0xF7777777

#define X(v) (im_scale() * (float) lrint(v))

#define PACK_ASPECT(x, y) (((x) << 8) | (y))

enum nav {
	NAV_NONE     = 0x0000,
	NAV_MENU     = 0x0100,
	NAV_OPEN_ROM = 0x0001,
};

static struct component_state {
	uint32_t nav;
	MTY_FileList *fl;
	bool refreshed;
	const char *dir;

	int64_t ts;
	int32_t timeout;
	char *msg;
} CMP;

void ui_set_message(const char *msg, int32_t timeout)
{
	free(CMP.msg);
	CMP.msg = MTY_Strdup(msg);

	CMP.ts = MTY_GetTime();
	CMP.timeout = timeout;
}

static void ui_message(void)
{
	if (CMP.ts != 0 && MTY_TimeDiff(CMP.ts, MTY_GetTime()) < CMP.timeout) {
		im_push_color(ImGuiCol_WindowBg, COLOR_MSG_BG);

		im_set_window_pos(X(12), X((CMP.nav & NAV_MENU) ? 34 : 12));
		im_set_window_size(0, 0);

		if (im_begin_window("TIMED_MESSAGE", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNavInputs)) {
			im_text(CMP.msg);
			im_end_window();
		}

		im_pop_color(1);
	}
}

static void ui_open_rom(struct app_event *event)
{
	float padding_h = X(12);
	float padding_v = X(12);
	float offset = X((CMP.nav & NAV_MENU) ? 34 : 12);
	im_set_window_pos(padding_h, offset);

	float w = im_display_x() - padding_h * 2.0f;
	float h = (im_display_y() - (offset - padding_v)) - padding_v * 2.0f;
	im_set_window_size(w, h);

	uint32_t flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoScrollWithMouse;

	if (im_begin_window("OPEN_ROM", flags)) {
		if (im_button("Close"))
			CMP.nav ^= NAV_OPEN_ROM;

		im_push_style_f2(ImGuiStyleVar_FramePadding, 0, 0);
		im_begin_frame(0x01, w, h - X(50), ImGuiWindowFlags_NavFlattened);

		if (!CMP.refreshed) {
			MTY_FileList *fl = MTY_GetFileList(CMP.dir ? CMP.dir : ".", NULL);

			if (fl->len > 0) {
				MTY_FreeFileList(&CMP.fl);
				CMP.fl = fl;

				CMP.refreshed = true;
			}
		}

		for (uint32_t x = 0; x < CMP.fl->len; x++) {
			if (im_selectable(CMP.fl->files[x].name)) {
				if (CMP.fl->files[x].dir) {
					CMP.dir = CMP.fl->files[x].path;
					CMP.refreshed = false;

				} else {
					event->type = APP_EVENT_LOAD_GAME;
					event->fetch_core = true;
					event->rt = true;
					snprintf(event->game, MTY_PATH_MAX, "%s", CMP.fl->files[x].path);
				}
			}
		}

		im_end_frame();
		im_pop_style(1);

		im_end_window();
	}
}

static void ui_save_state(struct core *core, const char *content_name, uint8_t index)
{
	size_t size = 0;
	void *state = core_get_state(core, &size);

	if (state) {
		const char *path = MTY_JoinPath(MTY_GetProcessDir(), "state");
		MTY_Mkdir(path);

		const char *name = MTY_SprintfDL("%s.state%u", content_name, index);
		MTY_WriteFile(MTY_JoinPath(path, name), state, size);

		ui_set_message(MTY_SprintfDL("State saved to slot %u", index), 3000);

		free(state);
	}
}

static void ui_load_state(struct core *core, const char *content_name, uint8_t index)
{
	const char *path = MTY_JoinPath(MTY_GetProcessDir(), "state");
	MTY_Mkdir(path);

	const char *name = MTY_SprintfDL("%s.state%u", content_name, index);

	size_t size = 0;
	void *state = MTY_ReadFile(MTY_JoinPath(path, name), &size);
	const char *msg = NULL;;

	if (state) {
		if (core_set_state(core, state, size)) {
			msg = MTY_SprintfDL("State loaded from slot %u", index);

		} else {
			msg = MTY_SprintfDL("Error loading state from slot %u", index);
		}

		free(state);

	} else {
		msg = MTY_SprintfDL("State does not exist for slot %u", index);
	}

	if (msg)
		ui_set_message(msg, 3000);
}

static void ui_menu(const struct ui_args *args, struct app_event *event)
{
	if (im_begin_main_menu()) {
		if (im_begin_menu("System", true)) {
			if (im_menu_item("Load ROM", "Ctrl+O", false))
				CMP.nav ^= NAV_OPEN_ROM;

			if (im_menu_item("Unload ROM", "", false)) {
				event->type = APP_EVENT_UNLOAD_GAME;
				event->rt = true;
			}

			im_separator();

			if (im_menu_item(args->paused ? "Unpause" : "Pause", "Ctrl+P", false))
				event->type = APP_EVENT_PAUSE;

			if (im_menu_item("Reset", "Ctrl+R", false)) {
				core_reset_game(args->core);
			}

			if (im_menu_item("Reload Game", "Ctrl+T", false)) {
				const char *name = core_get_game_path(args->core);
				if (name) {
					event->type = APP_EVENT_LOAD_GAME;
					event->fetch_core = true;
					event->rt = true;
					snprintf(event->game, MTY_PATH_MAX, "%s", name);
				}
			}

			im_separator();

			if (im_begin_menu("Save State", true)) {
				for (uint8_t x = 0; x < 8; x++) {
					char label[16];
					char key[8];
					snprintf(label, 16, "Slot %u", x + 1);
					snprintf(key, 8, "%u", x + 1);

					if (im_menu_item(label, key, false))
						ui_save_state(args->core, args->content_name, x + 1);
				}

				im_end_menu();
			}

			if (im_begin_menu("Load State", true)) {
				for (uint8_t x = 0; x < 8; x++) {
					char label[16];
					char key[8];
					snprintf(label, 16, "Slot %u", x + 1);
					snprintf(key, 8, "Ctrl+%u", x + 1);

					if (im_menu_item(label, key, false))
						ui_load_state(args->core, args->content_name, x + 1);
				}

				im_end_menu();
			}

			im_separator();

			if (im_begin_menu("Reduce Latency", true)) {
				for (uint32_t x = 0; x < 16; x++)
					if (im_menu_item(MTY_SprintfDL("%u", x), "", args->cfg->reduce_latency == x))
						event->cfg.reduce_latency = x;

				im_end_menu();
			}

			if (im_menu_item("Background Pause", "", args->cfg->bg_pause))
				event->cfg.bg_pause = !event->cfg.bg_pause;

			#if defined(_WIN32)
			if (im_menu_item("Console Window", "", args->cfg->console))
				event->cfg.console = !event->cfg.console;
			#endif

			im_separator();

			if (im_menu_item("Quit", "", false))
				event->type = APP_EVENT_QUIT;

			im_end_menu();
		}

		if (args->systems && im_begin_menu("Core", true)) {
			uint32_t len = MTY_JSONGetLength(args->systems);

			for (uint32_t x = 0; x < len; x++) {
				const char *key = MTY_JSONObjGetKey(args->systems, x);
				const MTY_JSON *val = MTY_JSONObjGetItem(args->systems, key);

				char name[SYSTEM_NAME_MAX];
				if (!MTY_JSONObjGetString(val, "name", name, SYSTEM_NAME_MAX))
					continue;

				if (im_begin_menu(name, true)) {
					const MTY_JSON *cores = MTY_JSONObjGetItem(val, "cores");

					if (cores) {
						uint32_t clen = MTY_JSONGetLength(cores);

						for (uint32_t y = 0; y < clen; y++) {
							const MTY_JSON *core = MTY_JSONArrayGetItem(cores, y);
							if (!core)
								continue;

							char cname[CONFIG_CORE_MAX];
							if (!MTY_JSONObjGetString(core, "name", cname, CONFIG_CORE_MAX))
								continue;

							const char *selected = CONFIG_GET_CORE(args->cfg, key);
							char *dest = CONFIG_GET_CORE(&event->cfg, key);

							if (im_menu_item(cname, "", !strcmp(cname, selected)))
								snprintf(dest, CONFIG_CORE_MAX, "%s", cname);
						}
					}

					im_end_menu();
				}
			}

			im_separator();

			if (im_menu_item("Reset Core Options", "", false)) {
				event->type = APP_EVENT_CLEAR_OPTS;
				event->rt = true;
			}

			if (core_has_disk_interface(args->core)) {
				if (im_begin_menu("Disks", true)) {
					uint8_t n = core_get_num_disks(args->core);
					int8_t disk = core_get_disk(args->core);

					if (im_begin_menu("Insert", true)) {
						for (uint8_t x = 0; x < n; x++) {
							char nstr[8];
							snprintf(nstr, 8, "%u", x);

							if (im_menu_item(nstr, "", x == disk))
								core_set_disk(args->core, x);
						}

						im_end_menu();
					}
					im_end_menu();
				}
			}

			uint32_t vlen = 0;
			const struct core_variable *vars = core_get_variables(args->core, &vlen);

			if (vlen > 0)
				im_separator();

			for (uint32_t x = 0; x < vlen; x++) {
				if (im_begin_menu(vars[x].desc, true)) {
					const char *cur = core_get_variable(args->core, vars[x].key);
					if (!cur)
						cur = vars[x].opts[0];

					for (uint32_t y = 0; y < vars[x].nopts; y++) {
						if (im_menu_item(vars[x].opts[y], "", !strcmp(cur, vars[x].opts[y]))) {
							event->type = APP_EVENT_CORE_OPT;
							event->rt = true;
							snprintf(event->opt.key, CORE_KEY_NAME_MAX, "%s", vars[x].key);
							snprintf(event->opt.val, CORE_OPT_NAME_MAX, "%s", vars[x].opts[y]);
						}
					}

					im_end_menu();
				}
			}


			im_end_menu();
		}

		if (im_begin_menu("Video", true)) {
			MTY_GFX apis[MTY_GFX_MAX];
			uint32_t n = MTY_GetAvailableGFX(apis);

			if (n > 1) {
				if (im_begin_menu("Graphics", true)) {
					for (uint32_t x = 0; x < n; x++) {
						MTY_GFX api = apis[x];

						const char *name =
							api == MTY_GFX_GL ? "OpenGL" :
							api == MTY_GFX_D3D9 ? "D3D9" :
							api == MTY_GFX_D3D11 ? "D3D11" :
							api == MTY_GFX_METAL ? "Metal" : "";

						if (im_menu_item(name, "", args->gfx == api))
							event->cfg.gfx = api;
					}

					im_end_menu();
				}
			}

			if (im_begin_menu("Window", true)) {
				if (im_menu_item("Fullscreen", "Ctrl+W", args->fullscreen))
					event->cfg.fullscreen = !event->cfg.fullscreen;

				im_end_menu();
			}

			if (im_begin_menu("Frame Size", true)) {
				if (im_menu_item("1x", "", args->cfg->frame_size == 1))
					event->cfg.frame_size = 1;

				if (im_menu_item("2x", "", args->cfg->frame_size == 2))
					event->cfg.frame_size = 2;

				if (im_menu_item("3x", "", args->cfg->frame_size == 3))
					event->cfg.frame_size = 3;

				if (im_menu_item("4x", "", args->cfg->frame_size == 4))
					event->cfg.frame_size = 4;

				if (im_menu_item("Fill", "", args->cfg->frame_size == 0))
					event->cfg.frame_size = 0;

				im_end_menu();
			}

			uint32_t aspect = PACK_ASPECT(args->cfg->aspect_ratio.x, args->cfg->aspect_ratio.y);

			if (im_begin_menu("Aspect Ratio", true)) {
				if (im_menu_item("From Core", "", aspect == PACK_ASPECT(0, 0)))
					event->cfg.aspect_ratio.x = 0, event->cfg.aspect_ratio.y = 0;

				if (im_menu_item("127:105", "", aspect == PACK_ASPECT(127, 105)))
					event->cfg.aspect_ratio.x = 127, event->cfg.aspect_ratio.y = 105;

				if (im_menu_item("16:15", "", aspect == PACK_ASPECT(16, 15)))
					event->cfg.aspect_ratio.x = 16, event->cfg.aspect_ratio.y = 15;

				if (im_menu_item("8:7", "", aspect == PACK_ASPECT(8, 7)))
					event->cfg.aspect_ratio.x = 8, event->cfg.aspect_ratio.y = 7;

				if (im_menu_item("4:3", "", aspect == PACK_ASPECT(4, 3)))
					event->cfg.aspect_ratio.x = 4, event->cfg.aspect_ratio.y = 3;

				im_end_menu();
			}

			if (im_begin_menu("Filter", true)) {
				if (im_menu_item("Nearest", "", args->cfg->filter == MTY_FILTER_NEAREST))
					event->cfg.filter = MTY_FILTER_NEAREST;

				if (im_menu_item("Linear", "", args->cfg->filter == MTY_FILTER_LINEAR))
					event->cfg.filter = MTY_FILTER_LINEAR;

				if (im_menu_item("Gaussian Soft", "", args->cfg->filter == MTY_FILTER_GAUSSIAN_SOFT))
					event->cfg.filter = MTY_FILTER_GAUSSIAN_SOFT;

				if (im_menu_item("Gaussian Sharp", "", args->cfg->filter == MTY_FILTER_GAUSSIAN_SHARP))
					event->cfg.filter = MTY_FILTER_GAUSSIAN_SHARP;

				im_end_menu();
			}

			if (im_begin_menu("Effect", true)) {
				if (im_menu_item("None", "", args->cfg->effect == MTY_EFFECT_NONE))
					event->cfg.effect = MTY_EFFECT_NONE;

				if (im_menu_item("Scanlines", "", args->cfg->effect == MTY_EFFECT_SCANLINES))
					event->cfg.effect = MTY_EFFECT_SCANLINES;

				if (im_menu_item("Scanlines x2", "", args->cfg->effect == MTY_EFFECT_SCANLINES_X2))
					event->cfg.effect = MTY_EFFECT_SCANLINES_X2;

				im_end_menu();
			}
			im_end_menu();
		}

		if (im_begin_menu("Audio", true)) {
			if (im_menu_item(args->cfg->mute ? "Unmute" : "Mute", "Ctrl+M", false))
				event->cfg.mute = !event->cfg.mute;

			im_end_menu();
		}

		im_end_main_menu();
	}
}

static void ui_hotkeys(const struct ui_args *args, struct app_event *event)
{
	if (im_key(MTY_KEY_ESCAPE)) {
		CMP.ts = 0;

		if (CMP.nav & NAV_OPEN_ROM) {
			CMP.nav ^= NAV_OPEN_ROM;

		} else {
			CMP.nav ^= NAV_MENU;
		}
	}

	if (im_key(MTY_KEY_W) && im_ctrl())
		event->cfg.fullscreen = !event->cfg.fullscreen;

	if (im_key(MTY_KEY_O) && im_ctrl())
		CMP.nav ^= NAV_OPEN_ROM;

	if (im_key(MTY_KEY_P) && im_ctrl())
		event->type = APP_EVENT_PAUSE;

	if (im_key(MTY_KEY_R) && im_ctrl())
		core_reset_game(args->core);

	if (im_key(MTY_KEY_T) && im_ctrl()) {
		const char *name = core_get_game_path(args->core);
		if (name) {
			event->type = APP_EVENT_LOAD_GAME;
			event->fetch_core = true;
			event->rt = true;
			snprintf(event->game, MTY_PATH_MAX, "%s", name);
		}
	}

	if (im_key(MTY_KEY_M) && im_ctrl())
		event->cfg.mute = !event->cfg.mute;

	for (uint8_t x = 0; x < 8; x++) {
		if (im_key(MTY_KEY_1 + x) && im_ctrl()) {
			ui_load_state(args->core, args->content_name, x + 1);

		} else if (im_key(MTY_KEY_1 + x)) {
			ui_save_state(args->core, args->content_name, x + 1);
		}
	}
}

void ui_root(const struct ui_args *args,
	void (*event_callback)(struct app_event *event, void *opaque), const void *opaque)
{
	struct app_event event = {0};
	event.cfg = *args->cfg;

	im_push_color(ImGuiCol_Separator,        COLOR_BORDER);
	im_push_color(ImGuiCol_SeparatorActive,  COLOR_BORDER);
	im_push_color(ImGuiCol_SeparatorHovered, COLOR_BORDER);
	im_push_color(ImGuiCol_Border,           COLOR_BORDER);
	im_push_color(ImGuiCol_Text,             COLOR_TEXT);
	im_push_color(ImGuiCol_TextDisabled,     COLOR_LABEL);
	im_push_color(ImGuiCol_WindowBg,         COLOR_DARK_BG);
	im_push_color(ImGuiCol_PopupBg,          COLOR_DARK_BG);
	im_push_color(ImGuiCol_MenuBarBg,        COLOR_DARK_BG);
	im_push_color(ImGuiCol_FrameBg,          COLOR_DARK_BG);
	im_push_color(ImGuiCol_FrameBgHovered,   COLOR_HOVER);
	im_push_color(ImGuiCol_FrameBgActive,    COLOR_DARK_BG);
	im_push_color(ImGuiCol_Header,           COLOR_DARK_BG);
	im_push_color(ImGuiCol_HeaderHovered,    COLOR_HOVER);
	im_push_color(ImGuiCol_HeaderActive,     COLOR_DARK_BG);
	im_push_color(ImGuiCol_Button,           COLOR_BUTTON);
	im_push_color(ImGuiCol_ButtonHovered,    COLOR_HOVER);
	im_push_color(ImGuiCol_ButtonActive,     COLOR_BUTTON);
	im_push_color(ImGuiCol_NavHighlight,     COLOR_BORDER);

	im_push_style_f(ImGuiStyleVar_ScrollbarSize,    X(12));
	im_push_style_f(ImGuiStyleVar_WindowBorderSize, 1);
	im_push_style_f(ImGuiStyleVar_ChildBorderSize,  1);
	im_push_style_f(ImGuiStyleVar_PopupBorderSize,  1);
	im_push_style_f(ImGuiStyleVar_WindowRounding,   0);
	im_push_style_f2(ImGuiStyleVar_ItemSpacing,     X(10), X(8));
	im_push_style_f2(ImGuiStyleVar_FramePadding,    X(10), X(6));
	im_push_style_f2(ImGuiStyleVar_WindowPadding,   X(10), X(10));

	ui_hotkeys(args, &event);

	if (args->show_menu)
		CMP.nav |= NAV_MENU;

	if (CMP.nav & NAV_MENU)
		ui_menu(args, &event);

	if (CMP.nav & NAV_OPEN_ROM)
		ui_open_rom(&event);

	ui_message();

	im_pop_style(8);
	im_pop_color(19);

	if (memcmp(args->cfg, &event.cfg, sizeof(struct config))) {
		event.type = APP_EVENT_CONFIG;
		event.rt = false;
	}

	if (event.type != APP_EVENT_NONE)
		event_callback(&event, (void *) opaque);
}

void ui_close_menu(void)
{
	CMP.nav = NAV_NONE;
}

void ui_destroy(void)
{
	MTY_FreeFileList(&CMP.fl);

	free(CMP.msg);

	memset(&CMP, 0, sizeof(struct component_state));
}
