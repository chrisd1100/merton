#pragma warning(push)
#pragma warning(disable: 4201)

#define COBJMACROS
#include "im-dx11.h"

#include <stdio.h>
#include <math.h>

#include "shaders/pixel.h"
#include "shaders/vertex.h"

struct im_dx11_buffer {
	ID3D11Buffer *b;
	ID3D11Resource *res;
	uint32_t len;
};

struct im_dx11 {
	struct im_dx11_buffer vb;
	struct im_dx11_buffer ib;
	ID3D11VertexShader *vs;
	ID3D11InputLayout *il;
	ID3D11Buffer *cb;
	ID3D11Resource *cb_res;
	ID3D11PixelShader *ps;
	ID3D11SamplerState *sampler;
	ID3D11ShaderResourceView *font;
	ID3D11RasterizerState *rs;
	ID3D11BlendState *bs;
	ID3D11DepthStencilState *dss;
};

struct im_dx11_state {
	UINT sr_count;
	UINT vp_count;
	D3D11_RECT sr[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	D3D11_VIEWPORT vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	ID3D11RasterizerState *rs;
	ID3D11BlendState *bs;
	FLOAT bf[4];
	UINT mask;
	UINT stencil_ref;
	ID3D11DepthStencilState *dss;
	ID3D11ShaderResourceView *ps_srv;
	ID3D11SamplerState *sampler;
	ID3D11PixelShader *ps;
	ID3D11VertexShader *vs;
	ID3D11GeometryShader *gs;
	UINT ps_count;
	UINT vs_count;
	UINT gs_count;
	ID3D11ClassInstance *ps_inst[256];
	ID3D11ClassInstance *vs_inst[256];
	ID3D11ClassInstance *gs_inst[256];
	D3D11_PRIMITIVE_TOPOLOGY topology;
	ID3D11Buffer *ib;
	ID3D11Buffer *vb;
	ID3D11Buffer *vs_cb;
	UINT ib_offset;
	UINT vb_stride;
	UINT vb_offset;
	DXGI_FORMAT ib_fmt;
	ID3D11InputLayout *il;
};

struct im_dx11_cb {
	float proj[4][4];
};

static void im_dx11_push_state(ID3D11DeviceContext *context, struct im_dx11_state *s)
{
	s->sr_count = s->vp_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	ID3D11DeviceContext_RSGetScissorRects(context, &s->sr_count, s->sr);
	ID3D11DeviceContext_RSGetViewports(context, &s->vp_count, s->vp);
	ID3D11DeviceContext_RSGetState(context, &s->rs);
	ID3D11DeviceContext_OMGetBlendState(context, &s->bs, s->bf, &s->mask);
	ID3D11DeviceContext_OMGetDepthStencilState(context, &s->dss, &s->stencil_ref);
	ID3D11DeviceContext_PSGetShaderResources(context, 0, 1, &s->ps_srv);
	ID3D11DeviceContext_PSGetSamplers(context, 0, 1, &s->sampler);

	s->ps_count = s->vs_count = s->gs_count = 256;
	ID3D11DeviceContext_PSGetShader(context, &s->ps, s->ps_inst, &s->ps_count);
	ID3D11DeviceContext_VSGetShader(context, &s->vs, s->vs_inst, &s->vs_count);
	ID3D11DeviceContext_VSGetConstantBuffers(context, 0, 1, &s->vs_cb);
	ID3D11DeviceContext_GSGetShader(context, &s->gs, s->gs_inst, &s->gs_count);

	ID3D11DeviceContext_IAGetPrimitiveTopology(context, &s->topology);
	ID3D11DeviceContext_IAGetIndexBuffer(context, &s->ib, &s->ib_fmt, &s->ib_offset);
	ID3D11DeviceContext_IAGetVertexBuffers(context, 0, 1, &s->vb, &s->vb_stride, &s->vb_offset);
	ID3D11DeviceContext_IAGetInputLayout(context, &s->il);
}

static void im_dx11_pop_state(ID3D11DeviceContext *context, struct im_dx11_state *s)
{
	ID3D11DeviceContext_IASetInputLayout(context, s->il);
	ID3D11DeviceContext_IASetVertexBuffers(context, 0, 1, &s->vb, &s->vb_stride, &s->vb_offset);
	ID3D11DeviceContext_IASetIndexBuffer(context, s->ib, s->ib_fmt, s->ib_offset);
	ID3D11DeviceContext_IASetPrimitiveTopology(context, s->topology);

	ID3D11DeviceContext_GSSetShader(context, s->gs, s->gs_inst, s->gs_count);
	ID3D11DeviceContext_VSSetConstantBuffers(context, 0, 1, &s->vs_cb);
	ID3D11DeviceContext_VSSetShader(context, s->vs, s->vs_inst, s->vs_count);
	ID3D11DeviceContext_PSSetShader(context, s->ps, s->ps_inst, s->ps_count);

	ID3D11DeviceContext_PSSetSamplers(context, 0, 1, &s->sampler);
	ID3D11DeviceContext_PSSetShaderResources(context, 0, 1, &s->ps_srv);
	ID3D11DeviceContext_OMSetDepthStencilState(context, s->dss, s->stencil_ref);
	ID3D11DeviceContext_OMSetBlendState(context, s->bs, s->bf, s->mask);
	ID3D11DeviceContext_RSSetState(context, s->rs);
	ID3D11DeviceContext_RSSetViewports(context, s->vp_count, s->vp);
	ID3D11DeviceContext_RSSetScissorRects(context, s->sr_count, s->sr);

	if (s->il)
		ID3D11InputLayout_Release(s->il);

	if (s->vb)
		ID3D11Buffer_Release(s->vb);

	if (s->ib)
		ID3D11Buffer_Release(s->ib);

	if (s->gs)
		ID3D11GeometryShader_Release(s->gs);

	if (s->vs_cb)
		ID3D11Buffer_Release(s->vs_cb);

	for (UINT i = 0; i < s->vs_count; i++)
		if (s->vs_inst[i])
			ID3D11ClassInstance_Release(s->vs_inst[i]);

	if (s->vs)
		ID3D11VertexShader_Release(s->vs);

	for (UINT i = 0; i < s->ps_count; i++)
		if (s->ps_inst[i])
			ID3D11ClassInstance_Release(s->ps_inst[i]);

	if (s->ps)
		ID3D11PixelShader_Release(s->ps);

	if (s->sampler)
		ID3D11SamplerState_Release(s->sampler);

	if (s->ps_srv)
		ID3D11ShaderResourceView_Release(s->ps_srv);

	if (s->dss)
		ID3D11DepthStencilState_Release(s->dss);

	if (s->bs)
		ID3D11BlendState_Release(s->bs);

	if (s->rs)
		ID3D11RasterizerState_Release(s->rs);
}

static HRESULT im_dx11_resize_buffer(ID3D11Device *device, struct im_dx11_buffer *buf,
	uint32_t len, uint32_t incr, uint32_t element_size, enum D3D11_BIND_FLAG bind_flag)
{
	if (buf->len < len) {
		if (buf->res) {
			ID3D11Resource_Release(buf->res);
			buf->res = NULL;
		}

		if (buf->b) {
			ID3D11Buffer_Release(buf->b);
			buf->b = NULL;
		}

		D3D11_BUFFER_DESC desc = {0};
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = (len + incr) * element_size;
		desc.BindFlags = bind_flag;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT e = ID3D11Device_CreateBuffer(device, &desc, NULL, &buf->b);
		if (e != S_OK)
			return e;

		e = ID3D11Buffer_QueryInterface(buf->b, &IID_ID3D11Resource, &buf->res);
		if (e != S_OK)
			return e;

		buf->len = len + incr;
	}

	return S_OK;
}

void im_dx11_render(struct im_dx11 *ctx, const struct im_draw_data *dd, ID3D11Device *device, ID3D11DeviceContext *context)
{
	// Prevent rendering under invalid scenarios
	if (dd->display_size.x <= 0 || dd->display_size.y <= 0 || dd->cmd_list_len == 0)
		return;

	// Resize vertex and index buffers if necessary
	HRESULT e = im_dx11_resize_buffer(device, &ctx->vb, dd->vtx_len, VTX_INCR, sizeof(struct im_vtx),
		D3D11_BIND_VERTEX_BUFFER);
	if (e != S_OK)
		return;

	e = im_dx11_resize_buffer(device, &ctx->ib, dd->idx_len, IDX_INCR, sizeof(uint16_t),
		D3D11_BIND_INDEX_BUFFER);
	if (e != S_OK)
		return;

	// Map both vertex and index buffers and bulk copy the data
	D3D11_MAPPED_SUBRESOURCE vtx_map = {0};
	e = ID3D11DeviceContext_Map(context, ctx->vb.res, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_map);
	if (e != S_OK)
		return;

	D3D11_MAPPED_SUBRESOURCE idx_map = {0};
	e = ID3D11DeviceContext_Map(context, ctx->ib.res, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_map);
	if (e != S_OK)
		return;

	struct im_vtx *vtx_dst = (struct im_vtx *) vtx_map.pData;
	uint16_t *idx_dst = (uint16_t *) idx_map.pData;

	for (uint32_t n = 0; n < dd->cmd_list_len; n++) {
		struct im_cmd_list *cmd_list = &dd->cmd_list[n];

		memcpy(vtx_dst, cmd_list->vtx, cmd_list->vtx_len * sizeof(struct im_vtx));
		memcpy(idx_dst, cmd_list->idx, cmd_list->idx_len * sizeof(uint16_t));

		vtx_dst += cmd_list->vtx_len;
		idx_dst += cmd_list->idx_len;
	}

	ID3D11DeviceContext_Unmap(context, ctx->ib.res, 0);
	ID3D11DeviceContext_Unmap(context, ctx->vb.res, 0);

	// Update the vertex shader's projection data based on the current display size
	float L = dd->display_pos.x;
	float R = dd->display_pos.x + dd->display_size.x;
	float T = dd->display_pos.y;
	float B = dd->display_pos.y + dd->display_size.y;
	float proj[4][4] = {
		{2.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 2.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.5f, 0.0f},
		{0.0f, 0.0f, 0.5f, 1.0f},
	};

	proj[0][0] /= R - L;
	proj[1][1] /= T - B;
	proj[3][0] = (R + L) / (L - R);
	proj[3][1] = (T + B) / (B - T);

	D3D11_MAPPED_SUBRESOURCE cb_map = {0};
	e = ID3D11DeviceContext_Map(context, ctx->cb_res, 0, D3D11_MAP_WRITE_DISCARD, 0, &cb_map);
	if (e != S_OK)
		return;

	struct im_dx11_cb *cb = (struct im_dx11_cb *) cb_map.pData;
	memcpy(&cb->proj, proj, sizeof(proj));
	ID3D11DeviceContext_Unmap(context, ctx->cb_res, 0);

	// Store current context state
	struct im_dx11_state state = {0};
	im_dx11_push_state(context, &state);

	// Set viewport based on display size
	D3D11_VIEWPORT vp = {0};
	vp.Width = dd->display_size.x;
	vp.Height = dd->display_size.y;
	vp.MaxDepth = 1.0f;
	ID3D11DeviceContext_RSSetViewports(context, 1, &vp);

	// Set up rendering pipeline
	uint32_t stride = sizeof(struct im_vtx);
	uint32_t offset = 0;
	ID3D11DeviceContext_IASetInputLayout(context, ctx->il);
	ID3D11DeviceContext_IASetVertexBuffers(context, 0, 1, &ctx->vb.b, &stride, &offset);
	ID3D11DeviceContext_IASetIndexBuffer(context, ctx->ib.b, DXGI_FORMAT_R16_UINT, 0);
	ID3D11DeviceContext_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D11DeviceContext_VSSetShader(context, ctx->vs, NULL, 0);
	ID3D11DeviceContext_VSSetConstantBuffers(context, 0, 1, &ctx->cb);
	ID3D11DeviceContext_PSSetShader(context, ctx->ps, NULL, 0);
	ID3D11DeviceContext_PSSetSamplers(context, 0, 1, &ctx->sampler);
	ID3D11DeviceContext_GSSetShader(context, NULL, NULL, 0);
	ID3D11DeviceContext_HSSetShader(context, NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..
	ID3D11DeviceContext_DSSetShader(context, NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..
	ID3D11DeviceContext_CSSetShader(context, NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..

	const float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	ID3D11DeviceContext_OMSetBlendState(context, ctx->bs, blend_factor, 0xFFFFFFFF);
	ID3D11DeviceContext_OMSetDepthStencilState(context, ctx->dss, 0);
	ID3D11DeviceContext_RSSetState(context, ctx->rs);

	// Draw
	uint32_t idx_offset = 0;
	uint32_t vtx_offset = 0;

	for (uint32_t n = 0; n < dd->cmd_list_len; n++) {
		struct im_cmd_list *cmd_list = &dd->cmd_list[n];

		for (uint32_t cmd_i = 0; cmd_i < cmd_list->cmd_len; cmd_i++) {
			struct im_cmd *pcmd = &cmd_list->cmd[cmd_i];

			// Use the clip_rect to apply scissor
			D3D11_RECT r = {0};
			r.left = lrint(pcmd->clip_rect.x - dd->display_pos.x);
			r.top = lrint(pcmd->clip_rect.y - dd->display_pos.y);
			r.right = lrint(pcmd->clip_rect.z - dd->display_pos.x);
			r.bottom = lrint(pcmd->clip_rect.w - dd->display_pos.y);

			// Make sure the rect is actually in the viewport
			if (r.left < dd->display_size.x && r.top < dd->display_size.y && r.right >= 0.0f && r.bottom >= 0.0f) {
				ID3D11DeviceContext_RSSetScissorRects(context, 1, &r);

				// Optionally sample from a texture (fonts, images)
				ID3D11ShaderResourceView *texture_srv = (ID3D11ShaderResourceView *) pcmd->texture_id;
				ID3D11DeviceContext_PSSetShaderResources(context, 0, 1, &texture_srv);

				// Draw indexed
				ID3D11DeviceContext_DrawIndexed(context, pcmd->elem_count, pcmd->idx_offset + idx_offset, pcmd->vtx_offset + vtx_offset);
			}
		}

		idx_offset += cmd_list->idx_len;
		vtx_offset += cmd_list->vtx_len;
	}

	// Restore previous context state
	im_dx11_pop_state(context, &state);
}

bool im_dx11_create(ID3D11Device *device, const void *font, uint32_t width, uint32_t height, struct im_dx11 **dx11)
{
	struct im_dx11 *ctx = *dx11 = calloc(1, sizeof(struct im_dx11));

	ID3D11Texture2D *tex = NULL;
	ID3D11Resource *res = NULL;

	// Create vertex, pixel shaders from precompiled data from headers
	HRESULT e = ID3D11Device_CreateVertexShader(device, VERTEX_SHADER, sizeof(VERTEX_SHADER), NULL, &ctx->vs);
	if (e != S_OK)
		goto except;

	e = ID3D11Device_CreatePixelShader(device, PIXEL_SHADER, sizeof(PIXEL_SHADER), NULL, &ctx->ps);
	if (e != S_OK)
		goto except;

	// Input layout describing the shape of the vertex buffer
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(struct im_vtx, pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(struct im_vtx, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",	 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(struct im_vtx, col), D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	e = ID3D11Device_CreateInputLayout(device, layout, 3, VERTEX_SHADER, sizeof(VERTEX_SHADER), &ctx->il);
	if (e != S_OK)
		goto except;

	// Pre create a constant buffer used for storing the vertex shader's projection data
	D3D11_BUFFER_DESC desc = {0};
	desc.ByteWidth = sizeof(struct im_dx11_cb);
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	e = ID3D11Device_CreateBuffer(device, &desc, NULL, &ctx->cb);
	if (e != S_OK)
		goto except;

	e = ID3D11Buffer_QueryInterface(ctx->cb, &IID_ID3D11Resource, &ctx->cb_res);
	if (e != S_OK)
		goto except;

	// Blend state
	D3D11_BLEND_DESC bdesc = {0};
	bdesc.AlphaToCoverageEnable = false;
	bdesc.RenderTarget[0].BlendEnable = true;
	bdesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bdesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bdesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bdesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bdesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	e = ID3D11Device_CreateBlendState(device, &bdesc, &ctx->bs);
	if (e != S_OK)
		goto except;

	// Rastersizer state enabling scissoring
	D3D11_RASTERIZER_DESC rdesc = {0};
	rdesc.FillMode = D3D11_FILL_SOLID;
	rdesc.CullMode = D3D11_CULL_NONE;
	rdesc.ScissorEnable = true;
	rdesc.DepthClipEnable = true;
	e = ID3D11Device_CreateRasterizerState(device, &rdesc, &ctx->rs);
	if (e != S_OK)
		goto except;

	// Depth stencil state
	D3D11_DEPTH_STENCIL_DESC sdesc = {0};
	sdesc.DepthEnable = false;
	sdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	sdesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	sdesc.StencilEnable = false;
	sdesc.FrontFace.StencilFailOp = sdesc.FrontFace.StencilDepthFailOp
		= sdesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	sdesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	sdesc.BackFace = sdesc.FrontFace;
	e = ID3D11Device_CreateDepthStencilState(device, &sdesc, &ctx->dss);
	if (e != S_OK)
		goto except;

	// Font texture, ultimately kept as a ShaderResourceView
	D3D11_TEXTURE2D_DESC tdesc = {0};
	tdesc.Width = width;
	tdesc.Height = height;
	tdesc.MipLevels = 1;
	tdesc.ArraySize = 1;
	tdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tdesc.SampleDesc.Count = 1;
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA subResource = {0};
	subResource.pSysMem = font;
	subResource.SysMemPitch = tdesc.Width * 4;
	e = ID3D11Device_CreateTexture2D(device, &tdesc, &subResource, &tex);
	if (e != S_OK)
		goto except;

	e = ID3D11Texture2D_QueryInterface(tex, &IID_ID3D11Resource, &res);
	if (e != S_OK)
		goto except;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {0};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = tdesc.MipLevels;
	e = ID3D11Device_CreateShaderResourceView(device, res, &srvDesc, &ctx->font);
	if (e != S_OK)
		goto except;

	// Sampler state for font texture
	D3D11_SAMPLER_DESC samdesc = {0};
	samdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samdesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samdesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samdesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samdesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	e = ID3D11Device_CreateSamplerState(device, &samdesc, &ctx->sampler);
	if (e != S_OK)
		goto except;

	except:

	if (res)
		ID3D11Resource_Release(res);

	if (tex)
		ID3D11Texture2D_Release(tex);

	if (e != S_OK)
		im_dx11_destroy(dx11);

	return e == S_OK;
}

ID3D11ShaderResourceView *im_dx11_font_texture(struct im_dx11 *ctx)
{
	return ctx->font;
}

void im_dx11_destroy(struct im_dx11 **dx11)
{
	if (!dx11 || !*dx11)
		return;

	struct im_dx11 *ctx = *dx11;

	if (ctx->sampler)
		ID3D11SamplerState_Release(ctx->sampler);

	if (ctx->font)
		ID3D11ShaderResourceView_Release(ctx->font);

	if (ctx->vb.res)
		ID3D11Resource_Release(ctx->vb.res);

	if (ctx->ib.res)
		ID3D11Resource_Release(ctx->ib.res);

	if (ctx->ib.b)
		ID3D11Buffer_Release(ctx->ib.b);

	if (ctx->vb.b)
		ID3D11Buffer_Release(ctx->vb.b);

	if (ctx->bs)
		ID3D11BlendState_Release(ctx->bs);

	if (ctx->dss)
		ID3D11DepthStencilState_Release(ctx->dss);

	if (ctx->rs)
		ID3D11RasterizerState_Release(ctx->rs);

	if (ctx->ps)
		ID3D11PixelShader_Release(ctx->ps);

	if (ctx->cb_res)
		ID3D11Resource_Release(ctx->cb_res);

	if (ctx->cb)
		ID3D11Buffer_Release(ctx->cb);

	if (ctx->il)
		ID3D11InputLayout_Release(ctx->il);

	if (ctx->vs)
		ID3D11VertexShader_Release(ctx->vs);

	free(ctx);
	*dx11 = NULL;
}

#pragma warning(pop)
