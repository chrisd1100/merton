#include "window-quad.h"

#include "shaders/ps.h"
#include "shaders/vs.h"

struct psvars {
	float width;
	float height;
	float constrain_w;
	float constrain_h;
	uint32_t filter;
	uint32_t effect;
};

struct window_quad {
	uint32_t width;
	uint32_t height;
	struct psvars psvars;
	ID3D11VertexShader *vs;
	ID3D11PixelShader *ps;
	ID3D11Buffer *vb;
	ID3D11Buffer *ib;
	ID3D11Buffer *psb;
	ID3D11InputLayout *il;
	ID3D11SamplerState *ss_nearest;
	ID3D11SamplerState *ss_linear;
	ID3D11Texture2D *texture;
	ID3D11Resource *resource;
	ID3D11ShaderResourceView *srv;
	ID3D11BlendState *blend;
};

HRESULT window_quad_init(ID3D11Device *device, struct window_quad **quad)
{
	struct window_quad *ctx = *quad = calloc(1, sizeof(struct window_quad));

	HRESULT e = ID3D11Device_CreateVertexShader(device, VS, sizeof(VS), NULL, &ctx->vs);
	e = ID3D11Device_CreatePixelShader(device, PS, sizeof(PS), NULL, &ctx->ps);
	if (e != S_OK) goto except;

	float vertex_data[] = {
		-1.0f, -1.0f,  // position0 (bottom-left)
		 0.0f,  1.0f,  // texcoord0
		-1.0f,  1.0f,  // position1 (top-left)
		 0.0f,  0.0f,  // texcoord1
		 1.0f,  1.0f,  // position2 (top-right)
		 1.0f,  0.0f,  // texcoord2
		 1.0f, -1.0f,  // position3 (bottom-right)
		 1.0f,  1.0f   // texcoord3
	};

	D3D11_BUFFER_DESC bd = {0};
	bd.ByteWidth = sizeof(vertex_data);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = {0};
	srd.pSysMem = vertex_data;
	e = ID3D11Device_CreateBuffer(device, &bd, &srd, &ctx->vb);
	if (e != S_OK) goto except;

	DWORD index_data[] = {
		0, 1, 2,
		2, 3, 0
	};

	D3D11_BUFFER_DESC ibd = {0};
	ibd.ByteWidth = sizeof(index_data);
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA isrd = {0};
	isrd.pSysMem = index_data;
	e = ID3D11Device_CreateBuffer(device, &ibd, &isrd, &ctx->ib);
	if (e != S_OK) goto except;

	D3D11_BUFFER_DESC psbd = {0};
	psbd.ByteWidth = sizeof(struct psvars);
	psbd.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA pssrd = {0};
	pssrd.pSysMem = &ctx->psvars;
	e = ID3D11Device_CreateBuffer(device, &psbd, &pssrd, &ctx->psb);
	if (e != S_OK) goto except;

	D3D11_INPUT_ELEMENT_DESC ied[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 2 * sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	e = ID3D11Device_CreateInputLayout(device, ied, 2, VS, sizeof(VS), &ctx->il);
	if (e != S_OK) goto except;

	D3D11_SAMPLER_DESC sdesc = {0};
	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sdesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	e = ID3D11Device_CreateSamplerState(device, &sdesc, &ctx->ss_nearest);
	if (e != S_OK) goto except;

	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	e = ID3D11Device_CreateSamplerState(device, &sdesc, &ctx->ss_linear);
	if (e != S_OK) goto except;

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
	e = ID3D11Device_CreateBlendState(device, &bdesc, &ctx->blend);
	if (e != S_OK) goto except;

	except:

	if (e != S_OK)
		window_quad_destroy(quad);

	return e;
}

static void window_quad_destroy_resource(struct window_quad *ctx)
{
	if (ctx->srv) {
		ID3D11ShaderResourceView_Release(ctx->srv);
		ctx->srv = NULL;
	}

	if (ctx->resource) {
		ID3D11Resource_Release(ctx->resource);
		ctx->resource = NULL;
	}

	if (ctx->texture) {
		ID3D11Texture2D_Release(ctx->texture);
		ctx->texture = NULL;
	}
}

static HRESULT window_quad_refresh_resource(struct window_quad *ctx, ID3D11Device *device,
	uint32_t width, uint32_t height)
{
	HRESULT e = S_OK;

	if (!ctx->texture || !ctx->srv || !ctx->resource || width != ctx->width || height != ctx->height) {
		window_quad_destroy_resource(ctx);

		D3D11_TEXTURE2D_DESC desc = {0};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		e = ID3D11Device_CreateTexture2D(device, &desc, NULL, &ctx->texture);
		if (e != S_OK) goto except;

		e = ID3D11Texture2D_QueryInterface(ctx->texture, &IID_ID3D11Resource, &ctx->resource);
		if (e != S_OK) goto except;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {0};
		srvd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels = 1;

		e = ID3D11Device_CreateShaderResourceView(device, ctx->resource, &srvd, &ctx->srv);
		if (e != S_OK) goto except;

		ctx->width = width;
		ctx->height = height;
	}

	except:

	if (e != S_OK)
		window_quad_destroy_resource(ctx);

	return e;
}

static HRESULT window_quad_copy(struct window_quad *ctx, ID3D11DeviceContext *context,
	const void *image, uint32_t width, uint32_t height)
{
	D3D11_MAPPED_SUBRESOURCE res = {0};
	HRESULT e = ID3D11DeviceContext_Map(context, ctx->resource, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);

	if (e == S_OK) {
		memcpy((uint8_t *) res.pData, image, sizeof(uint32_t) * width * height);
		ID3D11DeviceContext_Unmap(context, ctx->resource, 0);
	}

	return e;
}

static D3D11_VIEWPORT window_quad_viewport(UINT width, UINT height,
	uint32_t constrain_w, uint32_t constrain_h, uint32_t window_w, uint32_t window_h, float aspect_ratio)
{
	if (constrain_w == 0 || constrain_h == 0 || window_w < constrain_w || window_h < constrain_h) {
		constrain_w = window_w;
		constrain_h = window_h;
	}

	D3D11_VIEWPORT viewport = {0};
	viewport.Width = (float) constrain_w;
	viewport.Height = viewport.Width / aspect_ratio;

	if ((float) constrain_h / (float) height < ((float) constrain_w / aspect_ratio) / (float) width) {
		viewport.Height = (float) constrain_h;
		viewport.Width = viewport.Height * aspect_ratio;
	}

	viewport.TopLeftX = ((float) window_w - viewport.Width) / 2.0f;
	viewport.TopLeftY = ((float) window_h - viewport.Height) / 2.0f;

	return viewport;
}

static void window_quad_draw(struct window_quad *ctx, ID3D11DeviceContext *context,
	ID3D11RenderTargetView *rtv, enum filter filter)
{
	UINT stride = 4 * sizeof(float);
	UINT offset = 0;

	ID3D11DeviceContext_VSSetShader(context, ctx->vs, NULL, 0);
	ID3D11DeviceContext_PSSetShader(context, ctx->ps, NULL, 0);
	ID3D11DeviceContext_PSSetShaderResources(context, 0, 1, &ctx->srv);
	ID3D11DeviceContext_PSSetSamplers(context, 0, 1, filter == FILTER_NEAREST ? &ctx->ss_nearest : &ctx->ss_linear);
	ID3D11DeviceContext_PSSetConstantBuffers(context, 0, 1, &ctx->psb);
	ID3D11DeviceContext_IASetVertexBuffers(context, 0, 1, &ctx->vb, &stride, &offset);
	ID3D11DeviceContext_IASetIndexBuffer(context, ctx->ib, DXGI_FORMAT_R32_UINT, 0);
	ID3D11DeviceContext_IASetInputLayout(context, ctx->il);
	ID3D11DeviceContext_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rtv, NULL);

	float blend_factor[4] = {0};
	ID3D11DeviceContext_OMSetBlendState(context, ctx->blend, blend_factor, 0xFFFFFFFF);

	FLOAT clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	ID3D11DeviceContext_ClearRenderTargetView(context, rtv, clear_color);
	ID3D11DeviceContext_DrawIndexed(context, 6, 0, 0);
}

HRESULT window_quad_render(struct window_quad *ctx, ID3D11Device *device, ID3D11DeviceContext *context,
	const void *image, uint32_t width, uint32_t height, uint32_t constrain_w, uint32_t constrain_h,
	ID3D11Texture2D *dest, float aspect_ratio, enum filter filter, enum effect effect)
{
	HRESULT e = window_quad_refresh_resource(ctx, device, width, height);
	if (e != S_OK) return e;

	e = window_quad_copy(ctx, context, image, width, height);
	if (e != S_OK) return e;

	D3D11_TEXTURE2D_DESC desc = {0};
	ID3D11Texture2D_GetDesc(dest, &desc);

	D3D11_VIEWPORT vp = window_quad_viewport(ctx->width, ctx->height, constrain_w, constrain_h,
		desc.Width, desc.Height, aspect_ratio);

	ID3D11DeviceContext_RSSetViewports(context, 1, &vp);

	ID3D11Resource *cbresource = NULL;
	e = ID3D11Buffer_QueryInterface(ctx->psb, &IID_ID3D11Resource, &cbresource);

	if (e == S_OK) {
		ctx->psvars.width = (float) width;
		ctx->psvars.height = (float) height;
		ctx->psvars.constrain_w = (float) vp.Width;
		ctx->psvars.constrain_h = (float) vp.Height;
		ctx->psvars.filter = filter;
		ctx->psvars.effect = effect;
		ID3D11DeviceContext_UpdateSubresource(context, cbresource, 0, 0, &ctx->psvars, 0, 0);
		ID3D11Resource_Release(cbresource);
	}

	ID3D11Resource *resource = NULL;
	e = ID3D11Texture2D_QueryInterface(dest, &IID_ID3D11Resource, &resource);

	if (e == S_OK) {
		ID3D11RenderTargetView *rtv = NULL;
		e = ID3D11Device_CreateRenderTargetView(device, resource, NULL, &rtv);

		if (e == S_OK) {
			window_quad_draw(ctx, context, rtv, filter);
			ID3D11RenderTargetView_Release(rtv);
		}

		ID3D11Resource_Release(resource);
	}

	return e;
}

void window_quad_destroy(struct window_quad **quad)
{
	if (!quad || !*quad)
		return;

	struct window_quad *ctx = *quad;

	window_quad_destroy_resource(ctx);

	if (ctx->blend)
		ID3D11BlendState_Release(ctx->blend);

	if (ctx->ss_linear)
		ID3D11SamplerState_Release(ctx->ss_linear);

	if (ctx->ss_nearest)
		ID3D11SamplerState_Release(ctx->ss_nearest);

	if (ctx->il)
		ID3D11InputLayout_Release(ctx->il);

	if (ctx->psb)
		ID3D11Buffer_Release(ctx->psb);

	if (ctx->ib)
		ID3D11Buffer_Release(ctx->ib);

	if (ctx->vb)
		ID3D11Buffer_Release(ctx->vb);

	if (ctx->ps)
		ID3D11PixelShader_Release(ctx->ps);

	if (ctx->vs)
		ID3D11VertexShader_Release(ctx->vs);

	free(ctx);
	*quad = NULL;
}
