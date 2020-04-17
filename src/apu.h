#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "sys.h"
#include "cpu.h"

enum extaudio {
	EXT_NONE = 0,
	EXT_MMC5 = 1,
};

struct apu;

/*** READ & WRITE ***/
uint8_t apu_read_status(struct apu *apu, struct cpu *cpu, enum extaudio ext);
void apu_write(struct apu *apu, NES *nes, struct cpu *cpu, uint16_t addr, uint8_t v, enum extaudio ext);

/*** RUN ***/
void apu_step(struct apu *apu, NES *nes, struct cpu *cpu,
	NES_AudioCallback new_samples, const void *opaque);

/*** INIT & DESTROY ***/
void apu_set_stereo(struct apu *apu, bool stereo);
uint32_t apu_get_channels(struct apu *apu);
void apu_set_channels(struct apu *apu, uint32_t channels);
void apu_set_clock(struct apu *apu, uint32_t hz);
void apu_create(uint32_t sample_rate, bool stereo, struct apu **apu);
void apu_destroy(struct apu **apu);
void apu_reset(struct apu *apu, NES *nes, struct cpu *cpu, bool hard);
