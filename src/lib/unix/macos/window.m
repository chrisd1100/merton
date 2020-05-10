#include "lib.h"

#include <stdlib.h>

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

#include "window-quad.h"

struct window {
	NSWindow *nswindow;
	WINDOW_MSG_FUNC msg_func;
	const void *opaque;

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
	uint32_t width, uint32_t height, bool fullscreen, struct window **window)
{
	enum lib_status r = LIB_OK;
	struct window *ctx = *window = calloc(1, sizeof(struct window));
	ctx->msg_func = msg_func;
	ctx->opaque = opaque;

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

void window_set_title(struct window *ctx, const char *title, const char *subtitle)
{
}

static enum scancode window_keycode_to_wmsg(unsigned short kc)
{
	switch (kc) {
		case kVK_ANSI_A: return SCANCODE_A;
		case kVK_ANSI_S: return SCANCODE_S;
		case kVK_ANSI_D: return SCANCODE_D;
		case kVK_ANSI_F: return SCANCODE_NONE;
		case kVK_ANSI_H: return SCANCODE_NONE;
		case kVK_ANSI_G: return SCANCODE_NONE;
		case kVK_ANSI_Z: return SCANCODE_NONE;
		case kVK_ANSI_X: return SCANCODE_NONE;
		case kVK_ANSI_C: return SCANCODE_NONE;
		case kVK_ANSI_V: return SCANCODE_NONE;
		case kVK_ANSI_B: return SCANCODE_NONE;
		case kVK_ANSI_Q: return SCANCODE_NONE;
		case kVK_ANSI_W: return SCANCODE_W;
		case kVK_ANSI_E: return SCANCODE_NONE;
		case kVK_ANSI_R: return SCANCODE_R;
		case kVK_ANSI_Y: return SCANCODE_NONE;
		case kVK_ANSI_T: return SCANCODE_T;
		case kVK_ANSI_1: return SCANCODE_NONE;
		case kVK_ANSI_2: return SCANCODE_NONE;
		case kVK_ANSI_3: return SCANCODE_NONE;
		case kVK_ANSI_4: return SCANCODE_NONE;
		case kVK_ANSI_6: return SCANCODE_NONE;
		case kVK_ANSI_5: return SCANCODE_NONE;
		case kVK_ANSI_Equal: return SCANCODE_NONE;
		case kVK_ANSI_9: return SCANCODE_NONE;
		case kVK_ANSI_7: return SCANCODE_NONE;
		case kVK_ANSI_Minus: return SCANCODE_NONE;
		case kVK_ANSI_8: return SCANCODE_NONE;
		case kVK_ANSI_0: return SCANCODE_NONE;
		case kVK_ANSI_RightBracket: return SCANCODE_NONE;
		case kVK_ANSI_O: return SCANCODE_O;
		case kVK_ANSI_U: return SCANCODE_NONE;
		case kVK_ANSI_LeftBracket: return SCANCODE_NONE;
		case kVK_ANSI_I: return SCANCODE_NONE;
		case kVK_ANSI_P: return SCANCODE_NONE;
		case kVK_ANSI_L: return SCANCODE_L;
		case kVK_ANSI_J: return SCANCODE_NONE;
		case kVK_ANSI_Quote: return SCANCODE_NONE;
		case kVK_ANSI_K: return SCANCODE_NONE;
		case kVK_ANSI_Semicolon: return SCANCODE_SEMICOLON;
		case kVK_ANSI_Backslash: return SCANCODE_NONE;
		case kVK_ANSI_Comma: return SCANCODE_NONE;
		case kVK_ANSI_Slash: return SCANCODE_NONE;
		case kVK_ANSI_N: return SCANCODE_NONE;
		case kVK_ANSI_M: return SCANCODE_NONE;
		case kVK_ANSI_Period: return SCANCODE_NONE;
		case kVK_ANSI_Grave: return SCANCODE_NONE;
		case kVK_ANSI_KeypadDecimal: return SCANCODE_NONE;
		case kVK_ANSI_KeypadMultiply: return SCANCODE_NONE;
		case kVK_ANSI_KeypadPlus: return SCANCODE_NONE;
		case kVK_ANSI_KeypadClear: return SCANCODE_NONE;
		case kVK_ANSI_KeypadDivide: return SCANCODE_NONE;
		case kVK_ANSI_KeypadEnter: return SCANCODE_NONE;
		case kVK_ANSI_KeypadMinus: return SCANCODE_NONE;
		case kVK_ANSI_KeypadEquals: return SCANCODE_NONE;
		case kVK_ANSI_Keypad0: return SCANCODE_NONE;
		case kVK_ANSI_Keypad1: return SCANCODE_NONE;
		case kVK_ANSI_Keypad2: return SCANCODE_NONE;
		case kVK_ANSI_Keypad3: return SCANCODE_NONE;
		case kVK_ANSI_Keypad4: return SCANCODE_NONE;
		case kVK_ANSI_Keypad5: return SCANCODE_NONE;
		case kVK_ANSI_Keypad6: return SCANCODE_NONE;
		case kVK_ANSI_Keypad7: return SCANCODE_NONE;
		case kVK_ANSI_Keypad8: return SCANCODE_NONE;
		case kVK_ANSI_Keypad9: return SCANCODE_NONE;
		case kVK_Return: return SCANCODE_ENTER;
		case kVK_Tab: return SCANCODE_NONE;
		case kVK_Space: return SCANCODE_SPACE;
		case kVK_Delete: return SCANCODE_NONE;
		case kVK_Escape: return SCANCODE_ESCAPE;
		case kVK_Command: return SCANCODE_NONE;
		case kVK_Shift: return SCANCODE_LSHIFT;
		case kVK_CapsLock: return SCANCODE_NONE;
		case kVK_Option: return SCANCODE_NONE;
		case kVK_Control: return SCANCODE_LCTRL;
		case kVK_RightShift: return SCANCODE_RSHIFT;
		case kVK_RightOption: return SCANCODE_NONE;
		case kVK_RightControl: return SCANCODE_RCTRL;
		case kVK_Function: return SCANCODE_NONE;
		case kVK_F17: return SCANCODE_NONE;
		case kVK_VolumeUp: return SCANCODE_NONE;
		case kVK_VolumeDown: return SCANCODE_NONE;
		case kVK_Mute: return SCANCODE_NONE;
		case kVK_F18: return SCANCODE_NONE;
		case kVK_F19: return SCANCODE_NONE;
		case kVK_F20: return SCANCODE_NONE;
		case kVK_F5: return SCANCODE_NONE;
		case kVK_F6: return SCANCODE_NONE;
		case kVK_F7: return SCANCODE_NONE;
		case kVK_F3: return SCANCODE_NONE;
		case kVK_F8: return SCANCODE_NONE;
		case kVK_F9: return SCANCODE_NONE;
		case kVK_F11: return SCANCODE_NONE;
		case kVK_F13: return SCANCODE_NONE;
		case kVK_F16: return SCANCODE_NONE;
		case kVK_F14: return SCANCODE_NONE;
		case kVK_F10: return SCANCODE_NONE;
		case kVK_F12: return SCANCODE_NONE;
		case kVK_F15: return SCANCODE_NONE;
		case kVK_Help: return SCANCODE_NONE;
		case kVK_Home: return SCANCODE_NONE;
		case kVK_PageUp: return SCANCODE_NONE;
		case kVK_ForwardDelete: return SCANCODE_NONE;
		case kVK_F4: return SCANCODE_NONE;
		case kVK_End: return SCANCODE_NONE;
		case kVK_F2: return SCANCODE_NONE;
		case kVK_PageDown: return SCANCODE_NONE;
		case kVK_F1: return SCANCODE_NONE;
		case kVK_LeftArrow: return SCANCODE_LEFT;
		case kVK_RightArrow: return SCANCODE_RIGHT;
		case kVK_DownArrow: return SCANCODE_DOWN;
		case kVK_UpArrow: return SCANCODE_UP;
		case kVK_ISO_Section: return SCANCODE_NONE;
		case kVK_JIS_Yen: return SCANCODE_NONE;
		case kVK_JIS_Underscore: return SCANCODE_NONE;
		case kVK_JIS_KeypadComma: return SCANCODE_NONE;
		case kVK_JIS_Eisu: return SCANCODE_NONE;
		case kVK_JIS_Kana: return SCANCODE_NONE;
		default:
			break;
	}

	return SCANCODE_NONE;
}

void window_poll(struct window *ctx)
{
	while (true) {
		NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
		if (!event)
			break;

		struct window_msg wmsg = {0};

		switch (event.type) {
			case NSEventTypeKeyUp:
			case NSEventTypeKeyDown: {
				wmsg.keyboard.scancode = window_keycode_to_wmsg(event.keyCode);

				if (wmsg.keyboard.scancode != SCANCODE_NONE) {
					wmsg.type = WINDOW_MSG_KEYBOARD;
					wmsg.keyboard.pressed = event.type == NSEventTypeKeyDown;
				}
				break;
			}
			case NSEventTypeMouseMoved: {
				wmsg.type = WINDOW_MSG_MOUSE_MOTION;
				wmsg.mouseMotion.x = 0;
				wmsg.mouseMotion.y = 0;
				break;
			}
			default:
				break;
		}

		if (wmsg.type != WINDOW_MSG_NONE) {
			ctx->msg_func(&wmsg, ctx->opaque);

		} else {
			[NSApp sendEvent:event];
		}
	}

	uint32_t scale = lrint([ctx->nswindow screen].backingScaleFactor);
	CGSize size = ctx->nswindow.contentView.frame.size;
	size.width *= scale;
	size.height *= scale;

	if (size.width != ctx->layer.drawableSize.width || size.height != ctx->layer.drawableSize.height)
		ctx->layer.drawableSize = size;
}

bool window_is_foreground(struct window *ctx)
{
	return ctx->nswindow.isKeyWindow;
}

uint32_t window_refresh_rate(struct window *ctx)
{
	return 60;
}

float window_get_dpi_scale(struct window *ctx)
{
	return [ctx->nswindow screen].backingScaleFactor;
}

void window_set_fullscreen(struct window *ctx)
{
	if (!window_is_fullscreen(ctx))
		[ctx->nswindow toggleFullScreen:ctx->nswindow];
}

void window_set_windowed(struct window *ctx, uint32_t width, uint32_t height)
{
	if (window_is_fullscreen(ctx))
		[ctx->nswindow toggleFullScreen:ctx->nswindow];
}

bool window_is_fullscreen(struct window *ctx)
{
	return ([ctx->nswindow styleMask] & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen;
}

void window_present(struct window *ctx, uint32_t num_frames)
{
	for (uint32_t x = 0; x < num_frames; x++)
		dispatch_semaphore_wait(ctx->semaphore, DISPATCH_TIME_FOREVER);

	id<CAMetalDrawable> drawable = [ctx->layer nextDrawable];
	id<MTLCommandBuffer> cb = [ctx->cq commandBuffer];

	[cb presentDrawable:drawable];
	[cb commit];
	[cb release];
}

OpaqueDevice *window_get_device(struct window *ctx)
{
	return ctx->cq.device;
}

OpaqueContext *window_get_context(struct window *ctx)
{
	return ctx->cq;
}

OpaqueTexture *window_get_back_buffer(struct window *ctx)
{
	id<CAMetalDrawable> drawable = [ctx->layer nextDrawable];

	return drawable;
}

void window_release_back_buffer(OpaqueTexture *texture)
{
	[(id<CAMetalDrawable>) texture release];
}

void window_render_quad(struct window *ctx, const void *image, uint32_t width,
	uint32_t height, uint32_t constrain_w, uint32_t constrain_h, float aspect_ratio,
	enum filter filter, enum effect effect)
{
	if (!ctx->quad) {
		if (!window_quad_init(ctx->cq.device, &ctx->quad))
			return;
	}

	id<CAMetalDrawable> drawable = [ctx->layer nextDrawable];
	window_quad_render(ctx->quad, ctx->cq, image, width, height,
		constrain_w, constrain_h, drawable.texture, aspect_ratio,
		filter, effect);
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
