#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cart.h"
#include "cpu.h"

struct ppu;

/*** READ & WRITE ***/
uint8_t ppu_read(struct ppu *ppu, struct cpu *cpu, struct cart *cart, uint16_t addr);
void ppu_write(struct ppu *ppu, struct cpu *cpu, struct cart *cart, uint16_t addr, uint8_t v);

/*** RUN ***/
bool ppu_step(struct ppu *ppu, struct cpu *cpu, struct cart *cart,
	NES_VideoCallback new_frame, const void *opaque);

/*** INIT & DESTROY ***/
void ppu_create(struct ppu **ppu);
void ppu_destroy(struct ppu **ppu);
void ppu_reset(struct ppu *ppu);
