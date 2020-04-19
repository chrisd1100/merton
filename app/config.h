#pragma once

#include <stdint.h>
#include <stdbool.h>

enum config_filter {
	CONFIG_FILTER_NEAREST = 1,
	CONFIG_FILTER_LINEAR  = 2,
};

enum config_shader {
	CONFIG_SHADER_NONE = 0,
};

struct config {
	// System
	bool bg_pause;

	// Video
	bool windowed;
	int32_t frame_size;
	enum config_filter filter;
	enum config_shader shader;
	struct {
		int32_t x;
		int32_t y;
	} aspect_ratio;
	struct {
		bool top;
		bool right;
		bool bottom;
		bool left;
	} overscan;

	// Audio
	bool stereo;
	int32_t sample_rate;
	struct {
		bool square1;
		bool square2;
		bool triangle;
		bool noise;
		bool dmc;
		bool mapper1;
		bool mapper2;
		bool mapper4;
	} channels;
};

#define CONFIG_DEFAULTS { \
	true, \
	true, \
	3, \
	CONFIG_FILTER_NEAREST, \
	CONFIG_SHADER_NONE, \
	{16, 15}, \
	{8, 0, 8, 0}, \
	true, \
	44100, \
	{true, true, true, true, true, true, true, true}, \
}
