#include "im-mtl.h"

#include <math.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

struct im_mtl {
	id<MTLDepthStencilState> dss;
	id<MTLRenderPipelineState> rps;
	id<MTLTexture> font;
	id<MTLBuffer> vb;
	id<MTLBuffer> ib;
	uint32_t vb_len;
	uint32_t ib_len;
};

void im_mtl_render(struct im_mtl *ctx, const struct im_draw_data *dd, MTL_CommandQueue *ocq, MTL_Texture *otexture)
{
	id<MTLCommandQueue> cq = (__bridge id<MTLCommandQueue>) ocq;
	id<MTLTexture> texture = (__bridge id<MTLTexture>) otexture;

	int32_t fb_width = lrint(dd->display_size.x * dd->framebuffer_scale.x);
	int32_t fb_height = lrint(dd->display_size.y * dd->framebuffer_scale.y);

	if (fb_width <= 0 || fb_height <= 0 || dd->cmd_list_len == 0)
		return;

	if (dd->vtx_len > ctx->vb_len) {
		ctx->vb_len = dd->vtx_len + VTX_INCR;
		ctx->vb = [cq.device newBufferWithLength:ctx->vb_len * sizeof(struct im_vtx) options:MTLResourceStorageModeShared];
	}

	if (dd->idx_len > ctx->ib_len) {
		ctx->ib_len = dd->idx_len + IDX_INCR;
		ctx->ib = [cq.device newBufferWithLength:ctx->ib_len * sizeof(uint16_t) options:MTLResourceStorageModeShared];
	}

	id<MTLCommandBuffer> cb = [cq commandBuffer];

	MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
	rpd.colorAttachments[0].texture = texture;
	rpd.colorAttachments[0].loadAction = MTLLoadActionLoad;
	rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
	id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];

	[re setCullMode:MTLCullModeNone];
	[re setDepthStencilState:ctx->dss];

	MTLViewport viewport = {
		.originX = 0.0,
		.originY = 0.0,
		.width = dd->display_size.x * dd->framebuffer_scale.x,
		.height = dd->display_size.y * dd->framebuffer_scale.y,
		.znear = 0.0,
		.zfar = 1.0
	};
	[re setViewport:viewport];

	float L = dd->display_pos.x;
	float R = dd->display_pos.x + dd->display_size.x;
	float T = dd->display_pos.y;
	float B = dd->display_pos.y + dd->display_size.y;
	float N = viewport.znear;
	float F = viewport.zfar;
	const float ortho[4][4] = {
		{2.0f / (R-L),   0.0f,         0.0f,         0.0f},
		{0.0f,           2.0f / (T-B), 0.0f,         0.0f},
		{0.0f,           0.0f,         1.0f / (F-N), 0.0f},
		{(R+L) / (L-R), (T+B) / (B-T), N / (F-N),    1.0f},
	};

	[re setVertexBytes:&ortho length:sizeof(ortho) atIndex:1];
	[re setRenderPipelineState:ctx->rps];
	[re setVertexBuffer:ctx->vb offset:0 atIndex:0];

	struct im_vec2 clip_off = dd->display_pos;
	struct im_vec2 clip_scale = dd->framebuffer_scale;

	uint32_t vertex_offset = 0;
	uint32_t index_offset = 0;

	for (uint32_t n = 0; n < dd->cmd_list_len; n++) {
		const struct im_cmd_list *cmd_list = &dd->cmd_list[n];

		memcpy((uint8_t *) ctx->vb.contents + vertex_offset, cmd_list->vtx, cmd_list->vtx_len * sizeof(struct im_vtx));
		memcpy((uint8_t *) ctx->ib.contents + index_offset, cmd_list->idx, cmd_list->idx_len * sizeof(uint16_t));

		for (uint32_t cmd_i = 0; cmd_i < cmd_list->cmd_len; cmd_i++) {
			const struct im_cmd *pcmd = &cmd_list->cmd[cmd_i];

			struct im_vec4 clip_rect = {0};
			clip_rect.x = (pcmd->clip_rect.x - clip_off.x) * clip_scale.x;
			clip_rect.y = (pcmd->clip_rect.y - clip_off.y) * clip_scale.y;
			clip_rect.z = (pcmd->clip_rect.z - clip_off.x) * clip_scale.x;
			clip_rect.w = (pcmd->clip_rect.w - clip_off.y) * clip_scale.y;

			if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
				MTLScissorRect scissorRect = {
					.x = lrint(clip_rect.x),
					.y = lrint(clip_rect.y),
					.width = lrint(clip_rect.z - clip_rect.x),
					.height = lrint(clip_rect.w - clip_rect.y)
				};

				[re setScissorRect:scissorRect];

				if (pcmd->texture_id)
					[re setFragmentTexture:(__bridge id<MTLTexture>) pcmd->texture_id atIndex:0];

				[re setVertexBufferOffset:(vertex_offset + pcmd->vtx_offset * sizeof(struct im_vtx)) atIndex:0];
				[re drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:pcmd->elem_count
					indexType:MTLIndexTypeUInt16 indexBuffer:ctx->ib
					indexBufferOffset:index_offset + pcmd->idx_offset * sizeof(uint16_t)];
			}
		}

		vertex_offset += cmd_list->vtx_len * sizeof(struct im_vtx);
		index_offset += cmd_list->idx_len * sizeof(uint16_t);
	}

	[re endEncoding];
	[cb commit];
}

bool im_mtl_create(MTL_Device *odevice, const void *font, uint32_t width, uint32_t height, struct im_mtl **mtl)
{
	id<MTLDevice> device = (__bridge id<MTLDevice>) odevice;

	bool r = true;
	NSError *error = nil;

	struct im_mtl *ctx = *mtl = calloc(1, sizeof(struct im_mtl));

	id<MTLFunction> vertexFunction = nil;
	id<MTLFunction> fragmentFunction = nil;
	MTLDepthStencilDescriptor *depthStencilDescriptor = [MTLDepthStencilDescriptor new];
	MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
	MTLRenderPipelineDescriptor *pdesc = [MTLRenderPipelineDescriptor new];
	MTLTextureDescriptor *tdesc = nil;

	NSString *shaderSource = @""
		"#include <metal_stdlib>\n"
		"using namespace metal;\n"
		"\n"
		"struct Uniforms {\n"
		"	float4x4 projectionMatrix;\n"
		"};\n"
		"\n"
		"struct VertexIn {\n"
		"	float2 position  [[attribute(0)]];\n"
		"	float2 texCoords [[attribute(1)]];\n"
		"	uchar4 color	 [[attribute(2)]];\n"
		"};\n"
		"\n"
		"struct VertexOut {\n"
		"	float4 position [[position]];\n"
		"	float2 texCoords;\n"
		"	float4 color;\n"
		"};\n"
		"\n"
		"vertex VertexOut vertex_main(VertexIn in				 [[stage_in]],\n"
		"							 constant Uniforms &uniforms [[buffer(1)]]) {\n"
		"	VertexOut out;\n"
		"	out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);\n"
		"	out.texCoords = in.texCoords;\n"
		"	out.color = float4(in.color) / float4(255.0);\n"
		"	return out;\n"
		"}\n"
		"\n"
		"fragment half4 fragment_main(VertexOut in [[stage_in]],\n"
		"							 texture2d<half, access::sample> texture [[texture(0)]]) {\n"
		"	constexpr sampler linearSampler(coord::normalized, min_filter::linear, mag_filter::linear, mip_filter::linear);\n"
		"	half4 texColor = texture.sample(linearSampler, in.texCoords);\n"
		"	return half4(in.color) * texColor;\n"
		"}\n";

	id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];
	if (error) {
		NSLog(@"Metal error: %@", error);
		r = false;
		goto except;
	}

	depthStencilDescriptor.depthWriteEnabled = NO;
	depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
	ctx->dss = [device newDepthStencilStateWithDescriptor:depthStencilDescriptor];

	vertexFunction = [library newFunctionWithName:@"vertex_main"];
	fragmentFunction = [library newFunctionWithName:@"fragment_main"];

	vertexDescriptor.attributes[0].offset = offsetof(struct im_vtx, pos);
	vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2; // position
	vertexDescriptor.attributes[0].bufferIndex = 0;
	vertexDescriptor.attributes[1].offset = offsetof(struct im_vtx, uv);
	vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2; // texCoords
	vertexDescriptor.attributes[1].bufferIndex = 0;
	vertexDescriptor.attributes[2].offset = offsetof(struct im_vtx, col);
	vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4; // color
	vertexDescriptor.attributes[2].bufferIndex = 0;
	vertexDescriptor.layouts[0].stepRate = 1;
	vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
	vertexDescriptor.layouts[0].stride = sizeof(struct im_vtx);

	pdesc.vertexFunction = vertexFunction;
	pdesc.fragmentFunction = fragmentFunction;
	pdesc.vertexDescriptor = vertexDescriptor;
	pdesc.sampleCount = 1;
	pdesc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
	pdesc.colorAttachments[0].blendingEnabled = YES;
	pdesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
	pdesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
	pdesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	pdesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
	pdesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	pdesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

	ctx->rps = [device newRenderPipelineStateWithDescriptor:pdesc error:&error];
	if (error) {
		NSLog(@"Metal error: %@", error);
		r = false;
		goto except;
	}

	tdesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];

	tdesc.usage = MTLTextureUsageShaderRead;
	#if TARGET_OS_OSX
		tdesc.storageMode = MTLStorageModeManaged;
	#else
		tdesc.storageMode = MTLStorageModeShared;
	#endif
	ctx->font = [device newTextureWithDescriptor:tdesc];

	[ctx->font replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:font bytesPerRow:width * 4];

	except:

	if (!r)
		im_mtl_destroy(mtl);

	return r;
}

MTL_Texture *im_mtl_font_texture(struct im_mtl *ctx)
{
	return (__bridge MTL_Texture *) ctx->font;
}

void im_mtl_destroy(struct im_mtl **mtl)
{
	if (!mtl || !*mtl)
		return;

	struct im_mtl *ctx = *mtl;
	ctx->dss = nil;
	ctx->rps = nil;
	ctx->font = nil;
	ctx->vb = nil;
	ctx->ib = nil;

	free(ctx);
	*mtl = NULL;
}


/*** UTILITY ***/

void im_mtl_texture_size(MTL_Texture *texture, float *width, float *height)
{
	id<MTLTexture> mtltex = (__bridge id<MTLTexture>) texture;

	*width = mtltex.width;
	*height = mtltex.height;
}

MTL_Texture *im_mtl_get_drawable_texture(CA_MetalDrawable *drawable)
{
	id<CAMetalDrawable> d = (__bridge id<CAMetalDrawable>) drawable;

	return (__bridge MTL_Texture *) d.texture;
}
