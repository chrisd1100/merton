#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum lib_status {
	LIB_OK   = 0,
	LIB_ERR = -1,
};


/*** WINDOW ***/
#define TITLE_MAX 1024

enum window_msg_type {
	WINDOW_MSG_NONE     = 0,
	WINDOW_MSG_CLOSE    = 1,
	WINDOW_MSG_KEYBOARD = 2,
};

enum scancode {
	SCANCODE_NONE      = 0x0000,
	SCANCODE_W         = 0x0011,
	SCANCODE_R         = 0x0013,
	SCANCODE_A         = 0x001E,
	SCANCODE_S         = 0x001F,
	SCANCODE_D         = 0x0020,
	SCANCODE_L         = 0x0026,
	SCANCODE_SEMICOLON = 0x0027,
	SCANCODE_LSHIFT    = 0x002A,
	SCANCODE_SPACE     = 0x0039,

	SCANCODE_MAX       = 0x0200,
};

struct window_msg {
	enum window_msg_type type;

	union {
		struct {
			bool pressed;
			enum scancode scancode;
		} keyboard;
	};
};

struct window;

typedef void (*WINDOW_MSG_FUNC)(struct window_msg *wmsg, const void *opaque);

enum lib_status window_create(const char *title, WINDOW_MSG_FUNC msg_func, const void *opaque,
	uint32_t width, uint32_t height, struct window **window);
void window_poll(struct window *ctx);
bool window_is_foreground(struct window *ctx);
uint32_t window_refresh_rate(struct window *ctx);
void window_present(struct window *ctx, uint32_t num_frames);
void window_render_quad(struct window *ctx, const void *image, uint32_t width,
	uint32_t height, float aspect_ratio);
void window_destroy(struct window **window);


/*** AUDIO ***/

struct audio;

enum lib_status audio_create(struct audio **audio, uint32_t sample_rate);
uint32_t audio_queued_frames(struct audio *ctx);
bool audio_playing(struct audio *ctx);
void audio_play(struct audio *ctx);
void audio_stop(struct audio *ctx);
void audio_queue(struct audio *ctx, const int16_t *frames, uint32_t count);
void audio_destroy(struct audio **audio);


/*** TIME ***/
void time_sleep(uint32_t ms);
int64_t time_stamp(void);
double time_diff(int64_t begin, int64_t end);


/*** FS ***/
void *fs_read(const char *path, size_t *size);
void fs_write(const char *path, const void *data, size_t size);
void fs_mkdir(const char *path);
const char *fs_path(const char *dir, const char *file);


/*** CRYPTO ***/
uint32_t crypto_crc32(void *data, size_t size);
