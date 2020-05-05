#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "nes/nes.h"
#include "lib/lib.h"
#include "deps/imgui/im.h"

#include "ui.h"
#include "config.h"

#include "assets/db/nes20db.h"
#include "assets/font/anonymous.h"

#define APP_NAME "Merton"

struct main {
	NES *nes;
	uint32_t crc32;
	struct window *window;
	struct audio *audio;
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

	window_render_quad(ctx->window, ctx->cropped, NES_FRAME_WIDTH, NES_FRAME_HEIGHT,
		ctx->cfg.frame_size * NES_FRAME_WIDTH, ctx->cfg.frame_size * NES_FRAME_HEIGHT,
		(float) ctx->cfg.aspect_ratio.x / (float) ctx->cfg.aspect_ratio.y, ctx->cfg.filter);
}

static void main_nes_audio(const int16_t *frames, uint32_t count, void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	if (!ctx->cfg.mute)
		audio_queue(ctx->audio, frames, count);
}

static void main_nes_log(const char *str)
{
	ui_component_log(str, 3000);
}


/*** WINDOW MSG / INPUT HANDLING ***/

static const NES_Button NES_KEYBOARD_MAP[SCANCODE_MAX] = {
	[SCANCODE_SEMICOLON] = NES_BUTTON_A,
	[SCANCODE_L]         = NES_BUTTON_B,
	[SCANCODE_LSHIFT]    = NES_BUTTON_SELECT,
	[SCANCODE_SPACE]     = NES_BUTTON_START,
	[SCANCODE_W]         = NES_BUTTON_UP,
	[SCANCODE_S]         = NES_BUTTON_DOWN,
	[SCANCODE_A]         = NES_BUTTON_LEFT,
	[SCANCODE_D]         = NES_BUTTON_RIGHT,
};

static void main_window_msg_func(struct window_msg *wmsg, const void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	im_input(wmsg);

	switch (wmsg->type) {
		case WINDOW_MSG_CLOSE:
			ctx->running = false;
			break;
		case WINDOW_MSG_KEYBOARD: {
			NES_Button button = NES_KEYBOARD_MAP[wmsg->keyboard.scancode];
			if (button != 0)
				NES_ControllerButton(ctx->nes, 0, button, wmsg->keyboard.pressed);
			break;
		}
		case WINDOW_MSG_GAMEPAD: {
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

#define CLOCK_UP   1000
#define CLOCK_DOWN -1000

static const uint8_t PATTERN_60[]  = {1};
static const uint8_t PATTERN_75[]  = {1, 1, 1, 2, 1, 1, 1, 2};
static const uint8_t PATTERN_85[]  = {1, 2, 1, 1, 2, 1, 2, 1, 2, 1, 2, 1};
static const uint8_t PATTERN_100[] = {1, 2, 2};
static const uint8_t PATTERN_120[] = {2};
static const uint8_t PATTERN_144[] = {2, 3, 2, 3, 2};

static uint32_t main_sync_to_60(struct main *ctx)
{
	uint32_t rr = window_refresh_rate(ctx->window);

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
	uint32_t queued = audio_queued_frames(ctx->audio);

	uint32_t audio_start = 100 * (ctx->cfg.nes.sampleRate / 1000);
	uint32_t audio_buffer = 50 * (ctx->cfg.nes.sampleRate / 1000);

	if (queued >= audio_start && !audio_playing(ctx->audio))
		audio_play(ctx->audio);

	if (queued == 0 && audio_playing(ctx->audio))
		audio_stop(ctx->audio);

	if (++ctx->frames % 120 == 0) {
		int64_t now = time_stamp();

		if (ctx->ts != 0) {
			int32_t cycles_sec = lrint(((double) ctx->cycles * 1000.0) / time_diff(ctx->ts, now));
			if (abs(cycles_sec - NES_CLOCK) < 5000) {
				ctx->cfg.nes.APUClock = cycles_sec + (queued >= audio_buffer ? CLOCK_UP : CLOCK_DOWN);
				NES_SetConfig(ctx->nes, &ctx->cfg.nes);
			}
		}

		ctx->cycles = 0;
		ctx->ts = now;
	}
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
	uint8_t *rom = fs_read(name, &rom_size);

	if (rom && rom_size > 16) {
		uint32_t offset = 16 + ((rom[6] & 0x04) ? 512 : 0); // iNES and optional trainer

		if (rom_size > offset) {
			ctx->crc32 = crypto_crc32(rom + offset, rom_size - offset);

			ui_component_clear_log();
			ui_component_message("Press ESC to access the menu", 3000);

			char sram_name[16];
			snprintf(sram_name, 16, "%02X.sav", ctx->crc32);

			NES_CartDesc desc = {0};
			bool found_in_db = ctx->cfg.use_db ? main_get_desc_from_db(offset, ctx->crc32, &desc) : false;

			if (found_in_db) {
				char msg[UI_LOG_LEN];
				snprintf(msg, UI_LOG_LEN, "%02X found in database", ctx->crc32);
				ui_component_log(msg, 3000);
			}

			size_t sram_size = 0;
			void *sram = fs_read(fs_path(fs_prog_dir(), fs_path("save", sram_name)), &sram_size);
			NES_LoadCart(ctx->nes, rom, rom_size, sram, sram_size, found_in_db ? &desc : NULL);
			free(sram);
			free(rom);

			window_set_title(ctx->window, APP_NAME, fs_file_name(name, false));

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

		const char *save_path = fs_path(fs_prog_dir(), "save");
		fs_mkdir(save_path);
		fs_write(fs_path(save_path, sram_name), sram, sram_size);
		free(sram);
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
				audio_destroy(&ctx->audio);
				audio_create(&ctx->audio, event->cfg.nes.sampleRate);
			}

			// Fullscreen/windowed transitions
			if (event->cfg.fullscreen != ctx->cfg.fullscreen) {
				if (window_is_fullscreen(ctx->window)) {
					window_set_windowed(ctx->window, ctx->cfg.window.w, ctx->cfg.window.h);

				} else {
					window_set_fullscreen(ctx->window);
				}

				event->cfg.fullscreen = window_is_fullscreen(ctx->window);
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
			window_set_title(ctx->window, APP_NAME, NULL);
			break;
		case UI_EVENT_RESET:
			window_set_windowed(ctx->window, ctx->cfg.window.w, ctx->cfg.window.h);
			ctx->cfg.fullscreen = window_is_fullscreen(ctx->window);
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

	ui_component_root(&args, main_ui_event, ctx);
}


/*** CONFIG ***/

static struct config main_load_config(void)
{
	size_t size = 0;
	struct config *cfg = (struct config *) fs_read(fs_path(fs_prog_dir(), "config.bin"), &size);

	struct config r = cfg && size == sizeof(struct config) && cfg->version == CONFIG_VERSION ?
		*cfg : (struct config) CONFIG_DEFAULTS;

	free(cfg);
	return r;
}

static void main_save_config(struct config *cfg)
{
	fs_write(fs_path(fs_prog_dir(), "config.bin"), cfg, sizeof(struct config));
}


/*** MAIN ***/

int32_t main(int32_t argc, char **argv)
{
	struct main ctx = {0};
	ctx.cfg = main_load_config();
	ctx.running = true;

	int32_t r = window_create(APP_NAME, main_window_msg_func, &ctx,
		ctx.cfg.window.w, ctx.cfg.window.h, ctx.cfg.fullscreen, &ctx.window);
	if (r != LIB_OK) goto except;

	r = audio_create(&ctx.audio, ctx.cfg.nes.sampleRate);
	if (r != LIB_OK) goto except;

	NES_Create(&ctx.cfg.nes, &ctx.nes);
	NES_SetLogCallback(main_nes_log);

	im_create(anonymous_compressed_data, anonymous_compressed_size, 13.0f);

	if (argc >= 2)
		ctx.loaded = main_load_rom(&ctx, argv[1]);

	while (ctx.running) {
		int64_t ts = time_stamp();
		window_poll(ctx.window);

		if (window_is_foreground(ctx.window) || !ctx.cfg.bg_pause) {
			main_audio_adjustment(&ctx);

			if (!ctx.paused) {
				ctx.cycles += NES_NextFrame(ctx.nes, main_nes_video, main_nes_audio, &ctx);

			} else {
				main_nes_video(NULL, &ctx);
			}

			OpaqueDevice *device = window_get_device(ctx.window);
			OpaqueContext *context = window_get_context(ctx.window);
			OpaqueTexture *back_buffer = window_get_back_buffer(ctx.window);

			im_begin(window_get_dpi_scale(ctx.window), device, context, back_buffer);
			im_draw(main_im_root, &ctx);
			im_render(!NES_CartLoaded(ctx.nes));

			window_release_back_buffer(back_buffer);
			double wait = floor(1000.0 / 60.0 - time_diff(ts, time_stamp())) - 1.0;
			window_present(ctx.window, main_sync_to_60(&ctx));

			if (ctx.cfg.reduce_latency && wait > 0.0)
				time_sleep(lrint(wait));

		} else {
			ctx.ts = 0;
			time_sleep(16);
		}
	}

	main_save_sram(&ctx);
	main_save_config(&ctx.cfg);

	except:

	ui_component_destroy();
	im_destroy();
	NES_Destroy(&ctx.nes);
	audio_destroy(&ctx.audio);
	window_destroy(&ctx.window);

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
