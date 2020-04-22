#include "sys.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cart.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"

#define NES_LOG_MAX 1024

static NES_LogCallback NES_LOG;

struct NES {
	struct sys {
		uint8_t ram[0x0800];
		uint8_t io_open_bus;
		uint32_t frame;
		uint16_t read_addr;
		uint16_t write_addr;
		uint64_t cycle;
		uint64_t cycle_2007;
		bool odd_cycle;
	} sys;

	struct ctrl {
		bool strobe;
		uint32_t state[2];
		uint32_t bits[2];
		uint8_t buttons[4];
		uint8_t safe_buttons[4];
	} ctrl;

	struct cart *cart;
	struct cpu *cpu;
	struct ppu *ppu;
	struct apu *apu;

	const void *opaque;
	NES_VideoCallback new_frame;
	NES_AudioCallback new_samples;
};


/*** CONTROLLER ***/

static void ctrl_set_state(struct ctrl *ctrl, uint8_t player, uint8_t state)
{
	switch (player) {
		case 0:
			ctrl->state[0] = ((ctrl->state[0] & 0x00FFFF00) | (0x08 << 16) | state);
			break;
		case 1:
			ctrl->state[1] = ((ctrl->state[1] & 0x00FFFF00) | (0x04 << 16) | state);
			break;
		case 2:
			ctrl->state[0] = ((ctrl->state[0] & 0x000000FF) | (0x08 << 16) | (state << 8));
			break;
		case 3:
			ctrl->state[1] = ((ctrl->state[1] & 0x000000FF) | (0x04 << 16) | (state << 8));
			break;
	}
}

static void ctrl_set_safe_state(struct ctrl *ctrl, uint8_t player)
{
	uint8_t prev_state = ctrl->safe_buttons[player];
	ctrl->safe_buttons[player] = ctrl->buttons[player];

	// cancel out up + down
	if ((ctrl->safe_buttons[player] & 0x30) == 0x30)
		ctrl->safe_buttons[player] &= 0xCF;

	// cancel out left + right
	if ((ctrl->safe_buttons[player] & 0xC0) == 0xC0)
		ctrl->safe_buttons[player] &= 0x3F;

	if (prev_state != ctrl->safe_buttons[player])
		ctrl_set_state(ctrl, player, ctrl->safe_buttons[player]);
}

static uint8_t ctrl_read(struct ctrl *ctrl, uint8_t n)
{
	if (ctrl->strobe)
		return 0x40 | (ctrl->state[n] & 0x01);

	uint8_t r = 0x40 | (ctrl->bits[n] & 0x01);
	ctrl->bits[n] = (n < 2 ? 0x80 : 0x80000000) | (ctrl->bits[n] >> 1);

	return r;
}

static void ctrl_write(struct ctrl *ctrl, bool strobe)
{
	if (ctrl->strobe && !strobe) {
		ctrl->bits[0] = ctrl->state[0];
		ctrl->bits[1] = ctrl->state[1];
	}

	ctrl->strobe = strobe;
}


/*** SYSTEM ***/

// https://wiki.nesdev.com/w/index.php/CPU_memory_map

uint8_t sys_read(NES *nes, uint16_t addr)
{
	if (addr < 0x2000) {
		return nes->sys.ram[addr % 0x0800];

	} else if (addr < 0x4000) {
		addr = 0x2000 + addr % 8;

		// double 2007 read glitch and mapper 185 copy protection
		if (addr == 0x2007 && (nes->sys.cycle - nes->sys.cycle_2007 == 1 || cart_block_2007(nes->cart)))
			return ppu_read(nes->ppu, nes->cpu, nes->cart, 0x2003);

		nes->sys.cycle_2007 = nes->sys.cycle;
		return ppu_read(nes->ppu, nes->cpu, nes->cart, addr);

	} else if (addr == 0x4015) {
		nes->sys.io_open_bus = apu_read_status(nes->apu, nes->cpu, EXT_NONE);
		return nes->sys.io_open_bus;

	} else if (addr == 0x4016 || addr == 0x4017) {
		nes->sys.io_open_bus = ctrl_read(&nes->ctrl, addr & 1);
		return nes->sys.io_open_bus;

	} else if (addr >= 0x4020) {
		bool mem_hit = false;
		uint8_t v = cart_prg_read(nes->cart, nes->cpu, nes->apu, addr, &mem_hit);

		if (mem_hit) return v;
	}

	return nes->sys.io_open_bus;
}

uint8_t sys_read_dmc(NES *nes, uint16_t addr)
{
	if (nes->sys.read_addr == 0x2007) {
		ppu_read(nes->ppu, nes->cpu, nes->cart, 0x2007);
		ppu_read(nes->ppu, nes->cpu, nes->cart, 0x2007);
	}

	if (nes->sys.read_addr == 0x4016 || nes->sys.read_addr == 0x4017)
		sys_read(nes, nes->sys.read_addr);

	return cpu_dma_dmc(nes->cpu, nes, addr, nes->sys.write_addr != 0, nes->sys.write_addr == 0x4014);
}

void sys_write(NES *nes, uint16_t addr, uint8_t v)
{
	if (addr < 0x2000) {
		nes->sys.ram[addr % 0x800] = v;

	} else if (addr < 0x4000) {
		addr = 0x2000 + addr % 8;

		ppu_write(nes->ppu, nes->cpu, nes->cart, addr, v);
		cart_ppu_write_hook(nes->cart, addr, v); //MMC5 listens here

	} else if (addr < 0x4014 || addr == 0x4015 || addr == 0x4017) {
		nes->sys.io_open_bus = v;
		apu_write(nes->apu, nes, nes->cpu, addr, v, EXT_NONE);

	} else if (addr == 0x4014) {
		nes->sys.io_open_bus = v;
		cpu_dma_oam(nes->cpu, nes, v, nes->sys.odd_cycle);

	} else if (addr == 0x4016) {
		nes->sys.io_open_bus = v;
		ctrl_write(&nes->ctrl, v & 0x01);

	} else if (addr < 0x4020) {
		nes->sys.io_open_bus = v;

	} else {
		cart_prg_write(nes->cart, nes->cpu, nes->apu, addr, v);
	}
}

void sys_pre_tick_write(NES *nes, uint16_t addr)
{
	nes->sys.write_addr = addr;

	nes->sys.frame += ppu_step(nes->ppu, nes->cpu, nes->cart, nes->new_frame, nes->opaque);
	nes->sys.frame += ppu_step(nes->ppu, nes->cpu, nes->cart, nes->new_frame, nes->opaque);
	nes->sys.frame += ppu_step(nes->ppu, nes->cpu, nes->cart, nes->new_frame, nes->opaque);

	apu_step(nes->apu, nes, nes->cpu, nes->new_samples, nes->opaque);
}

void sys_post_tick_write(NES *nes)
{
	cart_step(nes->cart, nes->cpu);

	nes->sys.cycle++;
	nes->sys.odd_cycle = !nes->sys.odd_cycle;
	nes->sys.write_addr = 0;
}

void sys_pre_tick_read(NES *nes, uint16_t addr)
{
	nes->sys.read_addr = addr;

	nes->sys.frame += ppu_step(nes->ppu, nes->cpu, nes->cart, nes->new_frame, nes->opaque);
	nes->sys.frame += ppu_step(nes->ppu, nes->cpu, nes->cart, nes->new_frame, nes->opaque);

	apu_step(nes->apu, nes, nes->cpu, nes->new_samples, nes->opaque);
}

void sys_post_tick_read(NES *nes)
{
	nes->sys.frame += ppu_step(nes->ppu, nes->cpu, nes->cart, nes->new_frame, nes->opaque);

	cart_step(nes->cart, nes->cpu);

	nes->sys.cycle++;
	nes->sys.odd_cycle = !nes->sys.odd_cycle;
	nes->sys.read_addr = 0;
}

void sys_tick(NES *nes)
{
	sys_pre_tick_read(nes, 0);
	sys_post_tick_read(nes);
}

static void sys_reset(struct sys *sys, bool hard)
{
	sys->odd_cycle = false;
	sys->frame = 0;
	sys->read_addr = sys->write_addr = 0;
	sys->cycle = sys->cycle_2007 = 0;

	if (hard)
		memset(sys->ram, 0, 0x0800);
}


/*** PUBLIC ***/

void NES_Create(NES_VideoCallback videoCallback, NES_AudioCallback audioCallback, const void *opaque,
	uint32_t sampleRate, bool stereo, NES **nes)
{
	NES *ctx = *nes = calloc(1, sizeof(NES));

	ctx->opaque = opaque;
	ctx->new_frame = videoCallback;
	ctx->new_samples = audioCallback;

	cpu_create(&ctx->cpu);
	ppu_create(&ctx->ppu);
	apu_create(sampleRate, stereo, &ctx->apu);
}

void NES_LoadCart(NES *ctx, const void *rom, size_t romSize, const void *sram, size_t sramSize, const NES_CartDesc *hdr)
{
	if (ctx->cart)
		cart_destroy(&ctx->cart);

	if (rom) {
		cart_create(rom, romSize, sram, sramSize, hdr, &ctx->cart);
		NES_Reset(ctx, true);
	}
}

bool NES_CartLoaded(NES *ctx)
{
	return ctx->cart;
}

uint32_t NES_NextFrame(NES *ctx)
{
	if (!ctx->cart)
		return 0;

	uint64_t cycles = ctx->sys.cycle;

	for (uint32_t frame = ctx->sys.frame; frame == ctx->sys.frame; cpu_step(ctx->cpu, ctx));

	return (uint32_t) (ctx->sys.cycle - cycles);
}

void NES_ControllerButton(NES *nes, uint8_t player, NES_Button button, bool pressed)
{
	struct ctrl *ctrl = &nes->ctrl;

	if (pressed) {
		ctrl->buttons[player] |= button;

	} else {
		ctrl->buttons[player] &= ~button;
	}

	ctrl_set_safe_state(ctrl, player);
}

void NES_ControllerState(NES *nes, uint8_t player, uint8_t state)
{
	struct ctrl *ctrl = &nes->ctrl;

	ctrl->buttons[player] = state;
	ctrl_set_safe_state(ctrl, player);
}

void NES_Reset(NES *ctx, bool hard)
{
	if (!ctx->cart)
		return;

	sys_reset(&ctx->sys, hard);
	ppu_reset(ctx->ppu);
	apu_reset(ctx->apu, ctx, ctx->cpu, hard);
	cpu_reset(ctx->cpu, ctx, hard);

	ctx->sys.frame += ppu_step(ctx->ppu, ctx->cpu, ctx->cart, ctx->new_frame, ctx->opaque);
	ctx->sys.frame += ppu_step(ctx->ppu, ctx->cpu, ctx->cart, ctx->new_frame, ctx->opaque);
}

void NES_SetStereo(NES *ctx, bool stereo)
{
	apu_set_stereo(ctx->apu, stereo);
}

void NES_SetSampleRate(NES *ctx, uint32_t sampleRate)
{
	apu_set_sample_rate(ctx->apu, sampleRate);
}

void NES_SetAPUClock(NES *ctx, uint32_t hz)
{
	apu_set_clock(ctx->apu, hz);
}

void NES_SetChannels(NES *ctx, uint32_t channels)
{
	apu_set_channels(ctx->apu, channels);
}

size_t NES_SRAMDirty(NES *ctx)
{
	return ctx->cart ? cart_sram_dirty(ctx->cart) : 0;
}

void NES_GetSRAM(NES *ctx, void *sram, size_t size)
{
	if (ctx->cart)
		cart_sram_get(ctx->cart, sram, size);
}

void NES_Destroy(NES **nes)
{
	if (!nes || !*nes)
		return;

	NES *ctx = *nes;

	apu_destroy(&ctx->apu);
	ppu_destroy(&ctx->ppu);
	cpu_destroy(&ctx->cpu);
	cart_destroy(&ctx->cart);

	free(ctx);
	*nes = NULL;
}

void NES_SetLogCallback(NES_LogCallback log_callback)
{
	NES_LOG = log_callback;
}

void NES_Log(const char *fmt, ...)
{
	if (NES_LOG) {
		va_list args;
		va_start(args, fmt);

		char str[NES_LOG_MAX];
		vsnprintf(str, NES_LOG_MAX, fmt, args);

		NES_LOG(str);

		va_end(args);
	}
}
