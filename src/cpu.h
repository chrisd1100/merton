#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "sys.h"

enum irq {
	IRQ_APU    = 0x01,
	IRQ_DMC    = 0x02,
	IRQ_MAPPER = 0x04,
};

struct cpu;

void cpu_irq(struct cpu *cpu, enum irq irq, bool enabled);
void cpu_nmi(struct cpu *cpu, bool enabled);
void cpu_dma_oam(struct cpu *cpu, NES *nes, uint8_t v, bool odd_cycle);
uint8_t cpu_dma_dmc(struct cpu *cpu, NES *nes, uint16_t addr, bool in_write, bool begin_oam);
void cpu_step(struct cpu *cpu, NES *nes);
void cpu_create(struct cpu **cpu);
void cpu_destroy(struct cpu **cpu);
void cpu_reset(struct cpu *cpu, NES *nes, bool hard);
void *cpu_get_state(struct cpu *cpu, size_t *size);
size_t cpu_set_state(struct cpu *cpu, const void *state, size_t size);
