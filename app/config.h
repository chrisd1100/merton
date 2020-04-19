#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "lib.h"
#include "../src/nes.h"

enum config_shader {
	CONFIG_SHADER_NONE = 0,
};

struct config {
	// System
	bool bg_pause;

	// Video
	bool fullscreen;
	int32_t frame_size;
	enum filter filter;
	enum config_shader shader;
	struct {
		int32_t x;
		int32_t y;
	} aspect_ratio;
	struct {
		int32_t w;
		int32_t h;
	} window;
	struct {
		bool top;
		bool right;
		bool bottom;
		bool left;
	} overscan;

	// Audio
	bool mute;
	bool stereo;
	int32_t sample_rate;
	uint32_t channels;
};

#define CONFIG_DEFAULTS { \
	true, \
	true, \
	3, \
	FILTER_NEAREST, \
	CONFIG_SHADER_NONE, \
	{16, 15}, \
	{NES_FRAME_WIDTH * 3, NES_FRAME_HEIGHT * 3}, \
	{8, 0, 8, 0}, \
	false, \
	true, \
	44100, \
	NES_CHANNEL_ALL, \
}
