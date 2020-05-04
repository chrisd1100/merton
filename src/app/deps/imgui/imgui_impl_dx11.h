#pragma once

#include "imgui.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

static bool ImGui_ImplDX11_Init(ID3D11Device* device);
static void ImGui_ImplDX11_Shutdown();
static void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data, ID3D11Device *device, ID3D11DeviceContext *ctx);
