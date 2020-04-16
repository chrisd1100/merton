#include "window-quad.h"

#include "shaders/library.h"

struct window_quad {
	uint32_t width;
	uint32_t height;
	id<MTLLibrary> library;
	id<MTLFunction> fs;
	id<MTLFunction> vs;
	id<MTLBuffer> vb;
	id<MTLBuffer> ib;
	id<MTLRenderPipelineState> pipeline;
	id<MTLSamplerState> ss;
	id<MTLTexture> texture;
};

bool window_quad_init(id<MTLDevice> device, struct window_quad **quad)
{
	bool r = true;
	struct window_quad *ctx = *quad = calloc(1, sizeof(struct window_quad));

	dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	dispatch_data_t data = dispatch_data_create(MTL_LIBRARY, sizeof(MTL_LIBRARY),
		queue, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

	NSError *nse = nil;
	ctx->library = [device newLibraryWithData:data error:&nse];
	if (nse != nil) {r = false; NSLog(@"%@", nse); goto except;}

	ctx->vs = [ctx->library newFunctionWithName:@"vs"];
	ctx->fs = [ctx->library newFunctionWithName:@"fs"];

	float vdata[] = {
		-1.0f,  1.0f,	// Position 0
		 0.0f,  0.0f,	// TexCoord 0
		-1.0f, -1.0f,	// Position 1
		 0.0f,  1.0f,	// TexCoord 1
		 1.0f, -1.0f,	// Position 2
		 1.0f,  1.0f,	// TexCoord 2
		 1.0f,  1.0f,	// Position 3
		 1.0f,  0.0f	// TexCoord 3
	};

	ctx->vb = [device newBufferWithBytes:vdata length:sizeof(vdata) options:MTLResourceOptionCPUCacheModeDefault];
	ctx->vb.label = @"vb";

	uint16_t idata[] = {
		0, 1, 2,
		2, 3, 0
	};

	ctx->ib = [device newBufferWithBytes:idata length:sizeof(idata) options:MTLResourceOptionCPUCacheModeDefault];
	ctx->ib.label = @"ib";

	MTLRenderPipelineDescriptor *pdesc = [MTLRenderPipelineDescriptor new];
	pdesc.label = @"LIBQuad";
	pdesc.sampleCount = 1;
	pdesc.vertexFunction = ctx->vs;
	pdesc.fragmentFunction = ctx->fs;
	pdesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	pdesc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;

	ctx->pipeline = [device newRenderPipelineStateWithDescriptor:pdesc error:&nse];
	[pdesc release];
	if (nse != nil) {r = false; NSLog(@"%@", nse); goto except;}

	MTLSamplerMinMagFilter filter = MTLSamplerMinMagFilterNearest;
	MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
	sd.minFilter = sd.magFilter = filter;
	sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
	sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
	ctx->ss = [device newSamplerStateWithDescriptor:sd];
	[sd release];

	except:

	if (!r)
		window_quad_destroy(quad);

	return r;
}

static void window_quad_refresh_resource(struct window_quad *ctx, id<MTLDevice> device,
	uint32_t width, uint32_t height)
{
	if (!ctx->texture || width != ctx->width || height != ctx->height) {
		if (ctx->texture) {
			[ctx->texture release];
			ctx->texture = nil;
		}

		MTLTextureDescriptor *tdesc = [MTLTextureDescriptor new];
		tdesc.pixelFormat = MTLPixelFormatRGBA8Unorm;
		tdesc.width = width;
		tdesc.height = height;

		ctx->texture = [device newTextureWithDescriptor:tdesc];
		[tdesc release];

		ctx->width = width;
		ctx->height = height;
	}
}

static MTLViewport window_quad_viewport(uint32_t width, uint32_t height, uint32_t window_w, uint32_t window_h, float aspect_ratio)
{
	MTLViewport vp = {0};
	vp.width = (double) window_w;
	vp.height = vp.width / aspect_ratio;

	if ((double) window_h / (double) height < ((double) window_w / aspect_ratio) / (double) width) {
		vp.height = (double) window_h;
		vp.width = vp.height * aspect_ratio;
	}

	vp.originX = ((double) window_w - vp.width) / 2.0;
	vp.originY = ((double) window_h - vp.height) / 2.0;

	return vp;
}

static void window_quad_draw(struct window_quad *ctx, id<MTLCommandQueue> cq, id<MTLTexture> dest, float aspect_ratio)
{
	MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
	rpd.colorAttachments[0].texture = dest;
	rpd.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
	rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
	rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLCommandBuffer> cb = [cq commandBuffer];
	id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];

	[re setRenderPipelineState:ctx->pipeline];
	[re setViewport:window_quad_viewport(ctx->width, ctx->height, dest.width, dest.height, aspect_ratio)];
	[re setVertexBuffer:ctx->vb offset:0 atIndex:0];
	[re setFragmentTexture:ctx->texture atIndex:0];
	[re setFragmentSamplerState:ctx->ss atIndex:0];
	[re drawIndexedPrimitives:MTLPrimitiveTypeTriangleStrip indexCount:6
		indexType:MTLIndexTypeUInt16 indexBuffer:ctx->ib indexBufferOffset:0];

	[re endEncoding];
	[cb commit];

	[rpd release];
	[cb release];
	[re release];
}

void window_quad_render(struct window_quad *ctx, id<MTLCommandQueue> cq,
	const void *image, uint32_t width, uint32_t height, id<MTLTexture> dest, float aspect_ratio)
{
	window_quad_refresh_resource(ctx, cq.device, width, height);

	MTLRegion region = MTLRegionMake2D(0, 0, width, height);
	[ctx->texture replaceRegion:region mipmapLevel:0 withBytes:image bytesPerRow:4 * width];

	window_quad_draw(ctx, cq, dest, aspect_ratio);
}

void window_quad_destroy(struct window_quad **quad)
{
	if (!quad || !*quad)
		return;

	struct window_quad *ctx = *quad;

	free(ctx);
	*quad = NULL;
}
