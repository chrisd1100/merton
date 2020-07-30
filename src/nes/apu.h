// Copyright (c) 2019-2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "sys.h"
#include "cpu.h"

enum extaudio {
	EXT_NONE = 0,
	EXT_MMC5 = 1,
	EXT_VRC6 = 2,
	EXT_SS5B = 3,
};

struct apu;

// IO
void apu_dma_dmc_finish(struct apu *apu, uint8_t v);
uint8_t apu_read_status(struct apu *apu, enum extaudio ext);
void apu_write(struct apu *apu, NES *nes, uint16_t addr, uint8_t v, enum extaudio ext);

// Step
void apu_step(struct apu *apu, NES *nes);
void apu_assert_irqs(struct apu *apu, struct cpu *cpu);
const int16_t *apu_frames(struct apu *apu, uint32_t *count);

// Configuration
void apu_set_config(struct apu *apu, const NES_Config *cfg);
void apu_clock_drift(struct apu *apu, uint32_t clock, bool over);

// Lifecycle
void apu_create(const NES_Config *cfg, struct apu **apu);
void apu_destroy(struct apu **apu);
void apu_reset(struct apu *apu, NES *nes, bool hard);
size_t apu_set_state(struct apu *apu, const void *state, size_t size);
void *apu_get_state(struct apu *apu, size_t *size);
