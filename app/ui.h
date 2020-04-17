#pragma once

#include "lib.h"
#include "../src/nes.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ui_args {
	NES *nes;
};

/*** FRAMEWORK ***/
void ui_create(void);
void ui_destroy(void);
void ui_input(struct window_msg *wmsg);
bool ui_begin(OpaqueDevice *device, OpaqueContext *context, OpaqueTexture *texture);
void ui_draw(void (*callback)(void *opaque), const void *opaque);
void ui_render(bool clear);

/*** COMPONENTS ***/
void ui_root(struct ui_args *args);

#ifdef __cplusplus
}
#endif
