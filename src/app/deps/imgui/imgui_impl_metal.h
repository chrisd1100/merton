#include "imgui.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mtl;

bool im_mtl_create(void *odevice, const void *font, int32_t font_w, int32_t font_h, struct mtl **mtl);
void im_mtl_render(struct mtl *ctx, ImDrawData *draw_data, void *ocq, void *otexture);
void *im_mtl_font_texture(struct mtl *ctx);
void im_mtl_destroy(struct mtl **mtl);

void im_mtl_texture_size(void *texture, float *width, float *height);
void *im_mtl_get_drawable_texture(void *drawable);

#ifdef __cplusplus
}
#endif
