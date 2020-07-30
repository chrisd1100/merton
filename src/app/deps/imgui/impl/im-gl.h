#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "im-draw-data.h"

#ifdef __cplusplus
extern "C" {
#endif

struct im_gl;

typedef uint32_t GL_Uint;

bool im_gl_create(const char *version, const void *font, uint32_t width, uint32_t height, struct im_gl **gl);
GL_Uint im_gl_font_texture(struct im_gl *ctx);
void im_gl_render(struct im_gl *ctx, const struct im_draw_data *dd, bool clear, GL_Uint texture);
void im_gl_destroy(struct im_gl **gl);

void im_gl_texture_size(struct im_gl *ctx, GL_Uint texture, float *width, float *height);

#ifdef __cplusplus
}
#endif
