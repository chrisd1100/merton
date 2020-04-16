#pragma once

#include <Metal/Metal.h>

struct window_quad;

bool window_quad_init(id<MTLDevice> device, struct window_quad **quad);
void window_quad_render(struct window_quad *ctx, id<MTLCommandQueue> cq,
	const void *image, uint32_t width, uint32_t height, id<MTLTexture> dest, float aspect_ratio);
void window_quad_destroy(struct window_quad **quad);
