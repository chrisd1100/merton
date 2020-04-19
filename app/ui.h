#pragma once

#include "lib.h"
#include "config.h"
#include "../src/nes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ui_event_type {
	UI_EVENT_NONE   = 0,
	UI_EVENT_CONFIG = 1,
	UI_EVENT_QUIT   = 2,
	UI_EVENT_PAUSE  = 3,
};

struct ui_args {
	const struct config *cfg;
	bool paused;
	NES *nes;
};

struct ui_event {
	enum ui_event_type type;
	union {
		struct config cfg;
	};
};

/*** FRAMEWORK ***/
void ui_create(void);
void ui_destroy(void);
void ui_input(struct window_msg *wmsg);
bool ui_begin(float dpi_scale, OpaqueDevice *device, OpaqueContext *context, OpaqueTexture *texture);
void ui_draw(void (*callback)(void *opaque), const void *opaque);
void ui_render(bool clear);

/*** COMPONENTS ***/
void ui_root(const struct ui_args *args, void (*event_callback)(struct ui_event *event, void *opaque), const void *opaque);

#ifdef __cplusplus
}
#endif
