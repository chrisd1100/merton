#include "imgui.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ImGui_ImplMetal_Init(void *odevice, const void *font, int32_t font_w, int32_t font_h, void **font_tex);
void ImGui_ImplMetal_Shutdown();
void ImGui_ImplMetal_RenderDrawData(ImDrawData* draw_data, void *ocq, void *otexture);

void ImGui_ImplMetal_TextureSize(void *texture, float *width, float *height);
void *ImGui_ImplMetal_GetDrawableTexture(void *drawable);

#ifdef __cplusplus
}
#endif
