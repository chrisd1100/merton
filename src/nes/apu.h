#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "sys.h"
#include "cpu.h"

enum extaudio {
	EXT_NONE = 0,
	EXT_MMC5 = 1,
	EXT_VRC6 = 2,
};

struct apu;

void apu_dma_dmc_finish(struct apu *apu, uint8_t v);
uint8_t apu_read_status(struct apu *apu, struct cpu *cpu, enum extaudio ext);
void apu_write(struct apu *apu, NES *nes, struct cpu *cpu, uint16_t addr, uint8_t v, enum extaudio ext);
void apu_step(struct apu *apu, NES *nes, struct cpu *cpu,
	NES_AudioCallback new_samples, const void *opaque);
void apu_set_stereo(struct apu *apu, bool stereo);
void apu_set_sample_rate(struct apu *apu, uint32_t sample_rate);
uint32_t apu_get_channels(struct apu *apu);
void apu_set_channels(struct apu *apu, uint32_t channels);
void apu_set_clock(struct apu *apu, uint32_t hz);
void apu_create(uint32_t sample_rate, bool stereo, struct apu **apu);
void apu_destroy(struct apu **apu);
void apu_reset(struct apu *apu, NES *nes, struct cpu *cpu, bool hard);
void *apu_get_state(struct apu *apu, size_t *size);
size_t apu_set_state(struct apu *apu, const void *state, size_t size);
