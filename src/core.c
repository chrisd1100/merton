// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "core.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "matoya.h"
#include "deps/libretro.h"

#define CORE_VARIABLES_MAX 128

struct core {
	MTY_SO *so;
	bool game_loaded;
	char *game_path;
	char *game_data;
	size_t game_data_size;

	struct retro_system_info system_info;

	void (RETRO_CALLCONV *retro_set_environment)(retro_environment_t);
	void (RETRO_CALLCONV *retro_set_video_refresh)(retro_video_refresh_t);
	void (RETRO_CALLCONV *retro_set_audio_sample)(retro_audio_sample_t);
	void (RETRO_CALLCONV *retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
	void (RETRO_CALLCONV *retro_set_input_poll)(retro_input_poll_t);
	void (RETRO_CALLCONV *retro_set_input_state)(retro_input_state_t);
	void (RETRO_CALLCONV *retro_init)(void);
	void (RETRO_CALLCONV *retro_deinit)(void);
	unsigned (RETRO_CALLCONV *retro_api_version)(void);
	void (RETRO_CALLCONV *retro_get_system_info)(struct retro_system_info *info);
	void (RETRO_CALLCONV *retro_get_system_av_info)(struct retro_system_av_info *info);
	void (RETRO_CALLCONV *retro_set_controller_port_device)(unsigned port, unsigned device);
	void (RETRO_CALLCONV *retro_reset)(void);
	void (RETRO_CALLCONV *retro_run)(void);
	size_t (RETRO_CALLCONV *retro_serialize_size)(void);
	bool (RETRO_CALLCONV *retro_serialize)(void *data, size_t size);
	bool (RETRO_CALLCONV *retro_unserialize)(const void *data, size_t size);
	//void (RETRO_CALLCONV *retro_cheat_reset)(void);
	//void (RETRO_CALLCONV *retro_cheat_set)(unsigned index, bool enabled, const char *code);
	bool (RETRO_CALLCONV *retro_load_game)(const struct retro_game_info *game);
	//bool (RETRO_CALLCONV *retro_load_game_special)(unsigned game_type,
	//	const struct retro_game_info *info, size_t num_info);
	void (RETRO_CALLCONV *retro_unload_game)(void);
	unsigned (RETRO_CALLCONV *retro_get_region)(void);
	void *(RETRO_CALLCONV *retro_get_memory_data)(unsigned id);
	size_t (RETRO_CALLCONV *retro_get_memory_size)(unsigned id);
};


// Globals from the environment callback

static enum retro_pixel_format RETRO_PIXEL_FORMAT = RETRO_PIXEL_FORMAT_0RGB1555;
static struct retro_game_geometry RETRO_GAME_GEOMETRY;
static struct retro_system_timing RETRO_SYSTEM_TIMING;
static unsigned RETRO_REGION;

static uint32_t CORE_NUM_VARIABLES;
static struct core_variable CORE_VARIABLES[CORE_VARIABLES_MAX];
static MTY_Hash *CORE_OPTS;
static bool CORE_OPT_SET;

static CORE_LOG_FUNC CORE_LOG;
static CORE_AUDIO_FUNC CORE_AUDIO;
static CORE_VIDEO_FUNC CORE_VIDEO;
static void *CORE_LOG_OPAQUE;
static void *CORE_AUDIO_OPAQUE;
static void *CORE_VIDEO_OPAQUE;

static char CORE_SAVE_DIR[MTY_PATH_MAX];
static char CORE_SYSTEM_DIR[MTY_PATH_MAX];

static bool CORE_BUTTONS[CORE_PLAYERS_MAX][CORE_BUTTON_MAX];
static int16_t CORE_AXES[CORE_PLAYERS_MAX][CORE_AXIS_MAX];

static size_t CORE_NUM_FRAMES;
static int16_t CORE_FRAMES[CORE_SAMPLES_MAX];


// Maps

static const enum core_button CORE_BUTTON_MAP[16] = {
	[RETRO_DEVICE_ID_JOYPAD_B]      = CORE_BUTTON_B,
	[RETRO_DEVICE_ID_JOYPAD_Y]      = CORE_BUTTON_Y,
	[RETRO_DEVICE_ID_JOYPAD_SELECT] = CORE_BUTTON_SELECT,
	[RETRO_DEVICE_ID_JOYPAD_START]  = CORE_BUTTON_START,
	[RETRO_DEVICE_ID_JOYPAD_UP]     = CORE_BUTTON_DPAD_U,
	[RETRO_DEVICE_ID_JOYPAD_DOWN]   = CORE_BUTTON_DPAD_D,
	[RETRO_DEVICE_ID_JOYPAD_LEFT]   = CORE_BUTTON_DPAD_L,
	[RETRO_DEVICE_ID_JOYPAD_RIGHT]  = CORE_BUTTON_DPAD_R,
	[RETRO_DEVICE_ID_JOYPAD_A]      = CORE_BUTTON_A,
	[RETRO_DEVICE_ID_JOYPAD_X]      = CORE_BUTTON_X,
	[RETRO_DEVICE_ID_JOYPAD_L]      = CORE_BUTTON_L,
	[RETRO_DEVICE_ID_JOYPAD_R]      = CORE_BUTTON_R,
	[RETRO_DEVICE_ID_JOYPAD_L2]     = CORE_BUTTON_L2,
	[RETRO_DEVICE_ID_JOYPAD_R2]     = CORE_BUTTON_R2,
	[RETRO_DEVICE_ID_JOYPAD_L3]     = 0,
	[RETRO_DEVICE_ID_JOYPAD_R3]     = 0,
};

static const enum core_axis CORE_AXIS_MAP[3][2] = {
	[RETRO_DEVICE_INDEX_ANALOG_LEFT] = {
		[RETRO_DEVICE_ID_ANALOG_X] = CORE_AXIS_LX,
		[RETRO_DEVICE_ID_ANALOG_Y] = CORE_AXIS_LY,
	},
	[RETRO_DEVICE_INDEX_ANALOG_RIGHT] = {
		[RETRO_DEVICE_ID_ANALOG_X] = CORE_AXIS_RX,
		[RETRO_DEVICE_ID_ANALOG_Y] = CORE_AXIS_RY,
	},
	[RETRO_DEVICE_INDEX_ANALOG_BUTTON] = {
		[RETRO_DEVICE_ID_ANALOG_X] = 0,
		[RETRO_DEVICE_ID_ANALOG_Y] = 0,
	},
};


// libretro callbacks

static void core_retro_log_printf(enum retro_log_level level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char *msg = MTY_VsprintfD(fmt, args);

	va_end(args);

	if (CORE_LOG)
		CORE_LOG(msg, CORE_LOG_OPAQUE);

	MTY_Free(msg);
}

static void core_parse_variable(struct core_variable *var, const char *key, const char *val)
{
	snprintf(var->key, CORE_KEY_NAME_MAX, "%s", key);

	char *dup = MTY_Strdup(val);

	char *semi = strstr(dup, ";");
	if (semi) {
		*semi = '\0';
		snprintf(var->desc, CORE_DESC_MAX, "%s", dup);

		semi++;
		while (*semi == ' ')
			semi++;

		char *ptr = NULL;
		char *tok = MTY_Strtok(semi, "|", &ptr);

		while (tok && var->nopts < CORE_OPTS_MAX) {
			snprintf(var->opts[var->nopts++], CORE_OPT_NAME_MAX, "%s", tok);
			tok = MTY_Strtok(NULL, "|", &ptr);
		}
	}

	MTY_Free(dup);
}

static const char *core_variable_default(const char *key)
{
	// Hardware rendering is not set up, make sure to use
	// available software renderers

	if (!strcmp(key, "mupen64plus-rdp-plugin"))
		return "angrylion";

	if (!strcmp(key, "parallel-n64-gfxplugin"))
		return "angrylion";

	return NULL;
}

static bool core_retro_environment(unsigned cmd, void *data)
{
	switch (cmd) {
		case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
		case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION:
		case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: {
			unsigned *arg = data;
			*arg = 0;

			return true;
		}
		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
			struct retro_log_callback *arg = data;
			arg->log = core_retro_log_printf;

			return true;
		}
		case RETRO_ENVIRONMENT_SET_MESSAGE: {
			const struct retro_message *arg = data;

			if (CORE_LOG)
				CORE_LOG(arg->msg, CORE_LOG_OPAQUE);

			return true;
		}
		case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
			bool *arg = data;
			*arg = true;

			return true;
		}
		case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
			const char **arg = data;

			*arg = CORE_SYSTEM_DIR;
			MTY_Mkdir(*arg);

			return true;
		}
		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
			const char **arg = data;

			*arg = CORE_SAVE_DIR;
			MTY_Mkdir(*arg);

			return true;
		}
		case RETRO_ENVIRONMENT_GET_VARIABLE: {
			struct retro_variable *arg = data;
			arg->value = MTY_HashGet(CORE_OPTS, arg->key);

			return arg->value ? true : false;
		}
		case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
			int *arg = data;
			*arg = 0x3;

			return true;
		}
		case RETRO_ENVIRONMENT_GET_LANGUAGE: {
			unsigned *arg = data;
			*arg = RETRO_LANGUAGE_ENGLISH;

			return true;
		}
		case RETRO_ENVIRONMENT_SET_VARIABLES: {
			const struct retro_variable *arg = data;

			for (uint32_t x = 0; x < UINT32_MAX; x++) {
				const struct retro_variable *v = &arg[x];
				if (!v->key || !v->value)
					break;

				if (CORE_NUM_VARIABLES < CORE_VARIABLES_MAX) {
					core_parse_variable(&CORE_VARIABLES[CORE_NUM_VARIABLES], v->key, v->value);

					if (!MTY_HashGet(CORE_OPTS, v->key)) {
						const char *default_val = core_variable_default(v->key);

						if (!default_val)
							default_val = CORE_VARIABLES[CORE_NUM_VARIABLES].opts[0];

						if (default_val[0])
							MTY_HashSet(CORE_OPTS, v->key, MTY_Strdup(default_val));
					}

					CORE_NUM_VARIABLES++;
				}
			}

			return true;
		}
		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
			bool *arg = data;
			*arg = CORE_OPT_SET;
			CORE_OPT_SET = false;

			return true;
		}
		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
			const enum retro_pixel_format *arg = data;

			RETRO_PIXEL_FORMAT = *arg;

			return true;
		}
		case RETRO_ENVIRONMENT_SET_GEOMETRY: {
			const struct retro_game_geometry *arg = data;
			RETRO_GAME_GEOMETRY = *arg;

			return true;
		}
		case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
			const struct retro_system_av_info *arg = data;
			RETRO_GAME_GEOMETRY = arg->geometry;
			RETRO_SYSTEM_TIMING = arg->timing;

			return true;
		}

		// TODO
		case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
			printf("RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK\n");
			break;
		case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
			printf("RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO\n");
			break;
		case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
			printf("RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE\n");
			break;
		case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
			printf("RETRO_ENVIRONMENT_SET_CONTROLLER_INFO\n");
			break;
		case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
			printf("RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS\n");
			break;
		case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
			printf("RETRO_ENVIRONMENT_GET_VFS_INTERFACE\n");
			break;
		case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
			printf("RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS\n");
			break;
		case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
			printf("RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE\n");
			break;
		case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
			printf("RETRO_ENVIRONMENT_GET_PERF_INTERFACE\n");
			break;
		case RETRO_ENVIRONMENT_SET_HW_RENDER:
			printf("RETRO_ENVIRONMENT_SET_HW_RENDER\n");
			break;

		// Unimplemented
		case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
			// NES / SNES memory maps for cheats and other advanced emulator features
		case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
			// Preallocated memory for drawing, overhead is minimal on current systems
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
			// Optionally hide certain settings
		case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
			// Perfomance demands hint
		case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
			// Achievement system?
		case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
		case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
			// Merton handles this with resampling and libmatoya's min/max buffers
		case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
			// This is an optional efficiency for polling controllers
		case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
			return false;

		// Unknown
		default:
			printf("*** RETRO_ENVIRONMENT: %u, %p\n", cmd, data);
			break;
	}

	return false;
}

static void core_retro_video_refresh(const void *data, unsigned width,
	unsigned height, size_t pitch)
{
	if (CORE_VIDEO)
		CORE_VIDEO(data, width, height, pitch, CORE_VIDEO_OPAQUE);
}

static void core_retro_audio_sample(int16_t left, int16_t right)
{
	if (CORE_NUM_FRAMES + 1 <= CORE_FRAMES_MAX) {
		CORE_FRAMES[CORE_NUM_FRAMES * 2] = left;
		CORE_FRAMES[CORE_NUM_FRAMES * 2 + 1] = right;
		CORE_NUM_FRAMES++;
	}
}

static size_t core_retro_audio_sample_batch(const int16_t *data, size_t frames)
{
	if (CORE_NUM_FRAMES + frames <= CORE_FRAMES_MAX) {
		memcpy(CORE_FRAMES + CORE_NUM_FRAMES * 2, data, frames * 4);
		CORE_NUM_FRAMES += frames;
	}

	return frames;
}

static void core_retro_input_poll(void)
{
}

static int16_t core_retro_input_state(unsigned port, unsigned device,
	unsigned index, unsigned id)
{
	if (port >= CORE_PLAYERS_MAX)
		return 0;

	// Buttons
	if (device == RETRO_DEVICE_JOYPAD) {
		if (id >= 16)
			return 0;

		return CORE_BUTTONS[port][CORE_BUTTON_MAP[id]];

	// Axes
	} else if (device == RETRO_DEVICE_ANALOG) {
		if (index >= 3 || id >= 2)
			return 0;

		return CORE_AXES[port][CORE_AXIS_MAP[index][id]];
	}

	return 0;
}


// Core API

static bool core_load_symbols(struct core *ctx)
{
	#define CORE_LOAD_SYM(sym) \
		ctx->sym = MTY_SOGetSymbol(ctx->so, #sym); \
		if (!ctx->sym) return false

	CORE_LOAD_SYM(retro_set_environment);
	CORE_LOAD_SYM(retro_set_video_refresh);
	CORE_LOAD_SYM(retro_set_audio_sample);
	CORE_LOAD_SYM(retro_set_audio_sample_batch);
	CORE_LOAD_SYM(retro_set_input_poll);
	CORE_LOAD_SYM(retro_set_input_state);
	CORE_LOAD_SYM(retro_init);
	CORE_LOAD_SYM(retro_deinit);
	CORE_LOAD_SYM(retro_api_version);
	CORE_LOAD_SYM(retro_get_system_info);
	CORE_LOAD_SYM(retro_get_system_av_info);
	CORE_LOAD_SYM(retro_set_controller_port_device);
	CORE_LOAD_SYM(retro_reset);
	CORE_LOAD_SYM(retro_run);
	CORE_LOAD_SYM(retro_serialize_size);
	CORE_LOAD_SYM(retro_serialize);
	CORE_LOAD_SYM(retro_unserialize);
	CORE_LOAD_SYM(retro_load_game);
	CORE_LOAD_SYM(retro_unload_game);
	CORE_LOAD_SYM(retro_get_region);
	CORE_LOAD_SYM(retro_get_memory_data);
	CORE_LOAD_SYM(retro_get_memory_size);

	return true;
}

struct core *core_load(const char *name)
{
	struct core *ctx = MTY_Alloc(1, sizeof(struct core));

	bool r = true;

	snprintf(CORE_SAVE_DIR, MTY_PATH_MAX, "%s", MTY_JoinPath(MTY_GetProcessDir(), "save"));
	snprintf(CORE_SYSTEM_DIR, MTY_PATH_MAX, "%s", MTY_JoinPath(MTY_GetProcessDir(), "system"));

	CORE_OPTS = MTY_HashCreate(0);

	ctx->so = MTY_SOLoad(name);
	if (!ctx->so) {
		r = false;
		goto except;
	}

	r = core_load_symbols(ctx);
	if (!r)
		goto except;

	r = ctx->retro_api_version() == RETRO_API_VERSION;
	if (!r)
		goto except;

	ctx->retro_set_environment(core_retro_environment);
	ctx->retro_init();

	ctx->retro_set_video_refresh(core_retro_video_refresh);
	ctx->retro_set_audio_sample(core_retro_audio_sample);
	ctx->retro_set_audio_sample_batch(core_retro_audio_sample_batch);
	ctx->retro_set_input_poll(core_retro_input_poll);
	ctx->retro_set_input_state(core_retro_input_state);

	ctx->retro_get_system_info(&ctx->system_info);

	except:

	if (!r)
		core_unload(&ctx);

	return ctx;
}

void core_unload(struct core **core)
{
	if (!core || !*core)
		return;

	struct core *ctx = *core;

	core_unload_game(ctx);

	if (ctx->retro_deinit)
		ctx->retro_deinit();

	MTY_SOUnload(&ctx->so);
	MTY_Free(ctx->game_path);
	MTY_Free(ctx->game_data);

	// Globals
	RETRO_PIXEL_FORMAT = RETRO_PIXEL_FORMAT_0RGB1555;
	memset(&RETRO_GAME_GEOMETRY, 0, sizeof(struct retro_game_geometry));
	memset(&RETRO_SYSTEM_TIMING, 0, sizeof(struct retro_system_timing));
	RETRO_REGION = 0;

	CORE_NUM_VARIABLES = 0;
	memset(CORE_VARIABLES, 0, sizeof(struct core_variable) * CORE_VARIABLES_MAX);

	MTY_HashDestroy(&CORE_OPTS, MTY_Free);
	CORE_OPT_SET = false;

	CORE_LOG = NULL;
	CORE_AUDIO = NULL;
	CORE_VIDEO = NULL;
	CORE_LOG_OPAQUE = NULL;
	CORE_AUDIO_OPAQUE = NULL;
	CORE_VIDEO_OPAQUE = NULL;
	CORE_NUM_FRAMES = 0;

	memset(CORE_BUTTONS, 0, sizeof(bool) * CORE_PLAYERS_MAX * CORE_BUTTON_MAX);
	memset(CORE_AXES, 0, sizeof(int16_t) * CORE_PLAYERS_MAX * CORE_AXIS_MAX);

	MTY_Free(ctx);
	*core = NULL;
}

bool core_load_game(struct core *ctx, const char *path)
{
	if (!ctx)
		return false;

	core_unload_game(ctx);

	MTY_Free(ctx->game_path);
	ctx->game_path = MTY_Strdup(path);

	MTY_Free(ctx->game_data);
	ctx->game_data = NULL;
	ctx->game_data_size = 0;

	struct retro_game_info game = {0};
	game.path = ctx->game_path;
	game.meta = "merton";

	if (!ctx->system_info.need_fullpath) {
		ctx->game_data = MTY_ReadFile(path, &ctx->game_data_size);
		if (!ctx->game_data)
			return false;

		game.data = ctx->game_data;
		game.size = ctx->game_data_size;
	}

	ctx->game_loaded = ctx->retro_load_game(&game);

	if (ctx->game_loaded) {
		ctx->retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
		ctx->retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);

		struct retro_system_av_info av_info = {0};
		ctx->retro_get_system_av_info(&av_info);
		RETRO_SYSTEM_TIMING = av_info.timing;
		RETRO_GAME_GEOMETRY = av_info.geometry;

		RETRO_REGION = ctx->retro_get_region();
	}

	return ctx->game_loaded;
}

void core_unload_game(struct core *ctx)
{
	if (!ctx || !ctx->game_loaded)
		return;

	ctx->retro_unload_game();
	ctx->game_loaded = false;
}

void core_reset_game(struct core *ctx)
{
	if (!ctx || !ctx->game_loaded)
		return;

	ctx->retro_reset();
}

void core_run_frame(struct core *ctx)
{
	if (!ctx || !ctx->game_loaded)
		return;

	ctx->retro_run();

	if (CORE_AUDIO) {
		CORE_AUDIO(CORE_FRAMES, CORE_NUM_FRAMES, CORE_AUDIO_OPAQUE);
		CORE_NUM_FRAMES = 0;
	}
}

enum core_color_format core_get_color_format(struct core *ctx)
{
	if (!ctx)
		return CORE_COLOR_FORMAT_UNKNOWN;

	switch (RETRO_PIXEL_FORMAT) {
		case RETRO_PIXEL_FORMAT_XRGB8888: return CORE_COLOR_FORMAT_BGRA;
		case RETRO_PIXEL_FORMAT_RGB565:   return CORE_COLOR_FORMAT_B5G6R5;
		case RETRO_PIXEL_FORMAT_0RGB1555: return CORE_COLOR_FORMAT_B5G5R5A1;
	}

	return CORE_COLOR_FORMAT_UNKNOWN;
}

uint32_t core_get_sample_rate(struct core *ctx)
{
	if (!ctx || !ctx->game_loaded)
		return 0;

	return lrint(RETRO_SYSTEM_TIMING.sample_rate);
}

double core_get_frame_rate(struct core *ctx)
{
	if (!ctx || !ctx->game_loaded)
		return 0;

	return RETRO_SYSTEM_TIMING.fps;
}

float core_get_aspect_ratio(struct core *ctx)
{
	if (!ctx || !ctx->game_loaded)
		return 0.0f;

	float ar = RETRO_GAME_GEOMETRY.aspect_ratio;

	if (ar <= 0.0f)
		ar = (float) RETRO_GAME_GEOMETRY.base_width / (float) RETRO_GAME_GEOMETRY.base_height;

	return ar;
}

void core_set_button(struct core *ctx, uint8_t player, enum core_button button, bool pressed)
{
	if (!ctx)
		return;

	CORE_BUTTONS[player][button] = pressed;
}

void core_set_axis(struct core *ctx, uint8_t player, enum core_axis axis, int16_t value)
{
	if (!ctx)
		return;

	CORE_AXES[player][axis] = value;
}

void *core_get_state(struct core *ctx, size_t *size)
{
	if (!ctx || !ctx->game_loaded)
		return NULL;

	*size = ctx->retro_serialize_size();
	if (*size == 0)
		return NULL;

	void *state = MTY_Alloc(*size, 1);
	if (!ctx->retro_serialize(state, *size)) {
		MTY_Free(state);
		return NULL;
	}

	return state;
}

bool core_set_state(struct core *ctx, const void *state, size_t size)
{
	if (!ctx || !ctx->game_loaded)
		return false;

	return ctx->retro_unserialize(state, size);
}

void *core_get_sram(struct core *ctx, size_t *size)
{
	if (!ctx || !ctx->game_loaded)
		return NULL;

	*size = ctx->retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (*size == 0)
		return NULL;

	void *sram = ctx->retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
	if (!sram)
		return NULL;

	void *dup = MTY_Alloc(*size, 1);
	memcpy(dup, sram, *size);

	return dup;
}

bool core_set_sram(struct core *ctx, const void *sram, size_t size)
{
	if (!ctx || !ctx->game_loaded)
		return false;

	if (ctx->retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) != size)
		return false;

	void *cur_sram = ctx->retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
	if (!cur_sram)
		return false;

	memcpy(cur_sram, sram, size);

	return true;
}

const char *core_get_save_dir(struct core *ctx)
{
	if (!ctx)
		return NULL;

	return CORE_SAVE_DIR;
}

const char *core_get_game_path(struct core *ctx)
{
	if (!ctx)
		return NULL;

	return ctx->game_path;
}

bool core_game_is_loaded(struct core *ctx)
{
	return ctx ? ctx->game_loaded : false;
}

void core_set_log_func(CORE_LOG_FUNC func, void *opaque)
{
	CORE_LOG = func;
	CORE_LOG_OPAQUE = opaque;
}

void core_set_audio_func(struct core *ctx, CORE_AUDIO_FUNC func, void *opaque)
{
	if (!ctx)
		return;

	CORE_AUDIO = func;
	CORE_AUDIO_OPAQUE = opaque;
}

void core_set_video_func(struct core *ctx, CORE_VIDEO_FUNC func, void *opaque)
{
	if (!ctx)
		return;

	CORE_VIDEO = func;
	CORE_VIDEO_OPAQUE = opaque;
}

const struct core_variable *core_get_variables(struct core *ctx, uint32_t *len)
{
	*len = CORE_NUM_VARIABLES;

	return CORE_VARIABLES;
}

void core_set_variable(struct core *ctx, const char *key, const char *val)
{
	if (!ctx)
		return;

	MTY_Free(MTY_HashSet(CORE_OPTS, key, MTY_Strdup(val)));

	CORE_OPT_SET = true;
}

const char *core_get_variable(struct core *ctx, const char *key)
{
	if (!ctx)
		return NULL;

	return MTY_HashGet(CORE_OPTS, key);
}

void core_clear_variables(struct core *ctx)
{
	MTY_HashDestroy(&CORE_OPTS, MTY_Free);
	CORE_OPTS = MTY_HashCreate(0);

	for (uint32_t x = 0; x < CORE_NUM_VARIABLES; x++)
		MTY_HashSet(CORE_OPTS, CORE_VARIABLES[x].key, MTY_Strdup(CORE_VARIABLES[x].opts[0]));

	CORE_OPT_SET = true;
}
