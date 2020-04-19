#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum lib_status {
	LIB_OK   = 0,
	LIB_ERR = -1,
};


/*** WINDOW ***/
#define TITLE_MAX 1024

enum window_msg_type {
	WINDOW_MSG_NONE         = 0,
	WINDOW_MSG_CLOSE        = 1,
	WINDOW_MSG_KEYBOARD     = 2,
	WINDOW_MSG_GAMEPAD      = 3,
	WINDOW_MSG_MOUSE_WHEEL  = 4,
	WINDOW_MSG_MOUSE_BUTTON = 6,
	WINDOW_MSG_MOUSE_MOTION = 7,
};

enum scancode {
	SCANCODE_NONE      = 0x000,
	SCANCODE_ESCAPE    = 0x001,
	SCANCODE_TAB       = 0x00f,
	SCANCODE_W         = 0x011,
	SCANCODE_C         = 0x02e,
	SCANCODE_X         = 0x02d,
	SCANCODE_Y         = 0x015,
	SCANCODE_Z         = 0x02c,
	SCANCODE_V         = 0x02f,
	SCANCODE_R         = 0x013,
	SCANCODE_A         = 0x01E,
	SCANCODE_S         = 0x01F,
	SCANCODE_D         = 0x020,
	SCANCODE_L         = 0x026,
	SCANCODE_SEMICOLON = 0x027,
	SCANCODE_LSHIFT    = 0x02A,
	SCANCODE_SPACE     = 0x039,
	SCANCODE_LEFT      = 0x14b,
	SCANCODE_RIGHT     = 0x14d,
	SCANCODE_UP        = 0x148,
	SCANCODE_DOWN      = 0x150,
	SCANCODE_PAGEUP    = 0x149,
	SCANCODE_PAGEDOWN  = 0x151,
	SCANCODE_ENTER     = 0x01c,
	SCANCODE_HOME      = 0x147,
	SCANCODE_END       = 0x14f,
	SCANCODE_INSERT    = 0x152,
	SCANCODE_DELETE    = 0x153,
	SCANCODE_BACKSPACE = 0x00e,
	SCANCODE_RSHIFT    = 0x036,
	SCANCODE_LCTRL     = 0x01d,
	SCANCODE_RCTRL     = 0x11d,
	SCANCODE_LALT      = 0x038,
	SCANCODE_RALT      = 0x138,
	SCANCODE_LGUI      = 0x15b,
	SCANCODE_RGUI      = 0x15c,

	SCANCODE_MAX       = 0x200,
};

enum mouse_button {
	MOUSE_NONE   = 0,
	MOUSE_L      = 1,
	MOUSE_R      = 2,
	MOUSE_MIDDLE = 3,
};

enum filter {
	FILTER_NEAREST = 1,
	FILTER_LINEAR  = 2,
};

struct window_msg {
	enum window_msg_type type;

	union {
		struct {
			bool pressed;
			enum scancode scancode;
		} keyboard;

		struct {
			int8_t leftThumbX;
			int8_t leftThumbY;
			int8_t rightThumbX;
			int8_t rightThumbY;
			int8_t leftTrigger;
			int8_t rightTrigger;
			bool a;
			bool b;
			bool x;
			bool y;
			bool back;
			bool start;
			bool guide;
			bool leftShoulder;
			bool rightShoulder;
			bool leftThumb;
			bool rightThumb;
			bool up;
			bool right;
			bool down;
			bool left;
		} gamepad;

		struct {
			int32_t x;
			int32_t y;
		} mouseWheel;

		struct {
			enum mouse_button button;
			bool pressed;
		} mouseButton;

		struct {
			bool relative;
			int32_t x;
			int32_t y;
		} mouseMotion;
	};
};

struct window;

typedef void (*WINDOW_MSG_FUNC)(struct window_msg *wmsg, const void *opaque);

typedef void OpaqueDevice;
typedef void OpaqueContext;
typedef void OpaqueTexture;

enum lib_status window_create(const char *title, WINDOW_MSG_FUNC msg_func, const void *opaque,
	uint32_t width, uint32_t height, bool fullscreen, struct window **window);
void window_poll(struct window *ctx);
bool window_is_foreground(struct window *ctx);
uint32_t window_refresh_rate(struct window *ctx);
float window_get_dpi_scale(struct window *ctx);
void window_set_fullscreen(struct window *ctx);
void window_set_windowed(struct window *ctx, uint32_t width, uint32_t height);
bool window_is_fullscreen(struct window *ctx);
void window_present(struct window *ctx, uint32_t num_frames);
OpaqueDevice *window_get_device(struct window *ctx);
OpaqueContext *window_get_context(struct window *ctx);
OpaqueTexture *window_get_back_buffer(struct window *ctx);
void window_release_back_buffer(OpaqueTexture *texture);
void window_render_quad(struct window *ctx, const void *image, uint32_t width,
	uint32_t height, uint32_t constrain_w, uint32_t constrain_h, float aspect_ratio,
	enum filter filter);
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
struct finfo {
	bool dir;
	const char *path;
	const char *name;
};

void *fs_read(const char *path, size_t *size);
void fs_write(const char *path, const void *data, size_t size);
void fs_mkdir(const char *path);
const char *fs_path(const char *dir, const char *file);
uint32_t fs_list(const char *path, struct finfo **fi);
void fs_free_list(struct finfo **fi, uint32_t len);


/*** CRYPTO ***/
uint32_t crypto_crc32(void *data, size_t size);

#ifdef __cplusplus
}
#endif
