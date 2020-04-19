#pragma once

#include <stdint.h>

#include "lib.h"

#define COBJMACROS
#include <d3d11.h>
#include <dxgi1_3.h>

struct window_quad;

HRESULT window_quad_init(ID3D11Device *device, struct window_quad **quad);
HRESULT window_quad_render(struct window_quad *ctx, ID3D11Device *device, ID3D11DeviceContext *context,
	const void *image, uint32_t width, uint32_t height, ID3D11Texture2D *dest, float aspect_ratio,
	enum filter filter);
void window_quad_destroy(struct window_quad **quad);
