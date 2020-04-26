#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cart.h"
#include "cpu.h"

struct ppu;

uint8_t ppu_read(struct ppu *ppu, struct cpu *cpu, struct cart *cart, uint16_t addr);
void ppu_write(struct ppu *ppu, struct cpu *cpu, struct cart *cart, uint16_t addr, uint8_t v);
void ppu_clock(struct ppu *ppu);
bool ppu_step(struct ppu *ppu, struct cpu *cpu, struct cart *cart,
	NES_VideoCallback new_frame, const void *opaque);
void ppu_create(struct ppu **ppu);
void ppu_destroy(struct ppu **ppu);
void ppu_reset(struct ppu *ppu);
void *ppu_get_state(struct ppu *ppu, size_t *size);
size_t ppu_set_state(struct ppu *ppu, const void *state, size_t size);
