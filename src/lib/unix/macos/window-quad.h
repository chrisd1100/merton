#pragma once

#include <Metal/Metal.h>

#include "lib/lib.h"

struct window_quad;

bool window_quad_init(id<MTLDevice> device, struct window_quad **quad);
void window_quad_render(struct window_quad *ctx, id<MTLCommandQueue> cq,
	const void *image, uint32_t width, uint32_t height, uint32_t constrain_w,
	uint32_t constrain_h, id<MTLTexture> dest, float aspect_ratio, enum filter filter,
	enum effect effect);
void window_quad_destroy(struct window_quad **quad);
