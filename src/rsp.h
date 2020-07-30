// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdint.h>
#include <stddef.h>

struct rsp;

struct rsp *rsp_create(void);
void rsp_destroy(struct rsp **rsp);
const int16_t *rsp_convert(struct rsp *ctx, uint32_t rate_in, uint32_t rate_out,
	const int16_t *in, size_t *size);
void rsp_reset(struct rsp *ctx);
