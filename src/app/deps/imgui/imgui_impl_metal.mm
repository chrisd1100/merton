#include "imgui.h"
#include "imgui_impl_metal.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h> // Not supported in XCode 9.2. Maybe a macro to detect the SDK version can be used (something like #if MACOS_SDK >= 10.13 ...)
#import <simd/simd.h>

// A singleton that stores long-lived objects that are needed by the Metal
// renderer backend. Stores the render pipeline state cache and the default
// font texture, and manages the reusable buffer cache.
struct mtl {
	id<MTLDepthStencilState> depthStencilState;
	id<MTLRenderPipelineState> renderPipelineState;
	id<MTLTexture> fontTexture;
};

void im_mtl_render(struct mtl *ctx, ImDrawData* drawData, void *ocq, void *otexture)
{
	id<MTLCommandQueue> cq = (id<MTLCommandQueue>) ocq;
	id<MTLTexture> texture = (id<MTLTexture>) otexture;

	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	int fb_width = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
	int fb_height = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0 || drawData->CmdListsCount == 0)
		return;

	MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
	rpd.colorAttachments[0].texture = texture;
	rpd.colorAttachments[0].loadAction = MTLLoadActionLoad;
	rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLCommandBuffer> cb = [cq commandBuffer];
	id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];

	size_t vertexBufferLength = drawData->TotalVtxCount * sizeof(ImDrawVert);
	size_t indexBufferLength = drawData->TotalIdxCount * sizeof(ImDrawIdx);

	id<MTLBuffer> vertexBuffer = [cb.device newBufferWithLength:vertexBufferLength options:MTLResourceStorageModeShared];
	id<MTLBuffer> indexBuffer = [cb.device newBufferWithLength:indexBufferLength options:MTLResourceStorageModeShared];

	[re setCullMode:MTLCullModeNone];
	[re setDepthStencilState:ctx->depthStencilState];

	// Setup viewport, orthographic projection matrix
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to
	// draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayMin is typically (0,0) for single viewport apps.
	MTLViewport viewport = {
		.originX = 0.0,
		.originY = 0.0,
		.width = (double)(drawData->DisplaySize.x * drawData->FramebufferScale.x),
		.height = (double)(drawData->DisplaySize.y * drawData->FramebufferScale.y),
		.znear = 0.0,
		.zfar = 1.0
	};
	[re setViewport:viewport];

	float L = drawData->DisplayPos.x;
	float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
	float T = drawData->DisplayPos.y;
	float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
	float N = viewport.znear;
	float F = viewport.zfar;
	const float ortho_projection[4][4] =
	{
		{ 2.0f/(R-L),   0.0f,		   0.0f,   0.0f },
		{ 0.0f,		 2.0f/(T-B),	 0.0f,   0.0f },
		{ 0.0f,		 0.0f,		1/(F-N),   0.0f },
		{ (R+L)/(L-R),  (T+B)/(B-T), N/(F-N),   1.0f },
	};

	[re setVertexBytes:&ortho_projection length:sizeof(ortho_projection) atIndex:1];
	[re setRenderPipelineState:ctx->renderPipelineState];
	[re setVertexBuffer:vertexBuffer offset:0 atIndex:0];

	// Will project scissor/clipping rectangles into framebuffer space
	ImVec2 clip_off = drawData->DisplayPos;		 // (0,0) unless using multi-viewports
	ImVec2 clip_scale = drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

	// Render command lists
	size_t vertexBufferOffset = 0;
	size_t indexBufferOffset = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++) {
		const ImDrawList* cmd_list = drawData->CmdLists[n];

		memcpy((char *)vertexBuffer.contents + vertexBufferOffset, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy((char *)indexBuffer.contents + indexBufferOffset, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

			// Project scissor/clipping rectangles into framebuffer space
			ImVec4 clip_rect;
			clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
			clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
			clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
			clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

			if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
			{
				// Apply scissor/clipping rectangle
				MTLScissorRect scissorRect = {
					.x = NSUInteger(clip_rect.x),
					.y = NSUInteger(clip_rect.y),
					.width = NSUInteger(clip_rect.z - clip_rect.x),
					.height = NSUInteger(clip_rect.w - clip_rect.y)
				};
				[re setScissorRect:scissorRect];

				// Bind texture, Draw
				if (pcmd->TextureId != NULL)
					[re setFragmentTexture:(__bridge id<MTLTexture>)(pcmd->TextureId) atIndex:0];

				[re setVertexBufferOffset:(vertexBufferOffset + pcmd->VtxOffset * sizeof(ImDrawVert)) atIndex:0];
				[re drawIndexedPrimitives:MTLPrimitiveTypeTriangle
										   indexCount:pcmd->ElemCount
											indexType:sizeof(ImDrawIdx) == 2 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
										  indexBuffer:indexBuffer
									indexBufferOffset:indexBufferOffset + pcmd->IdxOffset * sizeof(ImDrawIdx)];
			}
		}

		vertexBufferOffset += cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
		indexBufferOffset += cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
	}

	[vertexBuffer release];
	[indexBuffer release];

	[re endEncoding];
	[cb commit];

	[rpd release];
	[cb release];
	[re release];
}

bool im_mtl_create(void *odevice, const void *font, int32_t font_w, int32_t font_h, struct mtl **mtl)
{
	id<MTLDevice> device = (id<MTLDevice>) odevice;

	struct mtl *ctx = *mtl = (struct mtl *) calloc(1, sizeof(struct mtl));

	MTLDepthStencilDescriptor *depthStencilDescriptor = [[MTLDepthStencilDescriptor alloc] init];
	depthStencilDescriptor.depthWriteEnabled = NO;
	depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
	ctx->depthStencilState = [device newDepthStencilStateWithDescriptor:depthStencilDescriptor];
	[depthStencilDescriptor release];

	NSError *error = nil;

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
	if (!library) {
		NSLog(@"Error: failed to create Metal library: %@", error);
		return nil;
	}

	id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
	id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];

	if (!vertexFunction || !fragmentFunction) {
		NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
		return nil;
	}

	MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
	vertexDescriptor.attributes[0].offset = IM_OFFSETOF(ImDrawVert, pos);
	vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2; // position
	vertexDescriptor.attributes[0].bufferIndex = 0;
	vertexDescriptor.attributes[1].offset = IM_OFFSETOF(ImDrawVert, uv);
	vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2; // texCoords
	vertexDescriptor.attributes[1].bufferIndex = 0;
	vertexDescriptor.attributes[2].offset = IM_OFFSETOF(ImDrawVert, col);
	vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4; // color
	vertexDescriptor.attributes[2].bufferIndex = 0;
	vertexDescriptor.layouts[0].stepRate = 1;
	vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
	vertexDescriptor.layouts[0].stride = sizeof(ImDrawVert);

	MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
	pipelineDescriptor.vertexFunction = vertexFunction;
	pipelineDescriptor.fragmentFunction = fragmentFunction;
	pipelineDescriptor.vertexDescriptor = vertexDescriptor;
	pipelineDescriptor.sampleCount = 1;
	pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
	pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
	pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
	pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
	pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
	pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

	ctx->renderPipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
	if (error)
		NSLog(@"Error: failed to create Metal pipeline state: %@", error);

	[pipelineDescriptor release];
	[vertexDescriptor release];
	[fragmentFunction release];
	[vertexFunction release];
	[library release];

	MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		width:font_w height:font_h mipmapped:NO];

	textureDescriptor.usage = MTLTextureUsageShaderRead;
	#if TARGET_OS_OSX
		textureDescriptor.storageMode = MTLStorageModeManaged;
	#else
		textureDescriptor.storageMode = MTLStorageModeShared;
	#endif
	ctx->fontTexture = [device newTextureWithDescriptor:textureDescriptor];
	[ctx->fontTexture replaceRegion:MTLRegionMake2D(0, 0, font_w, font_h) mipmapLevel:0 withBytes:font bytesPerRow:font_w * 4];
	[textureDescriptor release];

	return true;
}

void *im_mtl_font_texture(struct mtl *ctx)
{
	return ctx->fontTexture;
}

void im_mtl_destroy(struct mtl **mtl)
{
	if (!mtl || !*mtl)
		return;

	struct mtl *ctx = *mtl;

	if (ctx->fontTexture)
		[ctx->fontTexture release];

	if (ctx->depthStencilState)
		[ctx->depthStencilState release];

	if (ctx->renderPipelineState)
		[ctx->renderPipelineState release];

	free(ctx);
	*mtl = NULL;
}


/*** UTILITY ***/

void im_mtl_texture_size(void *texture, float *width, float *height)
{
	id<MTLTexture> mtltex = (id<MTLTexture>) texture;

	*width = mtltex.width;
	*height = mtltex.height;
}

void *im_mtl_get_drawable_texture(void *drawable)
{
	id<CAMetalDrawable> d = (id<CAMetalDrawable>) drawable;

	return d.texture;
}
