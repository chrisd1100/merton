#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "nes/nes.h"
#include "matoya.h"
#include "deps/imgui/im.h"

#include "ui.h"
#include "app.h"
#include "config.h"

#include "assets/db/nes20db.h"
#include "assets/font/anonymous.h"

struct main {
	NES *nes;
	uint32_t crc32;
	MTY_Window *window;
	MTY_Audio *audio;
	struct config cfg;
	bool running;
	bool paused;
	bool loaded;

	// Video
	uint32_t cropped[NES_FRAME_WIDTH * NES_FRAME_HEIGHT];
	uint8_t sync;

	// Audio
	uint32_t cycles;
	uint32_t frames;
	int64_t ts;
};


/*** NES CALLBACKS ***/

static void main_crop_copy(uint32_t *dest, const uint32_t *src, uint32_t top, uint32_t right,
	uint32_t bottom, uint32_t left)
{
	int32_t adjx = (right + left) / 2;
	int32_t adjy = (bottom + top) / 2;

	for (uint32_t row = top; row < NES_FRAME_HEIGHT - bottom; row++)
		memcpy(dest + ((row - top) + adjy) * NES_FRAME_WIDTH + adjx,
			src + row * NES_FRAME_WIDTH + left, 4 * (NES_FRAME_WIDTH - left - right));
}

static void main_nes_video(const uint32_t *frame, void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	if (frame)
		main_crop_copy(ctx->cropped, frame, ctx->cfg.overscan.top ? 8 : 0, ctx->cfg.overscan.right ? 8 : 0,
			ctx->cfg.overscan.bottom ? 8 : 0, ctx->cfg.overscan.left ? 8 : 0);

	MTY_WindowRenderQuad(ctx->window, ctx->cropped, NES_FRAME_WIDTH, NES_FRAME_HEIGHT,
		ctx->cfg.frame_size * NES_FRAME_WIDTH, ctx->cfg.frame_size * NES_FRAME_HEIGHT,
		(float) ctx->cfg.aspect_ratio.x / (float) ctx->cfg.aspect_ratio.y, ctx->cfg.filter, ctx->cfg.effect);
}

static void main_nes_audio(const int16_t *frames, uint32_t count, void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	if (!ctx->cfg.mute)
		MTY_AudioQueue(ctx->audio, frames, count);
}

static void main_nes_log(const char *str)
{
	ui_add_log(str, 3000);
}


/*** ROM / SRAM LOADING ***/

static bool main_get_desc_from_db(uint32_t offset, uint32_t crc32, NES_CartDesc *desc)
{
	for (uint32_t x = 0; x < NES_DB_ROWS; x++) {
		const uint8_t *row = NES_DB + x * NES_DB_ROW_SIZE;

		if (crc32 == *((uint32_t *) row)) {
			desc->offset = offset;
			desc->prgROMSize = row[4] * 0x4000;
			desc->chrROMSize = row[9] * 0x2000;
			desc->mapper = *((uint16_t *) (row + 14));
			desc->submapper = row[16] & 0xF;
			desc->mirror = (row[16] & 0x10) ? NES_MIRROR_VERTICAL :
				(row[16] & 0x20) ? NES_MIRROR_FOUR : NES_MIRROR_HORIZONTAL;
			desc->battery = row[16] & 0x80;
			desc->prgWRAMSize = *((uint16_t *) (row + 5)) * 8;
			desc->prgSRAMSize = *((uint16_t *) (row + 7)) * 8;
			desc->chrWRAMSize = *((uint16_t *) (row + 10)) * 8;
			desc->chrSRAMSize = *((uint16_t *) (row + 12)) * 8;

			return true;
		}
	}

	return false;
}

static bool main_load_rom(struct main *ctx, const char *name)
{
	size_t rom_size = 0;
	uint8_t *rom = NULL;

	if (MTY_FsRead(name, (void **) &rom, &rom_size) && rom_size > 16) {
		uint32_t offset = 16 + ((rom[6] & 0x04) ? 512 : 0); // iNES and optional trainer

		if (rom_size > offset) {
			ctx->crc32 = MTY_CryptoCRC32(rom + offset, rom_size - offset);

			ui_clear_log();
			ui_set_message("Press ESC to access the menu", 3000);

			char sram_name[16];
			snprintf(sram_name, 16, "%02X.sav", ctx->crc32);

			NES_CartDesc desc = {0};
			bool found_in_db = ctx->cfg.use_db ? main_get_desc_from_db(offset, ctx->crc32, &desc) : false;

			if (found_in_db) {
				char msg[UI_LOG_LEN];
				snprintf(msg, UI_LOG_LEN, "%02X found in database", ctx->crc32);
				ui_add_log(msg, 3000);
			}

			size_t sram_size = 0;
			void *sram = NULL;
			MTY_FsRead(MTY_FsPath(MTY_FsGetDir(MTY_DIR_PROGRAM), MTY_FsPath("save", sram_name)), &sram, &sram_size);
			NES_LoadCart(ctx->nes, rom, rom_size, sram, sram_size, found_in_db ? &desc : NULL);
			free(sram);
			free(rom);

			MTY_WindowSetTitle(ctx->window, APP_NAME, MTY_FsName(name, false));

			return true;
		}
	}

	return false;
}

static void main_save_sram(struct main *ctx)
{
	if (ctx->crc32 == 0)
		return;

	size_t sram_size = NES_SRAMDirty(ctx->nes);

	if (sram_size > 0) {
		void *sram = calloc(sram_size, 1);
		NES_GetSRAM(ctx->nes, sram, sram_size);

		char sram_name[16];
		snprintf(sram_name, 16, "%02X.sav", ctx->crc32);

		const char *save_path = MTY_FsPath(MTY_FsGetDir(MTY_DIR_PROGRAM), "save");
		MTY_FsMkdir(save_path);
		MTY_FsWrite(MTY_FsPath(save_path, sram_name), sram, sram_size);
		free(sram);
	}
}


/*** WINDOW MSG / INPUT HANDLING ***/

static const NES_Button NES_KEYBOARD_MAP[MTY_SCANCODE_MAX] = {
	[MTY_SCANCODE_SEMICOLON] = NES_BUTTON_A,
	[MTY_SCANCODE_L]         = NES_BUTTON_B,
	[MTY_SCANCODE_LSHIFT]    = NES_BUTTON_SELECT,
	[MTY_SCANCODE_SPACE]     = NES_BUTTON_START,
	[MTY_SCANCODE_W]         = NES_BUTTON_UP,
	[MTY_SCANCODE_S]         = NES_BUTTON_DOWN,
	[MTY_SCANCODE_A]         = NES_BUTTON_LEFT,
	[MTY_SCANCODE_D]         = NES_BUTTON_RIGHT,
};

static void main_window_msg_func(MTY_WindowMsg *wmsg, const void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	im_input(wmsg);

	switch (wmsg->type) {
		case MTY_WINDOW_MSG_CLOSE:
			ctx->running = false;
			break;
		case MTY_WINDOW_MSG_DRAG:
			main_save_sram(ctx);
			main_load_rom(ctx, wmsg->drag.name);
			ui_close_menu();
			break;
		case MTY_WINDOW_MSG_KEYBOARD: {
			NES_Button button = NES_KEYBOARD_MAP[wmsg->keyboard.scancode];
			if (button != 0)
				NES_ControllerButton(ctx->nes, 0, button, wmsg->keyboard.pressed);
			break;
		}
		case MTY_WINDOW_MSG_GAMEPAD: {
			uint8_t state = 0;
			state |= wmsg->gamepad.a     ? NES_BUTTON_A : 0;
			state |= wmsg->gamepad.b     ? NES_BUTTON_B : 0;
			state |= wmsg->gamepad.back  ? NES_BUTTON_SELECT : 0;
			state |= wmsg->gamepad.start ? NES_BUTTON_START : 0;
			state |= wmsg->gamepad.up    ? NES_BUTTON_UP : 0;
			state |= wmsg->gamepad.down  ? NES_BUTTON_DOWN : 0;
			state |= wmsg->gamepad.left  ? NES_BUTTON_LEFT : 0;
			state |= wmsg->gamepad.right ? NES_BUTTON_RIGHT : 0;

			state |= wmsg->gamepad.leftThumbX < -50 ? NES_BUTTON_LEFT : 0;
			state |= wmsg->gamepad.leftThumbX > 50  ? NES_BUTTON_RIGHT : 0;
			state |= wmsg->gamepad.leftThumbY < -50 ? NES_BUTTON_UP : 0;
			state |= wmsg->gamepad.leftThumbY > 50  ? NES_BUTTON_DOWN : 0;

			NES_ControllerState(ctx->nes, 0, state);
			break;
		}
		default:
			break;
	}
}


/*** AUDIO / VIDEO TIMING & SYNCHRONIZATION ***/

static const uint8_t PATTERN_60[]  = {1};
static const uint8_t PATTERN_75[]  = {1, 1, 1, 2, 1, 1, 1, 2};
static const uint8_t PATTERN_85[]  = {1, 2, 1, 1, 2, 1, 2, 1, 2, 1, 2, 1};
static const uint8_t PATTERN_100[] = {1, 2, 2};
static const uint8_t PATTERN_120[] = {2};
static const uint8_t PATTERN_144[] = {2, 3, 2, 3, 2};

static uint32_t main_sync_to_60(struct main *ctx)
{
	uint32_t rr = MTY_WindowGetRefreshRate(ctx->window);

	const uint8_t *pattern = PATTERN_60;
	size_t pattern_len = sizeof(PATTERN_60);

	switch (rr) {
		case 75:
			pattern = PATTERN_75;
			pattern_len = sizeof(PATTERN_75);
			break;
		case 85:
			pattern = PATTERN_85;
			pattern_len = sizeof(PATTERN_85);
			break;
		case 100:
			pattern = PATTERN_100;
			pattern_len = sizeof(PATTERN_100);
			break;
		case 120:
			pattern = PATTERN_120;
			pattern_len = sizeof(PATTERN_120);
			break;
		case 144:
			pattern = PATTERN_144;
			pattern_len = sizeof(PATTERN_144);
			break;
	}

	if (++ctx->sync >= pattern_len)
		ctx->sync = 0;

	return pattern[ctx->sync];
}

static void main_audio_adjustment(struct main *ctx)
{
	uint32_t queued = MTY_AudioGetQueuedFrames(ctx->audio);

	uint32_t audio_start = 100 * (ctx->cfg.nes.sampleRate / 1000);
	uint32_t audio_buffer = 50 * (ctx->cfg.nes.sampleRate / 1000);

	if (queued >= audio_start && !MTY_AudioIsPlaying(ctx->audio))
		MTY_AudioPlay(ctx->audio);

	if (queued == 0 && MTY_AudioIsPlaying(ctx->audio))
		MTY_AudioStop(ctx->audio);

	if (++ctx->frames % 120 == 0) {
		int64_t now = MTY_Timestamp();

		if (ctx->ts != 0) {
			uint32_t cycles_sec = lrint(((double) ctx->cycles * 1000.0) / MTY_TimestampDiff(ctx->ts, now));
			NES_APUClockDrift(ctx->nes, cycles_sec, queued >= audio_buffer);
		}

		ctx->cycles = 0;
		ctx->ts = now;
	}
}


/*** UI ***/

static void main_ui_event(struct ui_event *event, void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	switch (event->type) {
		case UI_EVENT_CONFIG:
			// Zero cropped buffer on overscan changes
			if (event->cfg.overscan.top != ctx->cfg.overscan.top ||
				event->cfg.overscan.right != ctx->cfg.overscan.right ||
				event->cfg.overscan.bottom != ctx->cfg.overscan.bottom ||
				event->cfg.overscan.left != ctx->cfg.overscan.left)
					memset(ctx->cropped, 0, sizeof(ctx->cropped));

			// Audio device must be reset on sample rate changes
			if (event->cfg.nes.sampleRate != ctx->cfg.nes.sampleRate) {
				MTY_AudioDestroy(&ctx->audio);
				MTY_AudioCreate(&ctx->audio, event->cfg.nes.sampleRate);
			}

			// Fullscreen/windowed transitions
			if (event->cfg.fullscreen != ctx->cfg.fullscreen) {
				if (MTY_WindowIsFullscreen(ctx->window)) {
					MTY_WindowSetWindowed(ctx->window, ctx->cfg.window.w, ctx->cfg.window.h);

				} else {
					MTY_WindowSetFullscreen(ctx->window);
				}

				event->cfg.fullscreen = MTY_WindowIsFullscreen(ctx->window);
			}

			ctx->cfg = event->cfg;
			break;
		case UI_EVENT_QUIT:
			ctx->running = false;
			break;
		case UI_EVENT_PAUSE:
			ctx->paused = !ctx->paused;
			break;
		case UI_EVENT_OPEN_ROM:
			main_save_sram(ctx);
			main_load_rom(ctx, event->rom_name);
			break;
		case UI_EVENT_UNLOAD_ROM:
			MTY_WindowSetTitle(ctx->window, APP_NAME, NULL);
			break;
		case UI_EVENT_RESET:
			MTY_WindowSetWindowed(ctx->window, ctx->cfg.window.w, ctx->cfg.window.h);
			ctx->cfg.fullscreen = MTY_WindowIsFullscreen(ctx->window);
			break;
		default:
			break;
	}
}

static void main_im_root(void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	struct ui_args args = {0};
	args.nes = ctx->nes;
	args.crc32 = ctx->crc32;
	args.cfg = &ctx->cfg;
	args.paused = ctx->paused;
	args.show_menu = !ctx->loaded;
	ctx->loaded = true;

	ui_root(&args, main_ui_event, ctx);
}


/*** CONFIG ***/

static struct config main_load_config(void)
{
	size_t size = 0;
	struct config *cfg = NULL;
	bool ok = MTY_FsRead(MTY_FsPath(MTY_FsGetDir(MTY_DIR_PROGRAM), "config.bin"), (void **) &cfg, &size);

	struct config r = ok && size == sizeof(struct config) && cfg->version == CONFIG_VERSION ?
		*cfg : (struct config) CONFIG_DEFAULTS;

	free(cfg);
	return r;
}

static void main_save_config(struct config *cfg)
{
	MTY_FsWrite(MTY_FsPath(MTY_FsGetDir(MTY_DIR_PROGRAM), "config.bin"), cfg, sizeof(struct config));
}


/*** MAIN ***/

int32_t main(int32_t argc, char **argv)
{
	struct main ctx = {0};
	ctx.cfg = main_load_config();
	ctx.running = true;

	bool r = MTY_WindowCreate(APP_NAME, main_window_msg_func, &ctx,
		ctx.cfg.window.w, ctx.cfg.window.h, ctx.cfg.fullscreen, &ctx.window);
	if (!r) goto except;

	r = MTY_AudioCreate(&ctx.audio, ctx.cfg.nes.sampleRate);
	if (!r) goto except;

	NES_Create(&ctx.cfg.nes, &ctx.nes);
	NES_SetLogCallback(main_nes_log);

	im_create(anonymous_compressed_data, anonymous_compressed_size, 16.0f);

	if (argc >= 2)
		ctx.loaded = main_load_rom(&ctx, argv[1]);

	while (ctx.running) {
		int64_t ts = MTY_Timestamp();
		MTY_WindowPoll(ctx.window);

		if (MTY_WindowIsForeground(ctx.window) || !ctx.cfg.bg_pause) {
			main_audio_adjustment(&ctx);

			if (!ctx.paused) {
				ctx.cycles += NES_NextFrame(ctx.nes, main_nes_video, main_nes_audio, &ctx);

			} else {
				main_nes_video(NULL, &ctx);
			}

			MTY_Device *device = MTY_WindowGetDevice(ctx.window);
			MTY_Context *context = MTY_WindowGetContext(ctx.window);
			MTY_Texture *back_buffer = MTY_WindowGetBackBuffer(ctx.window);

			if (im_begin(MTY_WindowGetDPIScale(ctx.window), device, context, back_buffer)) {
				im_draw(main_im_root, &ctx);
				im_render(!NES_CartLoaded(ctx.nes));
			}

			MTY_WindowReleaseBackBuffer(back_buffer);
			double wait = floor(1000.0 / 60.0 - MTY_TimestampDiff(ts, MTY_Timestamp())) - 1.0;
			MTY_WindowPresent(ctx.window, main_sync_to_60(&ctx));

			if (ctx.cfg.reduce_latency && wait > 0.0)
				MTY_Sleep(lrint(wait));

		} else {
			ctx.ts = 0;
			MTY_Sleep(16);
		}
	}

	main_save_sram(&ctx);
	main_save_config(&ctx.cfg);

	except:

	ui_destroy();
	im_destroy();
	NES_Destroy(&ctx.nes);
	MTY_AudioDestroy(&ctx.audio);
	MTY_WindowDestroy(&ctx.window);

	return 0;
}

#if defined(_WIN32)

#include <windows.h>
#include <timeapi.h>

int32_t WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int32_t nCmdShow)
{
	hInstance; hPrevInstance; lpCmdLine; nCmdShow;

	/*
	AllocConsole();
	AttachConsole(GetCurrentProcessId());

	FILE *f = NULL;
	freopen_s(&f, "CONOUT$", "w", stdout);
	*/

	timeBeginPeriod(1);

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	int32_t r = main(__argc, __argv);

	timeEndPeriod(1);

	return r;
}
#endif
