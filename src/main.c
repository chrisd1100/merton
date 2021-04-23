// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "matoya.h"
#include "im.h"

#include "ui.h"
#include "app.h"
#include "core.h"
#include "config.h"
#include "rsp.h"

#include "assets/font/font.h"

#define PCM_BUFFER  75
#define SAMPLE_RATE 48000

struct main_audio_packet {
	double fps;
	uint32_t sample_rate;
	int16_t data[CORE_SAMPLES_MAX];
	size_t frames;
};

struct main {
	struct core *core;

	char *content_name;
	MTY_App *app;
	MTY_JSON *systems;
	MTY_JSON *core_options;
	MTY_JSON *core_exts;
	MTY_Window window;
	MTY_Queue *rt_q;
	MTY_Queue *mt_q;
	MTY_Queue *a_q;
	struct config cfg;
	bool got_frame;
	bool running;
	bool paused;
	bool loaded;

	struct {
		uint32_t req;
		char file[MTY_PATH_MAX];
		char name[MTY_PATH_MAX];
	} core_fetch;
};


// Maps

static const enum core_button NES_KEYBOARD_MAP[MTY_KEY_MAX] = {
	[MTY_KEY_SEMICOLON] = CORE_BUTTON_A,
	[MTY_KEY_L]         = CORE_BUTTON_B,
	[MTY_KEY_O]         = CORE_BUTTON_X,
	[MTY_KEY_P]         = CORE_BUTTON_Y,
	[MTY_KEY_Q]         = CORE_BUTTON_L,
	[MTY_KEY_LBRACKET]  = CORE_BUTTON_R,
	[MTY_KEY_LSHIFT]    = CORE_BUTTON_SELECT,
	[MTY_KEY_SPACE]     = CORE_BUTTON_START,
	[MTY_KEY_W]         = CORE_BUTTON_DPAD_U,
	[MTY_KEY_S]         = CORE_BUTTON_DPAD_D,
	[MTY_KEY_A]         = CORE_BUTTON_DPAD_L,
	[MTY_KEY_D]         = CORE_BUTTON_DPAD_R,
};


// Config

static struct config main_load_config(MTY_JSON **core_options, MTY_JSON **core_exts)
{
	struct config cfg = {0};

	MTY_JSON *jcfg = MTY_JSONReadFile(MTY_JoinPath(MTY_GetProcessDir(), "config.json"));
	if (!jcfg)
		jcfg = MTY_JSONObjCreate();

	#define CFG_GET_BOOL(name, def) \
		if (!MTY_JSONObjGetBool(jcfg, #name, &cfg.name)) cfg.name = def

	#define CFG_GET_UINT(name, def) \
		if (!MTY_JSONObjGetUInt(jcfg, #name, (uint32_t *) &cfg.name)) cfg.name = def

	#define CFG_GET_STR(name, size, def) \
		if (!MTY_JSONObjGetString(jcfg, #name, cfg.name, size)) snprintf(cfg.name, size, def);

	CFG_GET_BOOL(bg_pause, false);
	CFG_GET_BOOL(console, false);
	CFG_GET_BOOL(fullscreen, false);
	CFG_GET_BOOL(mute, false);
	CFG_GET_UINT(reduce_latency, 0);
	CFG_GET_UINT(frame_size, 0);
	CFG_GET_UINT(gfx, MTY_GetDefaultGFX());
	CFG_GET_UINT(filter, MTY_FILTER_GAUSSIAN_SHARP);
	CFG_GET_UINT(effect, MTY_EFFECT_NONE);
	CFG_GET_UINT(aspect_ratio.x, 0);
	CFG_GET_UINT(aspect_ratio.y, 0);
	CFG_GET_UINT(window.w, 1024);
	CFG_GET_UINT(window.h, 576);

	CFG_GET_STR(core.atari2600, CONFIG_CORE_MAX, "stella");
	CFG_GET_STR(core.gameboy, CONFIG_CORE_MAX, "sameboy");
	CFG_GET_STR(core.gba, CONFIG_CORE_MAX, "mgba");
	CFG_GET_STR(core.genesis, CONFIG_CORE_MAX, "genesis-plus-gx");
	CFG_GET_STR(core.ms, CONFIG_CORE_MAX, "genesis-plus-gx");
	CFG_GET_STR(core.n64, CONFIG_CORE_MAX, "parallel-n64");
	CFG_GET_STR(core.nes, CONFIG_CORE_MAX, "mesen");
	CFG_GET_STR(core.ps, CONFIG_CORE_MAX, "mednafen-psx");
	CFG_GET_STR(core.snes, CONFIG_CORE_MAX, "bsnes");
	CFG_GET_STR(core.tg16, CONFIG_CORE_MAX, "mednafen-pce");

	const MTY_JSON *obj = MTY_JSONObjGetItem(jcfg, "core_options");
	*core_options = obj ? MTY_JSONDuplicate(obj) : MTY_JSONObjCreate();

	obj = MTY_JSONObjGetItem(jcfg, "core_exts");
	if (obj) {
		*core_exts = MTY_JSONDuplicate(obj);

	} else {
		*core_exts = MTY_JSONObjCreate();
		MTY_JSONObjSetString(*core_exts, "atari2600", "a26");
		MTY_JSONObjSetString(*core_exts, "gameboy", "gb|gbc");
		MTY_JSONObjSetString(*core_exts, "gba", "gba");
		MTY_JSONObjSetString(*core_exts, "genesis", "gen|md|smd");
		MTY_JSONObjSetString(*core_exts, "ms", "sms");
		MTY_JSONObjSetString(*core_exts, "n64", "n64|v64|z64");
		MTY_JSONObjSetString(*core_exts, "nes", "nes|fds|unf|unif");
		MTY_JSONObjSetString(*core_exts, "ps", "cue");
		MTY_JSONObjSetString(*core_exts, "snes", "smc|sfc|bs");
	}

	MTY_JSONDestroy(&jcfg);

	return cfg;
}

static void main_save_config(struct config *cfg, const MTY_JSON *core_options, const MTY_JSON *core_exts)
{
	MTY_JSON *jcfg = MTY_JSONObjCreate();

	#define CFG_SET_BOOL(name) \
		MTY_JSONObjSetBool(jcfg, #name, cfg->name)

	#define CFG_SET_UINT(name) \
		MTY_JSONObjSetUInt(jcfg, #name, cfg->name)

	#define CFG_SET_STR(name) \
		MTY_JSONObjSetString(jcfg, #name, cfg->name)

	CFG_SET_BOOL(bg_pause);
	CFG_SET_BOOL(console);
	CFG_SET_BOOL(fullscreen);
	CFG_SET_BOOL(mute);
	CFG_SET_UINT(reduce_latency);
	CFG_SET_UINT(frame_size);
	CFG_SET_UINT(gfx);
	CFG_SET_UINT(filter);
	CFG_SET_UINT(effect);
	CFG_SET_UINT(aspect_ratio.x);
	CFG_SET_UINT(aspect_ratio.y);
	CFG_SET_UINT(window.w);
	CFG_SET_UINT(window.h);

	CFG_SET_STR(core.atari2600);
	CFG_SET_STR(core.gameboy);
	CFG_SET_STR(core.gba);
	CFG_SET_STR(core.genesis);
	CFG_SET_STR(core.ms);
	CFG_SET_STR(core.n64);
	CFG_SET_STR(core.nes);
	CFG_SET_STR(core.ps);
	CFG_SET_STR(core.snes);
	CFG_SET_STR(core.tg16);

	MTY_JSONObjSetItem(jcfg, "core_options", MTY_JSONDuplicate(core_options));
	MTY_JSONObjSetItem(jcfg, "core_exts", MTY_JSONDuplicate(core_exts));

	MTY_JSONWriteFile(MTY_JoinPath(MTY_GetProcessDir(), "config.json"), jcfg);

	MTY_JSONDestroy(&jcfg);
}


// Default systems

static MTY_JSON *main_system_add(MTY_JSON *j, const char *name, const char *label)
{
	MTY_JSON *system = MTY_JSONObjCreate();
	MTY_JSONObjSetItem(j, name, system);

	MTY_JSONObjSetString(system, "name", label);

	MTY_JSON *cores = MTY_JSONArrayCreate();
	MTY_JSONObjSetItem(system, "cores", cores);

	return cores;
}

static void main_system_add_core(MTY_JSON *cores, const char *name, const char *sha256)
{
	MTY_JSON *core = MTY_JSONObjCreate();
	MTY_JSONObjSetString(core, "name", name);
	MTY_JSONObjSetString(core, "sha256", sha256);

	MTY_JSONArrayAppendItem(cores, core);
}

static void main_write_default_systems(void)
{
	MTY_JSON *j = MTY_JSONObjCreate();

	MTY_JSON *cores = main_system_add(j, "atari2600", "Atari 2600");
	main_system_add_core(cores, "stella", "");

	cores = main_system_add(j, "gameboy", "Game Boy");
	main_system_add_core(cores, "gambatte", "");
	main_system_add_core(cores, "sameboy", "");
	main_system_add_core(cores, "mgba", "");

	cores = main_system_add(j, "gba", "Game Boy Advance");
	main_system_add_core(cores, "mgba", "");

	cores = main_system_add(j, "genesis", "Genesis");
	main_system_add_core(cores, "genesis-plus-gx", "");
	main_system_add_core(cores, "picodrive", "");

	cores = main_system_add(j, "ms", "Master System");
	main_system_add_core(cores, "genesis-plus-gx", "");
	main_system_add_core(cores, "picodrive", "");

	cores = main_system_add(j, "n64", "N64");
	main_system_add_core(cores, "mupen64plus-next", "");
	main_system_add_core(cores, "parallel-n64", "");

	cores = main_system_add(j, "nes", "NES");
	main_system_add_core(cores, "merton-nes", "");
	main_system_add_core(cores, "mesen", "");
	main_system_add_core(cores, "nestopia", "");
	main_system_add_core(cores, "quicknes", "");

	cores = main_system_add(j, "ps", "PlayStation");
	main_system_add_core(cores, "mednafen-psx", "");
	main_system_add_core(cores, "pcsx-rearmed", "");

	cores = main_system_add(j, "snes", "SNES");
	main_system_add_core(cores, "bsnes", "");
	main_system_add_core(cores, "mesen-s", "");
	main_system_add_core(cores, "snes9x", "");

	cores = main_system_add(j, "tg16", "TurboGrafx-16");
	main_system_add_core(cores, "mednafen-pce", "");

	MTY_JSONWriteFile(MTY_JoinPath(MTY_GetProcessDir(), "systems.json"), j);
}


// Dynamic core fetching

static void main_push_app_event(const struct app_event *evt, void *opaque);

static void main_fetch_core(struct main *ctx, const char *file, const char *name)
{
	snprintf(ctx->core_fetch.name, MTY_PATH_MAX, "%s", name);
	snprintf(ctx->core_fetch.file, MTY_PATH_MAX, "%s", file);

	MTY_HttpAsyncRequest(&ctx->core_fetch.req, "merton.matoya.group", 0, true,
		"GET", MTY_SprintfDL("/cores/%s", file), NULL, NULL, 0, 10000, NULL);
}

static void main_poll_core_fetch(struct main *ctx)
{
	uint16_t status = 0;
	void *so = NULL;
	size_t size = 0;

	MTY_Async async = MTY_HttpAsyncPoll(ctx->core_fetch.req, &so, &size, &status);

	if (async == MTY_ASYNC_OK) {
		if (status == 200) {
			const char *base = MTY_JoinPath(MTY_GetProcessDir(), "cores");
			MTY_Mkdir(base);

			const char *path = MTY_JoinPath(base, ctx->core_fetch.file);

			if (MTY_WriteFile(path, so, size)) {
				struct app_event evt = {0};
				evt.type = APP_EVENT_LOAD_GAME;
				evt.rt = true;
				evt.fetch_core = false;
				snprintf(evt.game, MTY_PATH_MAX, "%s", ctx->core_fetch.name);
				main_push_app_event(&evt, ctx);
			}
		}

		MTY_HttpAsyncClear(&ctx->core_fetch.req);
	}
}


// Core

static void main_video(const void *buf, uint32_t width, uint32_t height, size_t pitch, void *opaque)
{
	struct main *ctx = (struct main *) opaque;
	ctx->got_frame = true;

	// A NULL buffer means we should render the previous frame
	enum core_color_format format = buf ? core_get_color_format(ctx->core) :
		CORE_COLOR_FORMAT_UNKNOWN;

	MTY_RenderDesc desc = {0};
	desc.format =
		format == CORE_COLOR_FORMAT_BGRA ? MTY_COLOR_FORMAT_BGRA :
		format == CORE_COLOR_FORMAT_B5G6R5 ? MTY_COLOR_FORMAT_BGR565 :
		format == CORE_COLOR_FORMAT_B5G5R5A1 ? MTY_COLOR_FORMAT_BGRA5551 :
		MTY_COLOR_FORMAT_UNKNOWN;

	desc.imageWidth = (uint32_t) (format == CORE_COLOR_FORMAT_BGRA ? pitch / 4 : pitch / 2);
	desc.imageHeight = height;
	desc.cropWidth = width;
	desc.cropHeight = height;
	desc.scale = (float) ctx->cfg.frame_size;
	desc.filter = ctx->cfg.filter;
	desc.effect = ctx->cfg.effect;

	desc.aspectRatio = ctx->cfg.aspect_ratio.y == 0 ?
		core_get_aspect_ratio(ctx->core) : (float) ctx->cfg.aspect_ratio.x / (float) ctx->cfg.aspect_ratio.y;

	MTY_WindowDrawQuad(ctx->app, ctx->window, buf, &desc);
}

static void main_audio(const int16_t *buf, size_t frames, void *opaque)
{
	struct main *ctx = opaque;

	struct main_audio_packet *pkt = MTY_QueueGetInputBuffer(ctx->a_q);

	if (pkt) {
		pkt->sample_rate = core_get_sample_rate(ctx->core);
		pkt->fps = core_get_frame_rate(ctx->core);
		pkt->frames = frames;

		memcpy(pkt->data, buf, frames * 4);

		MTY_QueuePush(ctx->a_q, sizeof(struct main_audio_packet));
	}
}

static void main_log(const char *msg, void *opaque)
{
	printf("%s", msg);
}

static void main_get_system_by_ext(struct main *ctx, const char *name,
	const char **core, const char **system)
{
	*core = NULL;
	*system = NULL;

	const char *ext = MTY_GetFileExtension(name);

	uint32_t len = MTY_JSONGetLength(ctx->core_exts);

	for (uint32_t x = 0; x < len; x++) {
		const char *key = MTY_JSONObjGetKey(ctx->core_exts, x);

		char exts[SYSTEM_EXTS_MAX];
		if (MTY_JSONObjGetString(ctx->core_exts, key, exts, SYSTEM_EXTS_MAX)) {
			if (MTY_Strcasestr(exts, ext)) {
				*core = CONFIG_GET_CORE(&ctx->cfg, key);
				*system = key;
				break;
			}
		}
	}
}

static void main_set_core_options(struct main *ctx)
{
	uint32_t len = MTY_JSONGetLength(ctx->core_options);
	for (uint32_t x = 0; x < len; x++) {
		const char *key = MTY_JSONObjGetKey(ctx->core_options, x);

		char val[CORE_OPT_NAME_MAX];
		if (MTY_JSONObjGetString(ctx->core_options, key, val, CORE_OPT_NAME_MAX))
			core_set_variable(ctx->core, key, val);
	}
}

static void main_read_sram(struct core *core, const char *content_name)
{
	const char *name = MTY_SprintfDL("%s.srm", content_name);

	size_t size = 0;
	void *sram = MTY_ReadFile(MTY_JoinPath(core_get_save_dir(core), name), &size);
	if (sram) {
		core_set_sram(core, sram, size);
		MTY_Free(sram);
	}
}

static void main_save_sram(struct core *core, const char *content_name)
{
	if (!content_name)
		return;

	size_t size = 0;
	void *sram = core_get_sram(core, &size);
	if (sram) {
		const char *name = MTY_SprintfDL("%s.srm", content_name);

		MTY_WriteFile(MTY_JoinPath(core_get_save_dir(core), name), sram, size);
		MTY_Free(sram);
	}
}

static void main_load_game(struct main *ctx, const char *name, bool fetch_core)
{
	const char *core = NULL;
	const char *system = NULL;
	main_get_system_by_ext(ctx, name, &core, &system);
	if (!core)
		return;

	const char *cname = MTY_SprintfDL("%s.%s", core, MTY_GetSOExtension());
	const char *core_path = MTY_JoinPath(MTY_JoinPath(MTY_GetProcessDir(), "cores"), cname);

	// If core is on the system, try to use it
	if (MTY_FileExists(core_path)) {
		main_save_sram(ctx->core, ctx->content_name);
		MTY_Free(ctx->content_name);
		ctx->content_name = NULL;

		core_unload(&ctx->core);

		ctx->core = core_load(core_path);
		if (!ctx->core)
			return;

		main_set_core_options(ctx);

		core_set_log_func(main_log, &ctx);
		core_set_audio_func(ctx->core, main_audio, ctx);
		core_set_video_func(ctx->core, main_video, ctx);

		ctx->loaded = core_load_game(ctx->core, name);
		if (!ctx->loaded)
			return;

		ui_set_message("Press ESC to access the menu", 3000);

		ctx->content_name = MTY_Strdup(MTY_GetFileName(name, false));
		main_read_sram(ctx->core, ctx->content_name);

		struct app_event evt = {0};
		evt.type = APP_EVENT_TITLE;
		snprintf(evt.title, APP_TITLE_MAX, "%s - %s", APP_NAME, ctx->content_name);
		main_push_app_event(&evt, ctx);

	// Get the core from the internet
	} else if (fetch_core) {
		main_fetch_core(ctx, cname, name);
	}
}


// App events

static void main_push_app_event(const struct app_event *evt, void *opaque)
{
	struct main *ctx = opaque;

	MTY_Queue *q = evt->rt ? ctx->rt_q : ctx->mt_q;

	struct app_event *qbuf = MTY_QueueGetInputBuffer(q);
	if (qbuf) {
		*qbuf = *evt;
		MTY_QueuePush(q, sizeof(struct app_event));
	}
}

static void main_poll_app_events(struct main *ctx, MTY_Queue *q)
{
	for (struct app_event *evt = NULL; MTY_QueueGetOutputBuffer(q, 0, (void **) &evt, NULL);) {
		switch (evt->type) {
			case APP_EVENT_CONFIG:
				// Fullscreen/windowed transitions
				if (evt->cfg.fullscreen != ctx->cfg.fullscreen)
					MTY_WindowSetFullscreen(ctx->app, ctx->window, evt->cfg.fullscreen);

				// Graphics API change
				if (evt->cfg.gfx != ctx->cfg.gfx) {
					struct app_event gevt = {0};
					gevt.rt = true;
					gevt.type = APP_EVENT_GFX;
					gevt.gfx = evt->cfg.gfx;
					main_push_app_event(&gevt, ctx);
				}

				// Console
				if (evt->cfg.console != ctx->cfg.console) {
					if (evt->cfg.console) {
						MTY_OpenConsole(APP_NAME);

					} else {
						MTY_CloseConsole();
					}
				}

				ctx->cfg = evt->cfg;
				break;
			case APP_EVENT_QUIT:
				ctx->running = false;
				break;
			case APP_EVENT_GFX:
				MTY_WindowSetGFX(ctx->app, ctx->window, evt->gfx, true);
				MTY_WindowMakeCurrent(ctx->app, ctx->window, true);
				break;
			case APP_EVENT_PAUSE:
				ctx->paused = !ctx->paused;
				break;
			case APP_EVENT_TITLE:
				MTY_WindowSetTitle(ctx->app, ctx->window, evt->title);
				break;
			case APP_EVENT_LOAD_GAME:
				ui_close_menu();
				main_load_game(ctx, evt->game, evt->fetch_core);
				break;
			case APP_EVENT_UNLOAD_GAME: {
				core_unload_game(ctx->core);
				ctx->got_frame = false;

				struct app_event tevt = {0};
				tevt.type = APP_EVENT_TITLE;
				snprintf(tevt.title, APP_TITLE_MAX, "%s", APP_NAME);
				main_push_app_event(&tevt, ctx);
				break;
			}
			case APP_EVENT_CLEAR_OPTS:
				MTY_JSONDestroy(&ctx->core_options);
				ctx->core_options = MTY_JSONObjCreate();
				core_clear_variables(ctx->core);
				break;
			case APP_EVENT_CORE_OPT:
				MTY_JSONObjSetString(ctx->core_options, evt->opt.key, evt->opt.val);
				core_set_variable(ctx->core, evt->opt.key, evt->opt.val);
				break;
			default:
				break;
		}

		MTY_QueuePop(q);
	}
}


// Audio thread

static void *main_audio_thread(void *opaque)
{
	struct main *ctx = opaque;

	MTY_Audio *audio = MTY_AudioCreate(48000, PCM_BUFFER, PCM_BUFFER * 2);
	if (!audio)
		return NULL;

	struct rsp *rsp = rsp_create();

	uint32_t sample_rate = 0;
	uint32_t target_rate = 0;
	bool correct_high = false;
	bool correct_low = false;

	while (ctx->running) {
		struct main_audio_packet *pkt = NULL;

		while (MTY_QueueGetOutputBuffer(ctx->a_q, 10, (void **) &pkt, NULL)) {
			#define TARGET_RATE(rate, fps) \
				((double) (rate) * (1.0 - ((60.0 - (fps)) / (fps))))

			// Reset resampler on sample rate changes
			if (sample_rate != pkt->sample_rate) {
				rsp_reset(rsp);

				sample_rate = pkt->sample_rate;
				target_rate = lrint(TARGET_RATE(SAMPLE_RATE, pkt->fps));
				correct_high = correct_low = false;
			}

			// Submit the audio
			if (!ctx->cfg.mute) {
				const int16_t *rsp_buf = rsp_convert(rsp, sample_rate, target_rate,
					pkt->data, &pkt->frames);

				MTY_AudioQueue(audio, rsp_buf, (uint32_t) pkt->frames);
			}

			// Correct buffer drift by tweaking the output sample rate
			uint32_t low = PCM_BUFFER / 2;
			uint32_t mid = PCM_BUFFER;
			uint32_t high = PCM_BUFFER + low;
			uint32_t queued = MTY_AudioGetQueued(audio);

			if (queued <= mid)
				correct_high = false;

			if (queued >= mid)
				correct_low = false;

			if (!correct_high && !correct_low) {
				if (queued >= high) {
					correct_high = true;
					target_rate = lrint(TARGET_RATE(SAMPLE_RATE, pkt->fps) * 0.995);

				} else if (queued <= low) {
					correct_low = true;
					target_rate = lrint(TARGET_RATE(SAMPLE_RATE, pkt->fps) * 1.005);

				} else {
					target_rate = lrint(TARGET_RATE(SAMPLE_RATE, pkt->fps));
				}
			}

			MTY_QueuePop(ctx->a_q);
		}
	}

	rsp_destroy(&rsp);
	MTY_AudioDestroy(&audio);

	return NULL;
}


// Render thread

static void main_im_root(void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	struct ui_args args = {0};
	args.content_name = ctx->content_name;
	args.systems = ctx->systems;
	args.core = ctx->core;
	args.cfg = &ctx->cfg;
	args.paused = ctx->paused;
	args.show_menu = !ctx->loaded;
	args.fullscreen = MTY_WindowIsFullscreen(ctx->app, ctx->window);
	args.gfx = MTY_WindowGetGFX(ctx->app, ctx->window);
	ctx->loaded = true;

	ui_root(&args, main_push_app_event, ctx);
}

static uint32_t main_calc_interval(MTY_GFX gfx, uint32_t rr, MTY_Time stamp, float *rem)
{
	float i = (float) rr / 60.0f;
	float ims = 1000.0f / (float) rr;

	float diff = MTY_TimeDiff(stamp, MTY_GetTime());
	float next = roundf(i + *rem);
	float missed = floorf(diff / ims);

	// D3D9 is not aware of frame skipping
	if (gfx == MTY_GFX_D3D9 && missed >= 1.0f) {
		next -= missed;
		*rem = 0;

		if (next < 1.0f)
			next = 1.0f;

	} else {
		*rem += i - next;

		if (*rem > 1.0f)
			*rem = 0.0f;
	}

	return lrint(next);
}

static void *main_render_thread(void *opaque)
{
	struct main *ctx = opaque;
	float rem = 0.0f;

	MTY_WindowSetGFX(ctx->app, ctx->window, ctx->cfg.gfx, true);
	MTY_WindowMakeCurrent(ctx->app, ctx->window, true);

	while (ctx->running) {
		MTY_Time stamp = MTY_GetTime();

		main_poll_app_events(ctx, ctx->rt_q);
		main_poll_core_fetch(ctx);

		if (MTY_WindowIsActive(ctx->app, ctx->window) || !ctx->cfg.bg_pause) {
			ctx->got_frame = false;

			// Wait longer for input to come in before running frame
			if (ctx->cfg.reduce_latency > 0)
				MTY_Sleep(ctx->cfg.reduce_latency);

			if (!ctx->paused) {
				core_run_frame(ctx->core);

			} else {
				main_video(NULL, 0, 0, 0, ctx);
			}

			uint32_t window_width = 0;
			uint32_t window_height = 0;
			MTY_WindowGetSize(ctx->app, ctx->window, &window_width, &window_height);

			float scale = MTY_WindowGetScreenScale(ctx->app, ctx->window);
			if (!MTY_WindowHasUITexture(ctx->app, ctx->window, IM_FONT_ID)) {
				int32_t width = 0;
				int32_t height = 0;
				void *font = im_get_font(font_compressed_data, font_compressed_size,
					18.0f, scale, &width, &height);
				MTY_WindowSetUITexture(ctx->app, ctx->window, IM_FONT_ID, font, width, height);
				MTY_Free(font);
			}

			const MTY_DrawData *dd = im_draw(window_width, window_height, scale,
				!ctx->got_frame, main_im_root, ctx);

			MTY_WindowDrawUI(ctx->app, ctx->window, dd);

			MTY_GFX gfx = MTY_WindowGetGFX(ctx->app, ctx->window);
			uint32_t rr = MTY_WindowGetRefreshRate(ctx->app, ctx->window);
			MTY_WindowPresent(ctx->app, ctx->window, main_calc_interval(gfx, rr, stamp, &rem));

		} else {
			MTY_Sleep(8);
		}
	}

	MTY_WindowSetGFX(ctx->app, ctx->window, MTY_GFX_NONE, false);

	main_save_sram(ctx->core, ctx->content_name);
	MTY_Free(ctx->content_name);

	core_unload(&ctx->core);

	ui_destroy();

	return NULL;
}


// Main thread

static void main_event_func(const MTY_Event *evt, void *opaque)
{
	struct main *ctx = opaque;

	im_input(evt);

	switch (evt->type) {
		case MTY_EVENT_CLOSE:
			ctx->running = false;
			break;
		case MTY_EVENT_DROP: {
			struct app_event devt = {0};
			devt.type = APP_EVENT_LOAD_GAME;
			devt.fetch_core = true;
			devt.rt = true;
			snprintf(devt.game, MTY_PATH_MAX, "%s", evt->drop.name);
			main_push_app_event(&devt, ctx);
			break;
		}
		case MTY_EVENT_KEY: {
			enum core_button button = NES_KEYBOARD_MAP[evt->key.key];
			if (button != 0)
				core_set_button(ctx->core, 0, button, evt->key.pressed);
			break;
		}
		case MTY_EVENT_CONTROLLER: {
			const MTY_ControllerEvent *c = &evt->controller;

			#define REV_AXIS(axis) \
				((axis) == INT16_MAX ? INT16_MIN : (axis) == INT16_MIN ? INT16_MAX : -(axis))

			core_set_button(ctx->core, 0, CORE_BUTTON_A, c->buttons[MTY_CBUTTON_B]);
			core_set_button(ctx->core, 0, CORE_BUTTON_B, c->buttons[MTY_CBUTTON_A]);
			core_set_button(ctx->core, 0, CORE_BUTTON_X, c->buttons[MTY_CBUTTON_Y]);
			core_set_button(ctx->core, 0, CORE_BUTTON_Y, c->buttons[MTY_CBUTTON_X]);
			core_set_button(ctx->core, 0, CORE_BUTTON_SELECT, c->buttons[MTY_CBUTTON_BACK]);
			core_set_button(ctx->core, 0, CORE_BUTTON_START, c->buttons[MTY_CBUTTON_START]);
			core_set_button(ctx->core, 0, CORE_BUTTON_L, c->buttons[MTY_CBUTTON_LEFT_SHOULDER]);
			core_set_button(ctx->core, 0, CORE_BUTTON_R, c->buttons[MTY_CBUTTON_RIGHT_SHOULDER]);
			core_set_button(ctx->core, 0, CORE_BUTTON_DPAD_U, MTY_DPAD_UP(c));
			core_set_button(ctx->core, 0, CORE_BUTTON_DPAD_D, MTY_DPAD_DOWN(c));
			core_set_button(ctx->core, 0, CORE_BUTTON_DPAD_L, MTY_DPAD_LEFT(c));
			core_set_button(ctx->core, 0, CORE_BUTTON_DPAD_R, MTY_DPAD_RIGHT(c));

			core_set_button(ctx->core, 0, CORE_BUTTON_L2, c->buttons[MTY_CBUTTON_LEFT_TRIGGER]);
			core_set_button(ctx->core, 0, CORE_BUTTON_R2, c->buttons[MTY_CBUTTON_RIGHT_TRIGGER]);

			core_set_axis(ctx->core, 0, CORE_AXIS_LX, c->axes[MTY_CAXIS_THUMB_LX].value);
			core_set_axis(ctx->core, 0, CORE_AXIS_LY, REV_AXIS(c->axes[MTY_CAXIS_THUMB_LY].value));
			core_set_axis(ctx->core, 0, CORE_AXIS_RX, c->axes[MTY_CAXIS_THUMB_RX].value);
			core_set_axis(ctx->core, 0, CORE_AXIS_RY, REV_AXIS(c->axes[MTY_CAXIS_THUMB_RY].value));

			break;
		}
		default:
			break;
	}
}

static bool main_app_func(void *opaque)
{
	struct main *ctx = opaque;

	main_poll_app_events(ctx, ctx->mt_q);

	return ctx->running;
}

static void main_mty_log_callback(const char *msg, void *opaque)
{
	printf("%s\n", msg);
}

int32_t main(int32_t argc, char **argv)
{
	im_create();
	main_write_default_systems();

	MTY_HttpAsyncCreate(4);

	struct main ctx = {0};
	ctx.cfg = main_load_config(&ctx.core_options, &ctx.core_exts);
	ctx.running = true;

	if (ctx.cfg.console)
		MTY_OpenConsole(APP_NAME);

	const char *path = MTY_JoinPath(MTY_GetProcessDir(), "systems.json");
	ctx.systems = MTY_JSONReadFile(path);

	MTY_SetTimerResolution(1);
	MTY_SetLogFunc(main_mty_log_callback, NULL);

	ctx.rt_q = MTY_QueueCreate(50, sizeof(struct app_event));
	ctx.mt_q = MTY_QueueCreate(50, sizeof(struct app_event));
	ctx.a_q = MTY_QueueCreate(5, sizeof(struct main_audio_packet));

	if (argc >= 2) {
		struct app_event evt = {0};
		evt.type = APP_EVENT_LOAD_GAME;
		evt.fetch_core = true;
		evt.rt = true;
		snprintf(evt.game, MTY_PATH_MAX, "%s", argv[1]);
		main_push_app_event(&evt, &ctx);
	}

	ctx.app = MTY_AppCreate(main_app_func, main_event_func, &ctx);
	MTY_AppSetTimeout(ctx.app, 1);

	MTY_WindowDesc desc = {0};
	desc.title = APP_NAME;
	desc.width = ctx.cfg.window.w;
	desc.height = ctx.cfg.window.h;
	desc.fullscreen = ctx.cfg.fullscreen;

	ctx.window = MTY_WindowCreate(ctx.app, &desc);
	if (ctx.window == -1)
		goto except;

	MTY_Thread *rt = MTY_ThreadCreate(main_render_thread, &ctx);
	MTY_Thread *at = MTY_ThreadCreate(main_audio_thread, &ctx);
	MTY_AppRun(ctx.app);
	MTY_ThreadDestroy(&at);
	MTY_ThreadDestroy(&rt);

	main_save_config(&ctx.cfg, ctx.core_options, ctx.core_exts);

	except:

	MTY_RevertTimerResolution(1);
	MTY_AppDestroy(&ctx.app);
	MTY_QueueDestroy(&ctx.rt_q);
	MTY_QueueDestroy(&ctx.mt_q);
	MTY_QueueDestroy(&ctx.a_q);
	MTY_JSONDestroy(&ctx.systems);
	MTY_JSONDestroy(&ctx.core_options);
	MTY_JSONDestroy(&ctx.core_exts);

	MTY_HttpAsyncDestroy();

	im_destroy();

	return 0;
}

#if defined(_WIN32)

#include <windows.h>

int32_t WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int32_t nCmdShow)
{
	hInstance; hPrevInstance; lpCmdLine; nCmdShow;

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	return main(__argc, __argv);
}
#endif
