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
		uint16_t read_addr;
		bool in_write;
		uint64_t cycle;
		uint64_t cycle_2007;

		struct {
			bool oam_begin;
			bool dmc_begin;
			bool oam;
			uint16_t oam_cycle;
			uint16_t dmc_addr;
			uint8_t dmc_delay;
		} dma;
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
		nes->sys.dma.oam_begin = true;

	} else if (addr == 0x4016) {
		nes->sys.io_open_bus = v;
		ctrl_write(&nes->ctrl, v & 0x01);

	} else if (addr < 0x4020) {
		nes->sys.io_open_bus = v;

	} else {
		cart_prg_write(nes->cart, nes->cpu, nes->apu, addr, v);
	}
}


/*** DMA ***/

static void sys_dma_oam(NES *nes, uint8_t v)
{
	// https://forums.nesdev.com/viewtopic.php?f=3&t=6100

	if (!nes->sys.dma.oam_begin)
		return;

	nes->sys.dma.oam_begin = false;
	nes->sys.dma.oam = true;
	cpu_halt(nes->cpu, true);

	sys_cycle(nes); //+1 default case

	if (nes->sys.cycle & 1) //+1 if odd cycle
		sys_cycle(nes);

	//+512 read/write
	for (nes->sys.dma.oam_cycle = 0; nes->sys.dma.oam_cycle < 256; nes->sys.dma.oam_cycle++)
		sys_write_cycle(nes, 0x2014, sys_read_cycle(nes, v * 0x0100 + nes->sys.dma.oam_cycle));

	cpu_halt(nes->cpu, false);
	nes->sys.dma.oam = false;
}

void sys_dma_dmc_begin(NES *nes, uint16_t addr)
{
	if (!nes->sys.in_write) {
		if (nes->sys.read_addr == 0x2007)
			ppu_read(nes->ppu, nes->cpu, nes->cart, 0x2007);

		sys_read(nes, nes->sys.read_addr);
	}

	nes->sys.dma.dmc_begin = true;
	nes->sys.dma.dmc_addr = addr;

	if (nes->sys.dma.oam) {
		if (nes->sys.dma.oam_cycle == 254) { //+0 second-to-second-to-last OAM cycle
			nes->sys.dma.dmc_delay = 0;

		} else if (nes->sys.dma.oam_cycle == 255) { //+2 last OAM cycle
			nes->sys.dma.dmc_delay = 2;

		} else { //+1 otherwise during OAM DMA
			nes->sys.dma.dmc_delay = 1;
		}
	} else if (nes->sys.in_write) { //+2 if CPU is writing
		nes->sys.dma.dmc_delay = 2;

	} else { //+3 default case
		nes->sys.dma.dmc_delay = 3;
	}
}

static void sys_dma_dmc(NES *nes)
{
	if (!nes->sys.dma.dmc_begin)
		return;

	nes->sys.dma.dmc_begin = false;
	cpu_halt(nes->cpu, true);

	for (uint8_t x = 0; x < nes->sys.dma.dmc_delay; x++)
		sys_cycle(nes);

	apu_dma_dmc_finish(nes->apu, sys_read_cycle(nes, nes->sys.dma.dmc_addr));

	cpu_halt(nes->cpu, false);
}


/*** TIMING ***/

uint8_t sys_read_cycle(NES *nes, uint16_t addr)
{
	nes->sys.read_addr = addr;

	ppu_step(nes->ppu, nes->cpu, nes->cart);
	ppu_step(nes->ppu, nes->cpu, nes->cart);

	// Begin concurrent tick
	cpu_phi_1(nes->cpu);
	apu_step(nes->apu, nes, nes->cpu, nes->new_samples, nes->opaque);
	uint8_t v = sys_read(nes, addr);

	ppu_step(nes->ppu, nes->cpu, nes->cart);

	cart_step(nes->cart, nes->cpu);
	cpu_phi_2(nes->cpu);
	// End concurrent tick

	sys_dma_dmc(nes);

	nes->sys.cycle++;

	return v;
}

void sys_write_cycle(NES *nes, uint16_t addr, uint8_t v)
{
	nes->sys.in_write = true;

	// DMC DMA will only engage on a read cycle, double writes will stall longer
	if (nes->sys.dma.dmc_begin)
		nes->sys.dma.dmc_delay++;

	ppu_step(nes->ppu, nes->cpu, nes->cart);
	ppu_step(nes->ppu, nes->cpu, nes->cart);

	// Begin concurrent tick
	cpu_phi_1(nes->cpu);
	apu_step(nes->apu, nes, nes->cpu, nes->new_samples, nes->opaque);

	ppu_step(nes->ppu, nes->cpu, nes->cart);

	sys_write(nes, addr, v);
	cart_step(nes->cart, nes->cpu);
	cpu_phi_2(nes->cpu);
	// End concurrent tick

	sys_dma_oam(nes, v);

	nes->sys.cycle++;
	nes->sys.in_write = false;
}

void sys_cycle(NES *nes)
{
	sys_read_cycle(nes, 0);
}


/*** PUBLIC ***/

void NES_Create(NES_AudioCallback audioCallback, const void *opaque, uint32_t sampleRate, bool stereo, NES **nes)
{
	NES *ctx = *nes = calloc(1, sizeof(NES));

	ctx->opaque = opaque;
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

uint32_t NES_NextFrame(NES *ctx, NES_VideoCallback videoCallback, const void *opaque)
{
	if (!ctx->cart)
		return 0;

	uint64_t cycles = ctx->sys.cycle;

	while (!ppu_new_frame(ctx->ppu))
		cpu_step(ctx->cpu, ctx);

	videoCallback(ppu_pixels(ctx->ppu), (void *) opaque);

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

	ctx->sys.in_write = false;
	ctx->sys.read_addr = 0;
	ctx->sys.cycle = ctx->sys.cycle_2007 = 0;

	if (hard)
		memset(ctx->sys.ram, 0, 0x0800);

	ppu_reset(ctx->ppu);
	apu_reset(ctx->apu, ctx, ctx->cpu, hard);
	cpu_reset(ctx->cpu, ctx, hard);
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

void *NES_GetState(NES *ctx, size_t *size)
{
	if (!ctx->cart)
		return NULL;

	size_t cpu_size = 0;
	void *cpu_state = cpu_get_state(ctx->cpu, &cpu_size);

	size_t apu_size = 0;
	void *apu_state = apu_get_state(ctx->apu, &apu_size);

	size_t ppu_size = 0;
	void *ppu_state = ppu_get_state(ctx->ppu, &ppu_size);

	size_t cart_size = 0;
	void *cart_state = cart_get_state(ctx->cart, &cart_size);

	size_t sys_size = sizeof(struct sys);
	struct sys sys_state = ctx->sys;

	size_t ctrl_size = sizeof(struct ctrl);
	struct ctrl ctrl_state = ctx->ctrl;

	*size = cpu_size + apu_size + ppu_size + cart_size + sys_size + ctrl_size;

	void *state = malloc(*size);
	uint8_t *u8state = state;

	memcpy(u8state, cpu_state, cpu_size);
	u8state += cpu_size;

	memcpy(u8state, apu_state, apu_size);
	u8state += apu_size;

	memcpy(u8state, ppu_state, ppu_size);
	u8state += ppu_size;

	memcpy(u8state, cart_state, cart_size);
	u8state += cart_size;

	memcpy(u8state, &sys_state, sys_size);
	u8state += sys_size;

	memcpy(u8state, &ctrl_state, ctrl_size);
	u8state += ctrl_size;

	free(cpu_state);
	free(apu_state);
	free(ppu_state);
	free(cart_state);

	return state;
}

bool NES_SetState(NES *ctx, const void *state, size_t size)
{
	if (!ctx->cart)
		return false;

	bool r = true;
	const uint8_t *u8state = state;

	size_t current_size = 0;
	void *current = NES_GetState(ctx, &current_size);

	size_t consumed = cpu_set_state(ctx->cpu, u8state, size);
	if (consumed == 0) {r = false; goto except;}
	size -= consumed;
	u8state += consumed;

	consumed = apu_set_state(ctx->apu, u8state, size);
	if (consumed == 0) {r = false; goto except;}
	size -= consumed;
	u8state += consumed;

	consumed = ppu_set_state(ctx->ppu, u8state, size);
	if (consumed == 0) {r = false; goto except;}
	size -= consumed;
	u8state += consumed;

	consumed = cart_set_state(ctx->cart, u8state, size);
	if (consumed == 0) {r = false; goto except;}
	size -= consumed;
	u8state += consumed;

	if (size < sizeof(struct sys)) {r = false; goto except;}
	const struct sys *sys_state = (const struct sys *) u8state;
	ctx->sys = *sys_state;
	size -= sizeof(struct sys);
	u8state += sizeof(struct sys);

	if (size < sizeof(struct ctrl)) {r = false; goto except;}
	const struct ctrl *ctrl_state = (const struct ctrl *) u8state;
	ctx->ctrl = *ctrl_state;
	size -= sizeof(struct ctrl);
	u8state += sizeof(struct ctrl);

	except:

	if (!r)
		NES_SetState(ctx, current, current_size);

	free(current);

	return r;
}
