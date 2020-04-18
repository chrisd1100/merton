#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "lib.h"
#include "ui.h"

#include "../src/nes.h"

#define WINDOW_W     (NES_FRAME_WIDTH * 3)
#define WINDOW_H     (NES_FRAME_HEIGHT * 3)

#define SAMPLE_RATE  44100
#define AUDIO_START  (100 * (SAMPLE_RATE / 1000)) // 100ms
#define AUDIO_BUFFER (50 * (SAMPLE_RATE / 1000))  // 50ms
#define CLOCK_UP     1000
#define CLOCK_DOWN   -1000

struct main {
	NES *nes;
	struct window *window;
	struct audio *audio;
	bool running;

	// Video
	uint32_t cropped[NES_FRAME_WIDTH * NES_FRAME_HEIGHT];
	uint8_t sync;

	// Audio
	uint32_t cycles;
	uint32_t frames;
	int64_t ts;
};

const uint8_t PATTERN_60[]  = {1};
const uint8_t PATTERN_75[]  = {1, 1, 1, 2, 1, 1, 1, 2};
const uint8_t PATTERN_85[]  = {1, 2, 1, 1, 2, 1, 2, 1, 2, 1, 2, 1};
const uint8_t PATTERN_100[] = {1, 2, 2};
const uint8_t PATTERN_120[] = {2};
const uint8_t PATTERN_144[] = {2, 3, 2, 3, 2};

static NES_Button NES_KEYBOARD_MAP[SCANCODE_MAX] = {
	[SCANCODE_SEMICOLON] = NES_BUTTON_A,
	[SCANCODE_L]         = NES_BUTTON_B,
	[SCANCODE_LSHIFT]    = NES_BUTTON_SELECT,
	[SCANCODE_SPACE]     = NES_BUTTON_START,
	[SCANCODE_W]         = NES_BUTTON_UP,
	[SCANCODE_S]         = NES_BUTTON_DOWN,
	[SCANCODE_A]         = NES_BUTTON_LEFT,
	[SCANCODE_D]         = NES_BUTTON_RIGHT,
};

static void main_crop_copy(uint32_t *dest, const uint32_t *src, uint32_t top, uint32_t right,
	uint32_t bottom, uint32_t left)
{
	int32_t adjx = (right - left) / 2;
	int32_t adjy = (bottom - top) / 2;

	for (uint32_t row = top; row < NES_FRAME_HEIGHT - bottom; row++)
		memcpy(dest + (row + adjy) * NES_FRAME_WIDTH + left + adjx,
			src + row * NES_FRAME_WIDTH + left, 4 * (NES_FRAME_WIDTH - left - right));
}

static void main_nes_video(const uint32_t *frame, void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	main_crop_copy(ctx->cropped, frame, 8, 0, 8, 0);
	window_render_quad(ctx->window, ctx->cropped, NES_FRAME_WIDTH, NES_FRAME_HEIGHT, 16.0f / 15.0f);
}

static void main_nes_audio(const int16_t *frames, uint32_t count, void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	audio_queue(ctx->audio, frames, count);
}

static void main_nes_log(const char *str)
{
	printf("[merton] %s\n", str);
}

static void main_window_msg_func(struct window_msg *wmsg, const void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	ui_input(wmsg);

	switch (wmsg->type) {
		case WINDOW_MSG_CLOSE:
			ctx->running = false;
			break;
		case WINDOW_MSG_KEYBOARD: {
			switch (wmsg->keyboard.scancode) {
				case SCANCODE_ESCAPE:
					NES_Reset(ctx->nes, false);
					break;
				default: {
					NES_Button button = NES_KEYBOARD_MAP[wmsg->keyboard.scancode];
					if (button != 0)
						NES_ControllerButton(ctx->nes, 0, button, wmsg->keyboard.pressed);
					break;
				}
			}
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

	if (queued >= AUDIO_START && !audio_playing(ctx->audio))
		audio_play(ctx->audio);

	if (queued == 0 && audio_playing(ctx->audio))
		audio_stop(ctx->audio);

	if (++ctx->frames % 120 == 0) {
		int64_t now = time_stamp();

		if (ctx->ts != 0) {
			uint32_t cycles_sec = lrint(((double) ctx->cycles * 1000.0) / time_diff(ctx->ts, now));
			NES_SetAPUClock(ctx->nes, cycles_sec + (queued >= AUDIO_BUFFER ? CLOCK_UP : CLOCK_DOWN));
		}

		ctx->cycles = 0;
		ctx->ts = now;
	}
}

static void main_save_sram(NES *nes, const char *sram_file)
{
	size_t sram_size = NES_SRAMDirty(nes);

	if (sram_size > 0) {
		void *sram = calloc(sram_size, 1);
		NES_GetSRAM(nes, sram, sram_size);

		fs_mkdir("save");
		fs_write(fs_path("save", sram_file), sram, sram_size);
		free(sram);
	}
}

static void main_ui_root(void *opaque)
{
	struct main *ctx = (struct main *) opaque;

	struct ui_args args = {0};
	args.nes = ctx->nes;

	ui_root(&args);
}

int32_t main(int32_t argc, char **argv)
{
	struct main ctx = {0};
	ctx.running = true;

	int32_t r = window_create("Merton", main_window_msg_func, &ctx, WINDOW_W, WINDOW_H, &ctx.window);
	if (r != LIB_OK) goto except;

	r = audio_create(&ctx.audio, SAMPLE_RATE);
	if (r != LIB_OK) goto except;

	NES_Create(main_nes_video, main_nes_audio, &ctx, SAMPLE_RATE, true, &ctx.nes);
	NES_SetLogCallback(main_nes_log);

	ui_create();

	char sram_file[16] = {0};

	if (argc >= 2) {
		size_t rom_size = 0;
		void *rom = fs_read(argv[1], &rom_size);

		if (rom) {
			uint32_t crc32 = crypto_crc32(rom, rom_size);
			snprintf(sram_file, 16, "%02X.sav", crc32);

			size_t sram_size = 0;
			void *sram = fs_read(fs_path("save", sram_file), &sram_size);
			NES_LoadCart(ctx.nes, rom, rom_size, sram, sram_size, NULL);
			free(sram);
			free(rom);
		}
	}

	while (ctx.running) {
		window_poll(ctx.window);

		if (window_is_foreground(ctx.window)) {
			main_audio_adjustment(&ctx);
			ctx.cycles += NES_NextFrame(ctx.nes);

			OpaqueDevice *device = window_get_device(ctx.window);
			OpaqueContext *context = window_get_context(ctx.window);
			OpaqueTexture *back_buffer = window_get_back_buffer(ctx.window);

			ui_begin(window_get_dpi_scale(ctx.window), device, context, back_buffer);
			ui_draw(main_ui_root, &ctx);
			ui_render(ctx.cycles == 0);

			window_release_back_buffer(back_buffer);
			window_present(ctx.window, main_sync_to_60(&ctx));

		} else {
			ctx.ts = 0;
			time_sleep(16);
		}
	}

	if (sram_file[0])
		main_save_sram(ctx.nes, sram_file);

	except:

	ui_destroy();
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

	AllocConsole();
	AttachConsole(GetCurrentProcessId());

	FILE *f = NULL;
	freopen_s(&f, "CONOUT$", "w", stdout);

	timeBeginPeriod(1);

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	int32_t r = main(__argc, __argv);

	timeEndPeriod(1);

	return r;
}
#endif
