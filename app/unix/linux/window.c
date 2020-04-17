#include "lib.h"

enum lib_status window_create(WINDOW_MSG_FUNC msg_func, const void *opaque, uint32_t width,
	uint32_t height, struct window **window)
{
	return LIB_OK;
}

void window_poll(struct window *ctx)
{
}

void window_present(struct window *ctx, uint32_t num_frames)
{
}

void window_render_quad(struct window *ctx, const void *image, uint32_t width,
	uint32_t height, float aspect_ratio)
{
}

void window_destroy(struct window **window)
{
}
