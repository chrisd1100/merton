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

// IO
void apu_dma_dmc_finish(struct apu *apu, uint8_t v);
uint8_t apu_read_status(struct apu *apu, struct cpu *cpu, enum extaudio ext);
void apu_write(struct apu *apu, NES *nes, struct cpu *cpu, uint16_t addr, uint8_t v, enum extaudio ext);

// Step
void apu_step(struct apu *apu, NES *nes, struct cpu *cpu);
const int16_t *apu_frames(struct apu *apu, uint32_t *count);

// Configuration
void apu_set_config(struct apu *apu, const NES_Config *cfg);

// Lifecycle
void apu_create(const NES_Config *cfg, struct apu **apu);
void apu_destroy(struct apu **apu);
void apu_reset(struct apu *apu, NES *nes, struct cpu *cpu, bool hard);
size_t apu_set_state(struct apu *apu, const void *state, size_t size);
void *apu_get_state(struct apu *apu, size_t *size);
