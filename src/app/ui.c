// Copyright (c) 2019-2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "deps/imgui/im.h"

#define COLOR_TEXT    0xFEEEEEEE
#define COLOR_LABEL   0xFFAAAAAA
#define COLOR_BUTTON  0xFF555555
#define COLOR_BORDER  0xF84F4F4F
#define COLOR_DARK_BG 0xF7333333
#define COLOR_MSG_BG  0xD1333333
#define COLOR_HOVER   0xF7777777

#define X(v) (im_dpi_scale() * (float) lrint(v))

#define PACK_ASPECT(x, y) (((x) << 8) | (y))

#define UI_LOG_LINES  20

enum nav {
	NAV_NONE     = 0x0000,
	NAV_MENU     = 0x0100,
	NAV_OPEN_ROM = 0x0001,
};

static struct component_state {
	uint32_t nav;
	MTY_FileDesc *fi;
	uint32_t fi_n;
	bool refreshed;
	const char *dir;

	int64_t ts;
	int32_t timeout;
	char *msg;

	char logs[UI_LOG_LINES][UI_LOG_LEN];
	uint32_t log_lines;
	int64_t log_ts;
	int32_t log_timeout;
} CMP;

void ui_set_message(const char *msg, int32_t timeout)
{
	free(CMP.msg);
	CMP.msg = MTY_Strdup(msg);

	CMP.ts = MTY_Timestamp();
	CMP.timeout = timeout;
}

static void ui_message(void)
{
	if (CMP.ts != 0 && MTY_TimeDiff(CMP.ts, MTY_Timestamp()) < CMP.timeout) {
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

void ui_add_log(const char *msg, int32_t timeout)
{
	if (++CMP.log_lines > UI_LOG_LINES) {
		memmove(CMP.logs, (char *) CMP.logs + UI_LOG_LEN, UI_LOG_LEN * (UI_LOG_LINES - 1));
		CMP.log_lines = UI_LOG_LINES;
	}

	snprintf(CMP.logs[CMP.log_lines - 1], UI_LOG_LEN, "%s", msg);

	CMP.log_ts = MTY_Timestamp();
	CMP.log_timeout = timeout;
}

void ui_clear_log(void)
{
	CMP.log_lines = 0;
	memset(CMP.logs, 0, UI_LOG_LEN * UI_LOG_LINES);
}

static void ui_log(bool always)
{
	if (always || (CMP.log_ts != 0 && MTY_TimeDiff(CMP.log_ts, MTY_Timestamp()) < CMP.log_timeout)) {
		im_push_color(ImGuiCol_WindowBg, COLOR_MSG_BG);
		im_push_style_f2(ImGuiStyleVar_ItemSpacing, X(5), X(4));

		float h = X(34.0f + 20.5f * (float) (CMP.log_lines - 1));
		float w = X(260);
		float padding_h = X(12);
		float padding_v = X((CMP.nav & NAV_MENU) ? 34 : 12);
		im_set_window_pos(im_display_x() - w - padding_h, padding_v);
		im_set_window_size(w, h);

		if (im_begin_window("LOG", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_NoNavInputs)) {
			for (uint32_t x = 0; x < CMP.log_lines; x++)
				im_text(CMP.logs[x]);

			im_end_window();
		}

		im_pop_style(1);
		im_pop_color(1);
	}
}

static void ui_open_rom(struct ui_event *event)
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
			MTY_FileDesc *fi = NULL;
			uint32_t n = MTY_FsList(CMP.dir ? CMP.dir : ".", &fi);

			if (n > 0) {
				MTY_FsFreeList(&CMP.fi, CMP.fi_n);
				CMP.fi = fi;
				CMP.fi_n = n;

				CMP.refreshed = true;
			}
		}

		for (uint32_t x = 0; x < CMP.fi_n; x++) {
			if (im_selectable(CMP.fi[x].name)) {
				if (CMP.fi[x].dir) {
					CMP.dir = CMP.fi[x].path;
					CMP.refreshed = false;

				} else {
					event->type = UI_EVENT_OPEN_ROM;
					event->rom_name = CMP.fi[x].path;
					ui_close_menu();
				}
			}
		}

		im_end_frame();
		im_pop_style(1);

		im_end_window();
	}
}

static void ui_save_state(NES *nes, uint32_t crc32, uint8_t index)
{
	size_t size = 0;
	void *state = NES_GetState(nes, &size);

	if (state) {
		const char *path = MTY_FsPath(MTY_FsGetDir(MTY_DIR_PROGRAM), "state");
		MTY_FsMkdir(path);

		char name[32];
		snprintf(name, 32, "%02X-%u.state", crc32, index);
		MTY_FsWrite(MTY_FsPath(path, name), state, size);

		char msg[64];
		snprintf(msg, 64, "State saved to slot %u", index);
		ui_set_message(msg, 3000);

		free(state);
	}
}

static void ui_load_state(NES *nes, uint32_t crc32, uint8_t index)
{
	const char *path = MTY_FsPath(MTY_FsGetDir(MTY_DIR_PROGRAM), "state");
	MTY_FsMkdir(path);

	char name[32];
	snprintf(name, 32, "%02X-%u.state", crc32, index);

	size_t size = 0;
	void *state = NULL;
	char msg[64];

	if (MTY_FsRead(MTY_FsPath(path, name), &state, &size)) {
		if (NES_SetState(nes, state, size)) {
			snprintf(msg, 64, "State loaded from slot %u", index);

		} else {
			snprintf(msg, 64, "Error loading state from slot %u", index);
		}

		free(state);
	} else {
		snprintf(msg, 64, "State does not exist for slot %u", index);
	}

	ui_set_message(msg, 3000);
}

static void ui_menu(const struct ui_args *args, struct ui_event *event)
{
	if (im_begin_main_menu()) {
		if (im_begin_menu("System", true)) {
			if (im_menu_item("Load ROM", "Ctrl+O", false))
				CMP.nav ^= NAV_OPEN_ROM;

			if (im_menu_item("Unload ROM", "", false)) {
				NES_LoadCart(args->nes, NULL, 0, NULL, 0, NULL);
				event->type = UI_EVENT_UNLOAD_ROM;
			}

			im_separator();

			if (im_menu_item(args->paused ? "Unpause" : "Pause", "Ctrl+P", false))
				event->type = UI_EVENT_PAUSE;

			if (im_menu_item("Reset", "Ctrl+R", false))
				NES_Reset(args->nes, false);

			if (im_menu_item("Power Cycle", "Ctrl+T", false))
				NES_Reset(args->nes, true);

			im_separator();

			if (im_begin_menu("Save State", true)) {
				for (uint8_t x = 0; x < 8; x++) {
					char label[16];
					char key[8];
					snprintf(label, 16, "Slot %u", x + 1);
					snprintf(key, 8, "%u", x + 1);

					if (im_menu_item(label, key, false))
						ui_save_state(args->nes, args->crc32, x + 1);
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
						ui_load_state(args->nes, args->crc32, x + 1);
				}

				im_end_menu();
			}

			im_separator();

			if (im_menu_item("Disable Sprite Limit", "", args->cfg->nes.maxSprites == 64))
				event->cfg.nes.maxSprites = args->cfg->nes.maxSprites == 64 ? 8 : 64;

			bool overclocking = args->cfg->nes.preNMI != 0 || args->cfg->nes.postNMI != 0;
			if (im_menu_item("Enable Overclocking", "", overclocking))
				event->cfg.nes.preNMI = event->cfg.nes.postNMI = overclocking ? 0 : 131;

			im_separator();

			if (im_menu_item("Background Pause", "", args->cfg->bg_pause))
				event->cfg.bg_pause = !event->cfg.bg_pause;

			if (im_menu_item("Reduce Latency", "", args->cfg->reduce_latency))
				event->cfg.reduce_latency = !event->cfg.reduce_latency;

			if (im_menu_item("Use NRS NES 2.0 DB", "", args->cfg->use_db))
				event->cfg.use_db = !event->cfg.use_db;

			if (im_begin_menu("Log", true)) {
				if (im_menu_item("Hide", "", args->cfg->log == CONFIG_LOG_HIDE))
					event->cfg.log = CONFIG_LOG_HIDE;

				if (im_menu_item("Timeout", "", args->cfg->log == CONFIG_LOG_TIMEOUT))
					event->cfg.log = CONFIG_LOG_TIMEOUT;

				if (im_menu_item("Always", "", args->cfg->log == CONFIG_LOG_ALWAYS))
					event->cfg.log = CONFIG_LOG_ALWAYS;

				im_end_menu();
			}

			im_separator();

			if (im_menu_item("Quit", "", false))
				event->type = UI_EVENT_QUIT;

			im_end_menu();
		}

		if (im_begin_menu("Video", true)) {
			if (im_begin_menu("Window", true)) {
				if (im_menu_item("Fullscreen", "Ctrl+W", args->fullscreen))
					event->cfg.fullscreen = !event->cfg.fullscreen;

				if (im_menu_item("Reset", "", false))
					event->type = UI_EVENT_RESET;

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

			if (im_begin_menu("Palette", true)) {
				if (im_menu_item("Smooth", "", args->cfg->nes.palette == NES_PALETTE_SMOOTH))
					event->cfg.nes.palette = NES_PALETTE_SMOOTH;

				if (im_menu_item("NES Classic", "", args->cfg->nes.palette == NES_PALETTE_CLASSIC))
					event->cfg.nes.palette = NES_PALETTE_CLASSIC;

				if (im_menu_item("Composite Direct", "", args->cfg->nes.palette == NES_PALETTE_COMPOSITE))
					event->cfg.nes.palette = NES_PALETTE_COMPOSITE;

				if (im_menu_item("PVM Style D93", "", args->cfg->nes.palette == NES_PALETTE_PVM_D93))
					event->cfg.nes.palette = NES_PALETTE_PVM_D93;

				if (im_menu_item("PC-10", "", args->cfg->nes.palette == NES_PALETTE_PC10))
					event->cfg.nes.palette = NES_PALETTE_PC10;

				if (im_menu_item("Sony CXA", "", args->cfg->nes.palette == NES_PALETTE_SONY_CXA))
					event->cfg.nes.palette = NES_PALETTE_SONY_CXA;

				if (im_menu_item("Wavebeam", "", args->cfg->nes.palette == NES_PALETTE_WAVEBEAM))
					event->cfg.nes.palette = NES_PALETTE_WAVEBEAM;

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

			if (im_begin_menu("Clear Overscan", true)) {
				if (im_menu_item("Top", "", args->cfg->overscan.top))
					event->cfg.overscan.top = !event->cfg.overscan.top;

				if (im_menu_item("Right", "", args->cfg->overscan.right))
					event->cfg.overscan.right = !event->cfg.overscan.right;

				if (im_menu_item("Bottom", "", args->cfg->overscan.bottom))
					event->cfg.overscan.bottom = !event->cfg.overscan.bottom;

				if (im_menu_item("Left", "", args->cfg->overscan.left))
					event->cfg.overscan.left = !event->cfg.overscan.left;

				im_end_menu();
			}
			im_end_menu();
		}

		if (im_begin_menu("Audio", true)) {
			if (im_menu_item(args->cfg->mute ? "Unmute" : "Mute", "Ctrl+M", false))
				event->cfg.mute = !event->cfg.mute;

			if (im_menu_item("Stereo", "", args->cfg->nes.stereo))
				event->cfg.nes.stereo = !event->cfg.nes.stereo;

			if (im_begin_menu("Sample Rate", true)) {
				if (im_menu_item("48000", "", args->cfg->nes.sampleRate == 48000))
					event->cfg.nes.sampleRate = 48000;

				if (im_menu_item("44100", "", args->cfg->nes.sampleRate == 44100))
					event->cfg.nes.sampleRate = 44100;

				if (im_menu_item("22050", "", args->cfg->nes.sampleRate == 22050))
					event->cfg.nes.sampleRate = 22050;

				if (im_menu_item("16000", "", args->cfg->nes.sampleRate == 16000))
					event->cfg.nes.sampleRate = 16000;

				if (im_menu_item("11025", "", args->cfg->nes.sampleRate == 11025))
					event->cfg.nes.sampleRate = 11025;

				if (im_menu_item("8000", "", args->cfg->nes.sampleRate == 8000))
					event->cfg.nes.sampleRate = 8000;

				im_end_menu();
			}

			if (im_begin_menu("Channels", true)) {
				if (im_menu_item("Square 1", "", args->cfg->nes.channels & NES_CHANNEL_PULSE_0))
					event->cfg.nes.channels ^= NES_CHANNEL_PULSE_0;

				if (im_menu_item("Square 2", "", args->cfg->nes.channels & NES_CHANNEL_PULSE_1))
					event->cfg.nes.channels ^= NES_CHANNEL_PULSE_1;

				if (im_menu_item("Triangle", "", args->cfg->nes.channels & NES_CHANNEL_TRIANGLE))
					event->cfg.nes.channels ^= NES_CHANNEL_TRIANGLE;

				if (im_menu_item("Noise", "", args->cfg->nes.channels & NES_CHANNEL_NOISE))
					event->cfg.nes.channels ^= NES_CHANNEL_NOISE;

				if (im_menu_item("DMC", "", args->cfg->nes.channels & NES_CHANNEL_DMC))
					event->cfg.nes.channels ^= NES_CHANNEL_DMC;

				if (im_menu_item("Mapper 1", "", args->cfg->nes.channels & NES_CHANNEL_EXT_0))
					event->cfg.nes.channels ^= NES_CHANNEL_EXT_0;

				if (im_menu_item("Mapper 2", "", args->cfg->nes.channels & NES_CHANNEL_EXT_1))
					event->cfg.nes.channels ^= NES_CHANNEL_EXT_1;

				if (im_menu_item("Mapper 3", "", args->cfg->nes.channels & NES_CHANNEL_EXT_2))
					event->cfg.nes.channels ^= NES_CHANNEL_EXT_2;

				im_end_menu();
			}
			im_end_menu();
		}

		im_end_main_menu();
	}
}

static void ui_hotkeys(const struct ui_args *args, struct ui_event *event)
{
	if (im_key(MTY_SCANCODE_ESCAPE)) {
		CMP.ts = 0;

		if (CMP.nav & NAV_OPEN_ROM) {
			CMP.nav ^= NAV_OPEN_ROM;

		} else {
			CMP.nav ^= NAV_MENU;
		}
	}

	if (im_key(MTY_SCANCODE_W) && im_ctrl())
		event->cfg.fullscreen = !event->cfg.fullscreen;

	if (im_key(MTY_SCANCODE_O) && im_ctrl())
		CMP.nav ^= NAV_OPEN_ROM;

	if (im_key(MTY_SCANCODE_P) && im_ctrl())
		event->type = UI_EVENT_PAUSE;

	if (im_key(MTY_SCANCODE_R) && im_ctrl())
		NES_Reset(args->nes, false);

	if (im_key(MTY_SCANCODE_T) && im_ctrl())
		NES_Reset(args->nes, true);

	if (im_key(MTY_SCANCODE_M) && im_ctrl())
		event->cfg.mute = !event->cfg.mute;

	for (uint8_t x = 0; x < 8; x++) {
		if (im_key(MTY_SCANCODE_1 + x) && im_ctrl()) {
			ui_load_state(args->nes, args->crc32, x + 1);

		} else if (im_key(MTY_SCANCODE_1 + x)) {
			ui_save_state(args->nes, args->crc32, x + 1);
		}
	}
}

void ui_root(const struct ui_args *args,
	void (*event_callback)(struct ui_event *event, void *opaque), const void *opaque)
{
	struct ui_event event = {0};
	event.type = UI_EVENT_NONE;
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
	im_push_style_f(ImGuiStyleVar_WindowBorderSize, 0);
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

	if (args->cfg->log != CONFIG_LOG_HIDE)
		ui_log(args->cfg->log == CONFIG_LOG_ALWAYS);

	im_pop_style(8);
	im_pop_color(19);

	if (memcmp(args->cfg, &event.cfg, sizeof(struct config)))
		event.type = UI_EVENT_CONFIG;

	if (memcmp(&event.cfg.nes, &args->cfg->nes, sizeof(NES_Config)))
		NES_SetConfig(args->nes, &event.cfg.nes);

	if (event.type != UI_EVENT_NONE)
		event_callback(&event, (void *) opaque);
}

void ui_close_menu(void)
{
	CMP.nav = NAV_NONE;
}

void ui_destroy(void)
{
	MTY_FsFreeList(&CMP.fi, CMP.fi_n);
	CMP.fi_n = 0;

	free(CMP.msg);

	memset(&CMP, 0, sizeof(struct component_state));
}
