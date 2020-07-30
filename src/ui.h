// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "config.h"
#include "core.h"

#define UI_LOG_LEN  128

#define APP_TITLE_MAX 1024

enum app_event_type {
	APP_EVENT_NONE        = 0,
	APP_EVENT_TITLE       = 1,
	APP_EVENT_LOAD_GAME   = 2,
	APP_EVENT_UNLOAD_GAME = 3,
	APP_EVENT_CONFIG      = 4,
	APP_EVENT_QUIT        = 5,
	APP_EVENT_PAUSE       = 6,
	APP_EVENT_GFX         = 7,
	APP_EVENT_CORE_OPT    = 8,
	APP_EVENT_CLEAR_OPTS  = 9,
};

struct app_event {
	enum app_event_type type;
	bool rt;

	// These should be unioned
	struct config cfg;
	MTY_GFX gfx;
	char title[APP_TITLE_MAX];
	char game[MTY_PATH_MAX];
	struct {
		char key[CORE_KEY_NAME_MAX];
		char val[CORE_OPT_NAME_MAX];
	} opt;
	bool fetch_core;
};

struct ui_args {
	const struct config *cfg;
	const MTY_JSON *systems;
	const char *content_name;
	bool paused;
	bool show_menu;
	bool fullscreen;
	MTY_GFX gfx;

	struct core *core;
};

void ui_root(const struct ui_args *args,
	void (*event_callback)(const struct app_event *event, void *opaque), const void *opaque);
void ui_set_message(const char *msg, int32_t timeout);
void ui_close_menu(void);
void ui_destroy(void);
