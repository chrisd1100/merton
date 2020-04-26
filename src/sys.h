#pragma once

#include <stdint.h>

#include "nes.h"

#define SET_FLAG(reg, flag)   ((reg) |= (flag))
#define GET_FLAG(reg, flag)   ((reg) & (flag))
#define UNSET_FLAG(reg, flag) ((reg) &= ~(flag))

uint8_t sys_read(NES *nes, uint16_t addr);
uint8_t sys_read_dmc(NES *nes, uint16_t addr);
void sys_write(NES *nes, uint16_t addr, uint8_t v);
void sys_pre_tick_write(NES *nes, uint16_t addr);
void sys_post_tick_write(NES *nes);
void sys_pre_tick_read(NES *nes, uint16_t addr);
void sys_post_tick_read(NES *nes);
void sys_tick(NES *nes);
bool sys_odd_cycle(NES *nes);
