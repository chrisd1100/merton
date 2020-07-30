// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "matoya.h"

#define CONFIG_CORE_MAX 64

#define SYSTEM_NAME_MAX 64
#define SYSTEM_EXTS_MAX 128

#define CONFIG_GET_CORE(cfg, name) ( \
	!strcmp(name, "atari2600") ? (cfg)->core.atari2600 : \
	!strcmp(name, "gameboy") ? (cfg)->core.gameboy : \
	!strcmp(name, "gba") ? (cfg)->core.gba : \
	!strcmp(name, "genesis") ? (cfg)->core.genesis : \
	!strcmp(name, "ms") ? (cfg)->core.ms : \
	!strcmp(name, "n64") ? (cfg)->core.n64 : \
	!strcmp(name, "nes") ? (cfg)->core.nes : \
	!strcmp(name, "ps") ? (cfg)->core.ps : \
	!strcmp(name, "snes") ? (cfg)->core.snes : \
	(cfg)->core.tg16 \
)

struct config {
	bool bg_pause;
	bool console;
	bool fullscreen;
	bool mute;
	uint32_t reduce_latency;
	uint32_t frame_size;

	MTY_GFX gfx;
	MTY_Filter filter;
	MTY_Effect effect;

	struct {
		uint32_t x;
		uint32_t y;
	} aspect_ratio;

	struct {
		uint32_t w;
		uint32_t h;
	} window;

	struct {
		char atari2600[CONFIG_CORE_MAX];
		char gameboy[CONFIG_CORE_MAX];
		char gba[CONFIG_CORE_MAX];
		char genesis[CONFIG_CORE_MAX];
		char ms[CONFIG_CORE_MAX];
		char n64[CONFIG_CORE_MAX];
		char nes[CONFIG_CORE_MAX];
		char ps[CONFIG_CORE_MAX];
		char snes[CONFIG_CORE_MAX];
		char tg16[CONFIG_CORE_MAX];
	} core;
};
