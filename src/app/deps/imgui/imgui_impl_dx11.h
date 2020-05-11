#pragma once

#include <d3d11.h>

#include "imgui.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dx11;

bool im_dx11_create(ID3D11Device *device, const void *pixels, int32_t width, int32_t height, struct dx11 **dx11);
void *im_dx11_font_texture(struct dx11 *ctx);
void im_dx11_render(struct dx11 *ctx, ImDrawData *draw_data, ID3D11Device *device, ID3D11DeviceContext *context);
void im_dx11_destroy(struct dx11 **dx11);

#ifdef __cplusplus
}
#endif
