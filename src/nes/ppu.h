#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cart.h"
#include "cpu.h"

struct ppu;

// IO
uint8_t ppu_read(struct ppu *ppu, struct cart *cart, uint16_t addr);
void ppu_write(struct ppu *ppu, struct cart *cart, uint16_t addr, uint8_t v);

// Step
void ppu_step(struct ppu *ppu, struct cpu *cpu, struct cart *cart);
void ppu_assert_nmi(struct ppu *ppu, struct cpu *cpu);
bool ppu_new_frame(struct ppu *ppu);
const uint32_t *ppu_pixels(struct ppu *ppu);

// Configuration
void ppu_set_config(struct ppu *ppu, const NES_Config *cfg);

// Lifecycle
void ppu_create(const NES_Config *cfg, struct ppu **ppu);
void ppu_destroy(struct ppu **ppu);
void ppu_reset(struct ppu *ppu);
size_t ppu_set_state(struct ppu *ppu, const void *state, size_t size);
void *ppu_get_state(struct ppu *ppu, size_t *size);
