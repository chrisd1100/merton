#include "imgui_impl_dx11.h"

#include <stdio.h>

#include "shaders/pixel.h"
#include "shaders/vertex.h"

// DirectX data

struct dx11 {
	ID3D11Buffer*			g_pVB;
	ID3D11Buffer*			g_pIB;
	ID3D11VertexShader*	 g_pVertexShader;
	ID3D11InputLayout*	  g_pInputLayout;
	ID3D11Buffer*			g_pVertexConstantBuffer;
	ID3D11PixelShader*	  g_pPixelShader;
	ID3D11SamplerState*	 g_pFontSampler;
	ID3D11ShaderResourceView*g_pFontTextureView;
	ID3D11RasterizerState*  g_pRasterizerState;
	ID3D11BlendState*		g_pBlendState;
	ID3D11DepthStencilState* g_pDepthStencilState;
	int g_VertexBufferSize;
	int g_IndexBufferSize;
};

struct BACKUP_DX11_STATE {
	UINT						ScissorRectsCount, ViewportsCount;
	D3D11_RECT				 ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	D3D11_VIEWPORT			 Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	ID3D11RasterizerState*	 RS;
	ID3D11BlendState*		  BlendState;
	FLOAT					  BlendFactor[4];
	UINT						SampleMask;
	UINT						StencilRef;
	ID3D11DepthStencilState*	DepthStencilState;
	ID3D11ShaderResourceView*  PSShaderResource;
	ID3D11SamplerState*		 PSSampler;
	ID3D11PixelShader*		 PS;
	ID3D11VertexShader*		 VS;
	ID3D11GeometryShader*	  GS;
	UINT						PSInstancesCount, VSInstancesCount, GSInstancesCount;
	ID3D11ClassInstance		 *PSInstances[256], *VSInstances[256], *GSInstances[256];  // 256 is max according to PSSetShader documentation
	D3D11_PRIMITIVE_TOPOLOGY	PrimitiveTopology;
	ID3D11Buffer*			  IndexBuffer, *VertexBuffer, *VSConstantBuffer;
	UINT						IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
	DXGI_FORMAT				 IndexBufferFormat;
	ID3D11InputLayout*		 InputLayout;
};

struct VERTEX_CONSTANT_BUFFER
{
	float mvp[4][4];
};

// Render function
void im_dx11_render(struct dx11 *ctx, ImDrawData* draw_data, ID3D11Device *device, ID3D11DeviceContext *context)
{
	// Avoid rendering when minimized
	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
		return;

	// Create and grow vertex/index buffers if needed
	if (!ctx->g_pVB || ctx->g_VertexBufferSize < draw_data->TotalVtxCount) {
		if (ctx->g_pVB) { ctx->g_pVB->Release(); ctx->g_pVB = NULL; }
		ctx->g_VertexBufferSize = draw_data->TotalVtxCount + 5000;
		D3D11_BUFFER_DESC desc = {};
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = ctx->g_VertexBufferSize * sizeof(ImDrawVert);
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		if (device->CreateBuffer(&desc, NULL, &ctx->g_pVB) < 0)
			return;
	}

	if (!ctx->g_pIB || ctx->g_IndexBufferSize < draw_data->TotalIdxCount) {
		if (ctx->g_pIB) { ctx->g_pIB->Release(); ctx->g_pIB = NULL; }
		ctx->g_IndexBufferSize = draw_data->TotalIdxCount + 10000;
		D3D11_BUFFER_DESC desc = {};
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = ctx->g_IndexBufferSize * sizeof(ImDrawIdx);
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (device->CreateBuffer(&desc, NULL, &ctx->g_pIB) < 0)
			return;
	}

	// Upload vertex/index data into a single contiguous GPU buffer
	D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
	if (context->Map(ctx->g_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)
		return;
	if (context->Map(ctx->g_pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK)
		return;
	ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource.pData;
	ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource.pData;
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	context->Unmap(ctx->g_pVB, 0);
	context->Unmap(ctx->g_pIB, 0);

	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
	D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
	if (context->Map(ctx->g_pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) != S_OK)
		return;
	VERTEX_CONSTANT_BUFFER* constant_buffer = (VERTEX_CONSTANT_BUFFER*)mapped_resource.pData;
	float L = draw_data->DisplayPos.x;
	float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	float T = draw_data->DisplayPos.y;
	float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	float mvp[4][4] = {
		{ 2.0f/(R-L),   0.0f,		   0.0f,	   0.0f },
		{ 0.0f,		 2.0f/(T-B),	 0.0f,	   0.0f },
		{ 0.0f,		 0.0f,		   0.5f,	   0.0f },
		{ (R+L)/(L-R),  (T+B)/(B-T),	0.5f,	   1.0f },
	};
	memcpy(&constant_buffer->mvp, mvp, sizeof(mvp));
	context->Unmap(ctx->g_pVertexConstantBuffer, 0);

	// Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and verbose. Close your eyes!)
	BACKUP_DX11_STATE old;
	old.ScissorRectsCount = old.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	context->RSGetScissorRects(&old.ScissorRectsCount, old.ScissorRects);
	context->RSGetViewports(&old.ViewportsCount, old.Viewports);
	context->RSGetState(&old.RS);
	context->OMGetBlendState(&old.BlendState, old.BlendFactor, &old.SampleMask);
	context->OMGetDepthStencilState(&old.DepthStencilState, &old.StencilRef);
	context->PSGetShaderResources(0, 1, &old.PSShaderResource);
	context->PSGetSamplers(0, 1, &old.PSSampler);
	old.PSInstancesCount = old.VSInstancesCount = old.GSInstancesCount = 256;
	context->PSGetShader(&old.PS, old.PSInstances, &old.PSInstancesCount);
	context->VSGetShader(&old.VS, old.VSInstances, &old.VSInstancesCount);
	context->VSGetConstantBuffers(0, 1, &old.VSConstantBuffer);
	context->GSGetShader(&old.GS, old.GSInstances, &old.GSInstancesCount);

	context->IAGetPrimitiveTopology(&old.PrimitiveTopology);
	context->IAGetIndexBuffer(&old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset);
	context->IAGetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);
	context->IAGetInputLayout(&old.InputLayout);

	// Setup desired DX state
	// Setup viewport
	D3D11_VIEWPORT vp = {};
	vp.Width = draw_data->DisplaySize.x;
	vp.Height = draw_data->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0;
	context->RSSetViewports(1, &vp);

	// Setup shader and vertex buffers
	unsigned int stride = sizeof(ImDrawVert);
	unsigned int offset = 0;
	context->IASetInputLayout(ctx->g_pInputLayout);
	context->IASetVertexBuffers(0, 1, &ctx->g_pVB, &stride, &offset);
	context->IASetIndexBuffer(ctx->g_pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(ctx->g_pVertexShader, NULL, 0);
	context->VSSetConstantBuffers(0, 1, &ctx->g_pVertexConstantBuffer);
	context->PSSetShader(ctx->g_pPixelShader, NULL, 0);
	context->PSSetSamplers(0, 1, &ctx->g_pFontSampler);
	context->GSSetShader(NULL, NULL, 0);
	context->HSSetShader(NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..
	context->DSSetShader(NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..
	context->CSSetShader(NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..

	// Setup blend state
	const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
	context->OMSetBlendState(ctx->g_pBlendState, blend_factor, 0xffffffff);
	context->OMSetDepthStencilState(ctx->g_pDepthStencilState, 0);
	context->RSSetState(ctx->g_pRasterizerState);

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_idx_offset = 0;
	int global_vtx_offset = 0;
	ImVec2 clip_off = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			// Apply scissor/clipping rectangle
			const D3D11_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
			context->RSSetScissorRects(1, &r);

			// Bind texture, Draw
			ID3D11ShaderResourceView* texture_srv = (ID3D11ShaderResourceView*)pcmd->TextureId;
			context->PSSetShaderResources(0, 1, &texture_srv);
			context->DrawIndexed(pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}

	// Restore modified DX state
	context->RSSetScissorRects(old.ScissorRectsCount, old.ScissorRects);
	context->RSSetViewports(old.ViewportsCount, old.Viewports);
	context->RSSetState(old.RS); if (old.RS) old.RS->Release();
	context->OMSetBlendState(old.BlendState, old.BlendFactor, old.SampleMask); if (old.BlendState) old.BlendState->Release();
	context->OMSetDepthStencilState(old.DepthStencilState, old.StencilRef); if (old.DepthStencilState) old.DepthStencilState->Release();
	context->PSSetShaderResources(0, 1, &old.PSShaderResource); if (old.PSShaderResource) old.PSShaderResource->Release();
	context->PSSetSamplers(0, 1, &old.PSSampler); if (old.PSSampler) old.PSSampler->Release();
	context->PSSetShader(old.PS, old.PSInstances, old.PSInstancesCount); if (old.PS) old.PS->Release();
	for (UINT i = 0; i < old.PSInstancesCount; i++) if (old.PSInstances[i]) old.PSInstances[i]->Release();
	context->VSSetShader(old.VS, old.VSInstances, old.VSInstancesCount); if (old.VS) old.VS->Release();
	context->VSSetConstantBuffers(0, 1, &old.VSConstantBuffer); if (old.VSConstantBuffer) old.VSConstantBuffer->Release();
	context->GSSetShader(old.GS, old.GSInstances, old.GSInstancesCount); if (old.GS) old.GS->Release();
	for (UINT i = 0; i < old.VSInstancesCount; i++) if (old.VSInstances[i]) old.VSInstances[i]->Release();
	context->IASetPrimitiveTopology(old.PrimitiveTopology);
	context->IASetIndexBuffer(old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset); if (old.IndexBuffer) old.IndexBuffer->Release();
	context->IASetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset); if (old.VertexBuffer) old.VertexBuffer->Release();
	context->IASetInputLayout(old.InputLayout); if (old.InputLayout) old.InputLayout->Release();
}

bool im_dx11_create(ID3D11Device *device, const void *pixels, int32_t width, int32_t height, struct dx11 **dx11)
{
	struct dx11 *ctx = *dx11 = (struct dx11 *) calloc(1, sizeof(struct dx11));

	D3D11_BUFFER_DESC desc = {};
	D3D11_BLEND_DESC bdesc = {};
	D3D11_RASTERIZER_DESC rdesc = {};
	D3D11_DEPTH_STENCIL_DESC sdesc = {};
	D3D11_TEXTURE2D_DESC tdesc = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	D3D11_SAMPLER_DESC samdesc = {};

	HRESULT e = device->CreateVertexShader(VERTEX_SHADER, sizeof(VERTEX_SHADER), NULL, &ctx->g_pVertexShader);
	if (e != S_OK)
		goto except;

	e = device->CreatePixelShader(PIXEL_SHADER, sizeof(PIXEL_SHADER), NULL, &ctx->g_pPixelShader);
	if (e != S_OK)
		goto except;

	D3D11_INPUT_ELEMENT_DESC local_layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t)(&((ImDrawVert*)0)->pos),  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t)(&((ImDrawVert*)0)->uv),   D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",	 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (size_t)(&((ImDrawVert*)0)->col),  D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	device->CreateInputLayout(local_layout, 3, VERTEX_SHADER, sizeof(VERTEX_SHADER), &ctx->g_pInputLayout);
	if (e != S_OK)
		goto except;

	// Create the constant buffer
	desc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER);
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	e = device->CreateBuffer(&desc, NULL, &ctx->g_pVertexConstantBuffer);
	if (e != S_OK)
		goto except;

	// Create the blending setup
	bdesc.AlphaToCoverageEnable = false;
	bdesc.RenderTarget[0].BlendEnable = true;
	bdesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bdesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bdesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bdesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bdesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	e = device->CreateBlendState(&bdesc, &ctx->g_pBlendState);
	if (e != S_OK)
		goto except;

	// Create the rasterizer state
	rdesc.FillMode = D3D11_FILL_SOLID;
	rdesc.CullMode = D3D11_CULL_NONE;
	rdesc.ScissorEnable = true;
	rdesc.DepthClipEnable = true;
	e = device->CreateRasterizerState(&rdesc, &ctx->g_pRasterizerState);
	if (e != S_OK)
		goto except;

	// Create depth-stencil State
	sdesc.DepthEnable = false;
	sdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	sdesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	sdesc.StencilEnable = false;
	sdesc.FrontFace.StencilFailOp = sdesc.FrontFace.StencilDepthFailOp = sdesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	sdesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	sdesc.BackFace = sdesc.FrontFace;
	e = device->CreateDepthStencilState(&sdesc, &ctx->g_pDepthStencilState);
	if (e != S_OK)
		goto except;

	// Upload texture to graphics system
	tdesc.Width = width;
	tdesc.Height = height;
	tdesc.MipLevels = 1;
	tdesc.ArraySize = 1;
	tdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tdesc.SampleDesc.Count = 1;
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tdesc.CPUAccessFlags = 0;

	ID3D11Texture2D *pTexture = NULL;
	D3D11_SUBRESOURCE_DATA subResource;
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = tdesc.Width * 4;
	subResource.SysMemSlicePitch = 0;
	e = device->CreateTexture2D(&tdesc, &subResource, &pTexture);
	if (e != S_OK)
		goto except;

	// Create texture view
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = tdesc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	e = device->CreateShaderResourceView(pTexture, &srvDesc, &ctx->g_pFontTextureView);
	pTexture->Release();
	if (e != S_OK)
		goto except;

	// Create texture sampler
	samdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samdesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samdesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samdesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samdesc.MipLODBias = 0.f;
	samdesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samdesc.MinLOD = 0.f;
	samdesc.MaxLOD = 0.f;
	e = device->CreateSamplerState(&samdesc, &ctx->g_pFontSampler);
	if (e != S_OK)
		goto except;

	except:

	if (e != S_OK)
		im_dx11_destroy(dx11);

	return e == S_OK;
}

void *im_dx11_font_texture(struct dx11 *ctx)
{
	return ctx->g_pFontTextureView;
}

void im_dx11_destroy(struct dx11 **dx11)
{
	if (!dx11 || !*dx11)
		return;

	struct dx11 *ctx = *dx11;

	if (ctx->g_pFontSampler)
		ctx->g_pFontSampler->Release();

	if (ctx->g_pFontTextureView)
		ctx->g_pFontTextureView->Release();

	if (ctx->g_pIB)
		ctx->g_pIB->Release();

	if (ctx->g_pVB)
		ctx->g_pVB->Release();

	if (ctx->g_pBlendState)
		ctx->g_pBlendState->Release();

	if (ctx->g_pDepthStencilState)
		ctx->g_pDepthStencilState->Release();

	if (ctx->g_pRasterizerState)
		ctx->g_pRasterizerState->Release();

	if (ctx->g_pPixelShader)
		ctx->g_pPixelShader->Release();

	if (ctx->g_pVertexConstantBuffer)
		ctx->g_pVertexConstantBuffer->Release();

	if (ctx->g_pInputLayout)
		ctx->g_pInputLayout->Release();

	if (ctx->g_pVertexShader)
		ctx->g_pVertexShader->Release();

	free(ctx);
	*dx11 = NULL;
}
