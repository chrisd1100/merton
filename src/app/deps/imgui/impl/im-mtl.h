#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "im-draw-data.h"

#ifdef __cplusplus
extern "C" {
#endif

struct im_mtl;

typedef struct MTL_Device MTL_Device;
typedef struct MTL_CommandQueue MTL_CommandQueue;
typedef struct MTL_Texture MTL_Texture;

bool im_mtl_create(MTL_Device *device, const void *font, uint32_t width, uint32_t height, struct im_mtl **mtl);
void im_mtl_render(struct im_mtl *ctx, const struct im_draw_data *dd, MTL_CommandQueue *cq, MTL_Texture *texture);
MTL_Texture *im_mtl_font_texture(struct im_mtl *ctx);
void im_mtl_destroy(struct im_mtl **mtl);

void im_mtl_texture_size(MTL_Texture *texture, float *width, float *height);

#ifdef __cplusplus
}
#endif
