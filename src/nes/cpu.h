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

// Interrupts
void cpu_irq(struct cpu *cpu, enum irq irq, bool enabled);
void cpu_nmi(struct cpu *cpu, bool enabled);
void cpu_halt(struct cpu *cpu, bool halt);
void cpu_poll_interrupts(struct cpu *cpu);

// Step
void cpu_step(struct cpu *cpu, NES *nes);

// Lifecycle
void cpu_create(struct cpu **cpu);
void cpu_destroy(struct cpu **cpu);
void cpu_reset(struct cpu *cpu, NES *nes, bool hard);
size_t cpu_set_state(struct cpu *cpu, const void *state, size_t size);
void *cpu_get_state(struct cpu *cpu, size_t *size);
