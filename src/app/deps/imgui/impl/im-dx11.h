#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <d3d11.h>

#include "im-draw-data.h"

#ifdef __cplusplus
extern "C" {
#endif

struct im_dx11;

bool im_dx11_create(ID3D11Device *device, const void *font, uint32_t width, uint32_t height, struct im_dx11 **dx11);
ID3D11ShaderResourceView *im_dx11_font_texture(struct im_dx11 *ctx);
void im_dx11_render(struct im_dx11 *ctx, const struct im_draw_data *dd, ID3D11Device *device, ID3D11DeviceContext *context);
void im_dx11_destroy(struct im_dx11 **dx11);

#ifdef __cplusplus
}
#endif
