// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "rsp.h"

#include "matoya.h"

#include "deps/libsamplerate/samplerate.c"

#define BUF_SIZE (64 * 1024)

struct rsp {
	SRC_STATE *state;
	float *in;
	float *out;
	int16_t *out16;
	size_t in_frames;
};

struct rsp *rsp_create(void)
{
	struct rsp *ctx = MTY_Alloc(1, sizeof(struct rsp));

	// The in and out buffers NEED to be space in the same allocation
	int32_t e = -1;
	ctx->state = src_new(SRC_SINC_FASTEST, 2, &e);
	ctx->in = MTY_Alloc(BUF_SIZE * 2, sizeof(float));
	ctx->out16 = MTY_Alloc(BUF_SIZE * 2, sizeof(int16_t));
	ctx->out = ctx->in + BUF_SIZE;

	return ctx;
}

void rsp_destroy(struct rsp **rsp)
{
	if (!rsp || !*rsp)
		return;

	struct rsp *ctx = *rsp;

	src_delete(ctx->state);

	MTY_Free(ctx->in);
	MTY_Free(ctx->out16);

	MTY_Free(ctx);
	*rsp = NULL;
}

const int16_t *rsp_convert(struct rsp *ctx, uint32_t rate_in, uint32_t rate_out, const int16_t *in, size_t *size)
{
	src_short_to_float_array(in, ctx->in + ctx->in_frames * 2, (int32_t) *size * 2);

	SRC_DATA data = {0};
	data.data_in = ctx->in;
	data.input_frames = (long) ctx->in_frames + (int32_t) *size;
	data.data_out = ctx->out;
	data.output_frames = BUF_SIZE;
	data.src_ratio = (double) rate_out / (double) rate_in;

	if (src_process(ctx->state, &data) != 0)
		return in;

	ctx->in_frames += *size - data.input_frames_used;
	*size = data.output_frames_gen;

	src_float_to_short_array(ctx->out, ctx->out16, (int32_t) *size * 2);

	memmove(ctx->in, ctx->in + data.input_frames_used * 2 * sizeof(float),
		ctx->in_frames * 2 * sizeof(float));

	return ctx->out16;
}

void rsp_reset(struct rsp *ctx)
{
	src_reset(ctx->state);
	ctx->in_frames = 0;
}
