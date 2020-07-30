// Copyright (c) 2019-2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cpu.h"
#include "apu.h"

enum mem {
	UNMAPPED = 0x000000,
	ROM      = 0x010000,
	RAM      = 0x100000,
	CIRAM    = RAM | 0x000100,
	EXRAM    = RAM | 0x000200,
	SPRROM   = ROM | 0x000010,
	BGROM    = ROM | 0x000011,
};

struct cart;

// IO
uint8_t cart_prg_read(struct cart *cart, struct apu *apu, uint16_t addr, bool *mem_hit);
void cart_prg_write(struct cart *cart, struct apu *apu, uint16_t addr, uint8_t v);
uint8_t cart_chr_read(struct cart *cart, uint16_t addr, enum mem type, bool nt);
void cart_chr_write(struct cart *cart, uint16_t addr, uint8_t v);

// Hooks
void cart_ppu_a12_toggle(struct cart *cart);
void cart_ppu_write_hook(struct cart *cart, uint16_t addr, uint8_t v);
bool cart_block_2007(struct cart *cart);

// Step
void cart_step(struct cart *cart, struct cpu *cpu);

// SRAM
size_t cart_sram_dirty(struct cart *cart);
void cart_sram_get(struct cart *cart, void *sram, size_t size);

// Lifecycle
void cart_create(const void *rom, size_t rom_size, const void *sram, size_t sram_size,
	const NES_CartDesc *desc, struct cart **cart);
void cart_destroy(struct cart **cart);
void cart_reset(struct cart *cart);
size_t cart_set_state(struct cart *cart, const void *state, size_t size);
void *cart_get_state(struct cart *cart, size_t *size);
