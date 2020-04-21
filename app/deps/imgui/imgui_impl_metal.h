// dear imgui: Renderer for Metal
// This needs to be used along with a Platform Binding (e.g. OSX)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'MTLTexture' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Support for large meshes (64k+ vertices) with 16-bit indices.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you are new to dear imgui, read examples/README.txt and read the documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include "imgui.h"      // IMGUI_IMPL_API

IMGUI_IMPL_API bool ImGui_ImplMetal_Init(void *device);
IMGUI_IMPL_API void ImGui_ImplMetal_Shutdown();
IMGUI_IMPL_API void ImGui_ImplMetal_NewFrame(void *renderPassDescriptor);
IMGUI_IMPL_API void ImGui_ImplMetal_RenderDrawData(ImDrawData* draw_data,
                                                   void *commandBuffer,
                                                   void *commandEncoder);

// Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API bool ImGui_ImplMetal_CreateFontsTexture(void *device);
IMGUI_IMPL_API void ImGui_ImplMetal_DestroyFontsTexture();
IMGUI_IMPL_API bool ImGui_ImplMetal_CreateDeviceObjects(void * device);
IMGUI_IMPL_API void ImGui_ImplMetal_DestroyDeviceObjects();
