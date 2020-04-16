#include "lib.h"

#include <stdlib.h>

#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

#include "window-quad.h"

struct window {
	NSWindow *nswindow;
	CAMetalLayer *layer;
	CVDisplayLinkRef display_link;
	dispatch_semaphore_t semaphore;

	id<MTLCommandQueue> cq;

	struct window_quad *quad;
};

static CVReturn window_display_link(CVDisplayLinkRef displayLink, const CVTimeStamp *inNow,
	const CVTimeStamp *inOutputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext)
{
	displayLink; inNow; inOutputTime; flagsIn; flagsOut;

	struct window *ctx = (struct window *) displayLinkContext;

	dispatch_semaphore_signal(ctx->semaphore);

	return 0;
}

enum lib_status window_create(const char *title, WINDOW_MSG_FUNC msg_func, const void *opaque,
	uint32_t width, uint32_t height, struct window **window)
{
	enum lib_status r = LIB_OK;
	struct window *ctx = *window = calloc(1, sizeof(struct window));

	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp finishLaunching];

	NSRect rect = NSMakeRect(0, 0, width, height);
	NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
	ctx->nswindow = [[NSWindow alloc] initWithContentRect:rect styleMask:style
		backing:NSBackingStoreBuffered defer:NO];

	ctx->nswindow.title = [NSString stringWithUTF8String:title];
	[ctx->nswindow center];

	[ctx->nswindow makeKeyAndOrderFront:ctx->nswindow];

	ctx->layer = [CAMetalLayer layer];
	ctx->layer.device = MTLCreateSystemDefaultDevice();
	ctx->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

	ctx->cq = [ctx->layer.device newCommandQueue];

	ctx->nswindow.contentView.wantsLayer = YES;
	ctx->nswindow.contentView.layer = ctx->layer;

	CVReturn e = CVDisplayLinkCreateWithCGDisplay(CGMainDisplayID(), &ctx->display_link);
	if (e != 0) {r = LIB_ERR; goto except;}

	ctx->semaphore = dispatch_semaphore_create(0);

	CVDisplayLinkSetOutputCallback(ctx->display_link, window_display_link, ctx);
	CVDisplayLinkStart(ctx->display_link);

	except:

	if (r != LIB_OK)
		window_destroy(window);

	return r;
}

void window_poll(struct window *ctx)
{
	while (true) {
		NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
		if (!event)
			break;

		[NSApp sendEvent:event];
	}
}

bool window_is_foreground(struct window *ctx)
{
	return ctx->nswindow.isKeyWindow;
}

uint32_t window_refresh_rate(struct window *ctx)
{
	return 60;
}

void window_present(struct window *ctx, uint32_t num_frames)
{
	dispatch_semaphore_wait(ctx->semaphore, DISPATCH_TIME_FOREVER);

	id<CAMetalDrawable> drawable = [ctx->layer nextDrawable];
	id<MTLCommandBuffer> cb = [ctx->cq commandBuffer];

	[cb presentDrawable:drawable];
	[cb commit];
	[cb release];
}

void window_render_quad(struct window *ctx, const void *image, uint32_t width,
	uint32_t height, float aspect_ratio)
{
	if (!ctx->quad) {
		if (!window_quad_init(ctx->cq.device, &ctx->quad))
			return;
	}

	id<CAMetalDrawable> drawable = [ctx->layer nextDrawable];
	window_quad_render(ctx->quad, ctx->cq, image, width, height, drawable.texture, aspect_ratio);
	[drawable release];
}

void window_destroy(struct window **window)
{
	if (!window || !*window)
		return;

	struct window *ctx = *window;

	window_quad_destroy(&ctx->quad);

	free(ctx);
	*window = NULL;
}
