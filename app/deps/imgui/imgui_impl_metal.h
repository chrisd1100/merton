// dear imgui: Renderer for Metal
// This needs to be used along with a Platform Binding (e.g. OSX)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'MTLTexture' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Support for large meshes (64k+ vertices) with 16-bit indices.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you are new to dear imgui, read examples/README.txt and read the documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include "imgui.h"      // IMGUI_IMPL_API

#ifdef __cplusplus
extern "C" {
#endif

IMGUI_IMPL_API bool ImGui_ImplMetal_Init(void *device);
IMGUI_IMPL_API void ImGui_ImplMetal_Shutdown();
IMGUI_IMPL_API void ImGui_ImplMetal_RenderDrawData(ImDrawData* draw_data, void *ocq, void *otexture);
IMGUI_IMPL_API void ImGui_ImplMetal_TextureSize(void *texture, float *width, float *height);
IMGUI_IMPL_API void *ImGui_ImplMetal_GetDrawableTexture(void *drawable);

#ifdef __cplusplus
}
#endif
