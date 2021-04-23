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
	int16_t *out;
};

struct rsp *rsp_create(void)
{
	struct rsp *ctx = MTY_Alloc(1, sizeof(struct rsp));

	ctx->state = src_new(SRC_SINC_FASTEST);
	ctx->out = MTY_Alloc(BUF_SIZE, sizeof(int16_t));

	return ctx;
}

void rsp_destroy(struct rsp **rsp)
{
	if (!rsp || !*rsp)
		return;

	struct rsp *ctx = *rsp;

	src_delete(ctx->state);

	MTY_Free(ctx->out);

	MTY_Free(ctx);
	*rsp = NULL;
}

const int16_t *rsp_convert(struct rsp *ctx, uint32_t rate_in, uint32_t rate_out, const int16_t *in, size_t *size)
{
	double ratio = (double) rate_out / (double) rate_in;

	if (src_process(ctx->state, in, *size, ctx->out, BUF_SIZE, ratio, size) != 0)
		return in;

	return ctx->out;
}

void rsp_reset(struct rsp *ctx)
{
	src_reset(ctx->state);
}
