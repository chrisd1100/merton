#include "cart.h"

#include <stdlib.h>
#include <string.h>


// Mapping

#define PRG_SLOT 0x1000
#define CHR_SLOT 0x0400

#define PRG_SHIFT 12
#define CHR_SHIFT 10

struct memory {
	uint8_t *data;
	size_t size;
};

struct map {
	enum mem type;
	struct memory *mem;
	size_t offset;
	bool mapped;
};

struct asset {
	struct map map[2][16];
	uint16_t mask;
	uint8_t shift;
	struct memory rom;
	struct memory ram;
	struct memory ciram;
	struct memory exram;
	size_t sram;
	size_t wram;
};

static uint8_t map_read(struct asset *asset, uint8_t index, uint16_t addr, bool *hit)
{
	struct map *m = &asset->map[index][addr >> asset->shift];

	if (m->mapped) {
		if (hit)
			*hit = true;

		return m->mem->data[m->offset + (addr & asset->mask)];
	}

	return 0;
}

static void map_write(struct asset *asset, uint8_t index, uint16_t addr, uint8_t v)
{
	struct map *m = &asset->map[index][addr >> asset->shift];

	if (m->mapped && (m->type & RAM))
		m->mem->data[m->offset + (addr & asset->mask)] = v;
}

static void map_unmap(struct asset *asset, uint8_t index, uint16_t addr)
{
	struct map *m = &asset->map[index][addr >> asset->shift];
	m->mapped = false;
}

static struct memory *map_get_mem(struct asset *asset, enum mem type)
{
	return ((type & EXRAM) == EXRAM) ? &asset->exram : ((type & CIRAM) == CIRAM) ?
		&asset->ciram : (type & RAM) ? &asset->ram : &asset->rom;
}

static void cart_map(struct asset *asset, enum mem type, uint16_t addr, uint16_t bank, uint8_t bank_size_kb)
{
	struct memory *mem = map_get_mem(asset, type);

	int32_t start_slot = addr >> asset->shift;
	int32_t bank_size_bytes = bank_size_kb * 0x0400;
	int32_t bank_offset = bank * bank_size_bytes;
	int32_t end_slot = start_slot + (bank_size_bytes >> asset->shift);

	for (int32_t x = start_slot, y = 0; x < end_slot; x++, y++) {
		struct map *m = &asset->map[type & 0x0F][x];

		m->type = type;
		m->mem = mem;
		m->offset = (bank_offset + (y << asset->shift)) % mem->size;
		m->mapped = true;
	}
}

static void cart_map_ciram_offset(struct asset *asset, uint8_t dest, enum mem type, size_t offset)
{
	struct memory *mem = map_get_mem(asset, type);

	asset->map[0][dest + 8].type = type;
	asset->map[0][dest + 8].mem = mem;
	asset->map[0][dest + 8].offset = offset;
	asset->map[0][dest + 8].mapped = true;

	if (dest < 4) {
		asset->map[0][dest + 12].type = type;
		asset->map[0][dest + 12].mem = mem;
		asset->map[0][dest + 12].offset = offset;
		asset->map[0][dest + 12].mapped = true;
	}
}

static void cart_unmap_ciram(struct asset *asset, uint8_t dest)
{
	asset->map[0][dest + 8].mapped = false;

	if (dest < 4)
		asset->map[0][dest + 12].mapped = false;
}

static void cart_map_ciram_slot(struct asset *asset, uint8_t dest, uint8_t src)
{
	cart_map_ciram_offset(asset, dest, CIRAM, src * CHR_SLOT);
}

static void cart_map_ciram(struct asset *asset, NES_Mirror mirror)
{
	for (uint8_t x = 0; x < 8; x++)
		cart_map_ciram_slot(asset, x, (mirror >> (x * 4)) & 0xF);
}

static uint8_t cart_bus_conflict(struct asset *asset, uint16_t addr, uint8_t v)
{
	bool hit = false;
	uint8_t v0 = map_read(asset, 0, addr, &hit);

	if (hit)
		return v & v0;

	return v;
}


// Cart

#define KB(b) ((b) / 0x0400)

struct cart {
	NES_CartDesc hdr;
	struct asset prg;
	struct asset chr;
	size_t dynamic_size;
	void *mem;

	size_t sram_dirty;
	uint64_t read_counter;
	uint64_t cycle;

	bool ram_enable;
	uint8_t prg_mode;
	uint8_t chr_mode;
	uint8_t REG[8];
	uint8_t PRG[8];
	uint8_t CHR[16];

	struct {
		bool enable;
		bool reload;
		bool pending;
		uint16_t counter;
		uint16_t value;
		uint8_t period;
		int16_t scanline;
		bool cycle;
		bool ack;
	} irq;

	struct {
		bool use256;
		uint64_t cycle;
		uint8_t n;
	} mmc1;

	struct {
		uint8_t bank_update;
	} mmc3;

	struct {
		uint8_t exram_mode;
		uint8_t fill_tile;
		uint8_t fill_attr;
		uint8_t exram1;
		uint8_t ram_banks;
		uint16_t multiplicand;
		uint16_t multiplier;
		uint16_t chr_bank_upper;
		uint16_t scanline;
		uint64_t last_ppu_read;
		enum mem active_map;
		bool nt_latch;
		bool exram_latch;
		bool large_sprites;
		bool in_frame;

		struct {
			bool enable;
			bool right;
			bool fetch;
			uint16_t htile;
			uint16_t scroll;
			uint8_t scroll_reload;
			uint8_t tile;
			uint8_t bank;
		} vs;
	} mmc5;

	struct {
		bool is2;
		uint16_t type;
	} vrc;
};

#include "mapper/fcg.c"
#include "mapper/fme7.c"
#include "mapper/jaleco.c"
#include "mapper/mapper.c"
#include "mapper/mmc1.c"
#include "mapper/mmc2.c"
#include "mapper/mmc3.c"
#include "mapper/mmc5.c"
#include "mapper/namco.c"
#include "mapper/vrc.c"
#include "mapper/vrc6.c"
#include "mapper/vrc7.c"


// IO

uint8_t cart_prg_read(struct cart *cart, struct apu *apu, uint16_t addr, bool *mem_hit)
{
	switch (cart->hdr.mapper) {
		case 5:  return mmc5_prg_read(cart, apu, addr, mem_hit);
		case 19: return namco_prg_read(cart, addr, mem_hit);
		default:
			return map_read(&cart->prg, 0, addr, mem_hit);
	}
}

void cart_prg_write(struct cart *cart, struct apu *apu, uint16_t addr, uint8_t v)
{
	switch (cart->hdr.mapper) {
		case 1:   mmc1_prg_write(cart, addr, v);       break;
		case 4:
		case 206: mmc3_prg_write(cart, addr, v);       break;
		case 5:   mmc5_prg_write(cart, apu, addr, v);  break;
		case 9:   mmc2_prg_write(cart, addr, v);       break;
		case 10:  mmc2_prg_write(cart, addr, v);       break;
		case 18:  jaleco_prg_write(cart, addr, v);     break;
		case 210:
		case 19:  namco_prg_write(cart, addr, v);      break;
		case 21:  vrc_prg_write(cart, addr, v);        break;
		case 22:  vrc_prg_write(cart, addr, v);        break;
		case 24:  vrc6_prg_write(cart, apu, addr, v);  break;
		case 26:  vrc6_prg_write(cart, apu, addr, v);  break;
		case 23:  vrc_prg_write(cart, addr, v);        break;
		case 25:  vrc_prg_write(cart, addr, v);        break;
		case 69:  fme7_prg_write(cart, apu, addr, v);  break;
		case 85:  vrc7_prg_write(cart, addr, v);       break;
		case 16:
		case 159: fcg_prg_write(cart, addr, v);        break;
		case 0:
		case 2:
		case 3:
		case 7:
		case 11:
		case 13:
		case 30:
		case 31:
		case 34:
		case 38:
		case 66:
		case 70:
		case 71:
		case 77:
		case 78:
		case 79:
		case 87:
		case 89:
		case 93:
		case 94:
		case 97:
		case 101:
		case 107:
		case 111:
		case 113:
		case 140:
		case 145:
		case 146:
		case 148:
		case 149:
		case 152:
		case 180:
		case 184:
		case 185: mapper_prg_write(cart, addr, v);     break;
	}
}

uint8_t cart_chr_read(struct cart *cart, uint16_t addr, enum mem type, bool nt)
{
	if (addr < 0x2000) {
		switch (cart->hdr.mapper) {
			case 5:  return mmc5_chr_read(cart, addr, type);
			case 9:
			case 10: return mmc2_chr_read(cart, addr);
		}

	} else {
		switch (cart->hdr.mapper) {
			case 5: return mmc5_nt_read_hook(cart, addr, type, nt);
		}
	}

	return map_read(&cart->chr, 0, addr, NULL);
}

void cart_chr_write(struct cart *cart, uint16_t addr, uint8_t v)
{
	map_write(&cart->chr, 0, addr, v);
}


// Hooks

void cart_ppu_a12_toggle(struct cart *cart)
{
	switch (cart->hdr.mapper) {
		case 4: mmc3_ppu_a12_toggle(cart);
	}
}

void cart_ppu_write_hook(struct cart *cart, uint16_t addr, uint8_t v)
{
	switch (cart->hdr.mapper) {
		case 5: mmc5_ppu_write_hook(cart, addr, v); break;
	}
}

bool cart_block_2007(struct cart *cart)
{
	switch (cart->hdr.mapper) {
		case 185: return mapper_block_2007(cart);
	}

	return false;
}


// SRAM

size_t cart_sram_dirty(struct cart *cart)
{
	if (!cart->hdr.battery) return 0;

	size_t dirty_size = cart->sram_dirty;
	cart->sram_dirty = 0;

	return dirty_size;
}

void cart_sram_get(struct cart *cart, void *buf, size_t size)
{
	memcpy(buf, cart->prg.ram.data, size);
	cart->sram_dirty = 0;
}


// Step

void cart_step(struct cart *cart, struct cpu *cpu)
{
	switch (cart->hdr.mapper) {
		case 4: mmc3_step(cart, cpu);    break;
		case 5: mmc5_step(cart, cpu);    break;
		case 18: jaleco_step(cart, cpu); break;
		case 19: namco_step(cart, cpu);  break;
		case 21: vrc_step(cart, cpu);    break;
		case 23: vrc_step(cart, cpu);    break;
		case 24: vrc_step(cart, cpu);    break;
		case 26: vrc_step(cart, cpu);    break;
		case 25: vrc_step(cart, cpu);    break;
		case 69: fme7_step(cart, cpu);   break;
		case 85: vrc_step(cart, cpu);    break;
		case 16:
		case 159: fcg_step(cart, cpu);   break;
	}

	cart->cycle++;
}


// Lifecycle

static bool cart_parse_header(const uint8_t *rom, NES_CartDesc *hdr)
{
	if (rom[0] == 'U' && rom[1] == 'N' && rom[2] == 'I' && rom[3] == 'F') {
		NES_Log("UNIF format unsupported");
		return false;
	}

	if (!(rom[0] == 'N' && rom[1] == 'E' && rom[2] == 'S' && rom[3] == 0x1A)) {
		NES_Log("Bad iNES header");
		return false;
	}

	// Archaic iNES
	hdr->offset = 16;
	hdr->prgROMSize = rom[4] * 0x4000;
	hdr->chrROMSize = rom[5] * 0x2000;
	hdr->mirror = (rom[6] & 0x08) ? NES_MIRROR_FOUR : (rom[6] & 0x01) ? NES_MIRROR_VERTICAL : NES_MIRROR_HORIZONTAL;
	hdr->battery = rom[6] & 0x02;
	hdr->offset += (rom[6] & 0x04) ? 512: 0; // Trainer
	hdr->mapper = rom[6] >> 4;

	// Modern iNES
	if ((rom[7] & 0x0C) == 0 && rom[12] == 0 && rom[13] == 0 && rom[14] == 0 && rom[15] == 0) {
		hdr->mapper |= rom[7] & 0xF0;

	// NES 2.0
	} else if (((rom[7] & 0x0C) >> 2) == 0x02) {
		hdr->mapper |= rom[7] & 0xF0;
		hdr->mapper |= (rom[8] & 0x0F) << 8;
		hdr->submapper = rom[8] >> 4;

		uint8_t volatile_shift = rom[10] & 0x0F;
		hdr->prgWRAMSize = volatile_shift ? 64 << volatile_shift : 0;

		uint8_t non_volatile_shift = (rom[10] & 0xF0) >> 4;
		hdr->prgSRAMSize = non_volatile_shift ? 64 << non_volatile_shift : 0;

		volatile_shift = rom[11] & 0x0F;
		hdr->chrWRAMSize = volatile_shift ? 64 << volatile_shift : 0;

		non_volatile_shift = (rom[11] & 0xF0) >> 4;
		hdr->chrSRAMSize = non_volatile_shift ? 64 << non_volatile_shift : 0;
	}

	return true;
}

static void cart_log_desc(NES_CartDesc *hdr, bool log_ram_sizes)
{
	NES_Log("PRG ROM Size: %uKB", KB(hdr->prgROMSize));
	NES_Log("CHR ROM Size: %uKB", KB(hdr->chrROMSize));

	if (log_ram_sizes) {
		NES_Log("PRG RAM V / NV: %uKB / %uKB", KB(hdr->prgWRAMSize), KB(hdr->prgSRAMSize));
		NES_Log("CHR RAM V / NV: %uKB / %uKB", KB(hdr->chrWRAMSize), KB(hdr->chrSRAMSize));
	}

	NES_Log("Mapper: %u", hdr->mapper);

	if (hdr->submapper != 0)
		NES_Log("Submapper: %x", hdr->submapper);

	NES_Log("Mirroring: %s", hdr->mirror == NES_MIRROR_VERTICAL ? "Vertical" :
		hdr->mirror == NES_MIRROR_HORIZONTAL ? "Horizontal" : "Four Screen");

	NES_Log("Battery: %s", hdr->battery ? "true" : "false");
}

static void cart_set_data_pointers(struct cart *cart)
{
	uint8_t *ptr = cart->mem;

	cart->prg.rom.data = ptr;
	ptr += cart->prg.rom.size;

	cart->prg.ram.data = ptr;
	ptr += cart->prg.ram.size;

	cart->chr.rom.data = ptr;
	ptr += cart->chr.rom.size;

	cart->chr.ram.data = ptr;
	ptr += cart->chr.ram.size;

	cart->chr.ciram.data = ptr;
	ptr += cart->chr.ciram.size;

	cart->chr.exram.data = ptr;
	ptr += cart->chr.exram.size;

	for (uint8_t x = 0; x < 2; x++) {
		for (uint8_t y = 0; y < 16; y++) {
			cart->prg.map[x][y].mem = map_get_mem(&cart->prg, cart->prg.map[x][y].type);
			cart->chr.map[x][y].mem = map_get_mem(&cart->chr, cart->chr.map[x][y].type);
		}
	}
}

static bool cart_init_mapper(struct cart *ctx)
{
	cart_map(&ctx->prg, ROM, 0x8000, 0, 32);
	cart_map_ciram(&ctx->chr, ctx->hdr.mirror);
	cart_map(&ctx->chr, ctx->chr.rom.size > 0 ? ROM : RAM, 0x0000, 0, 8);

	switch (ctx->hdr.mapper) {
		case 1:   mmc1_create(ctx);    break;
		case 4:
		case 206: mmc3_create(ctx);    break;
		case 5:   mmc5_create(ctx);    break;
		case 9:   mmc2_create(ctx);    break;
		case 10:  mmc2_create(ctx);    break;
		case 18:  jaleco_create(ctx);  break;
		case 210:
		case 19:  namco_create(ctx);   break;
		case 21:  vrc2_4_create(ctx);  break;
		case 22:  vrc2_4_create(ctx);  break;
		case 24:  vrc_create(ctx);     break;
		case 26:  vrc_create(ctx);     break;
		case 23:  vrc2_4_create(ctx);  break;
		case 25:  vrc2_4_create(ctx);  break;
		case 69:  fme7_create(ctx);    break;
		case 85:  vrc_create(ctx);     break;
		case 16:
		case 159: fcg_create(ctx);     break;
		case 0:
		case 2:
		case 3:
		case 7:
		case 11:
		case 13:
		case 30:
		case 31:
		case 34:
		case 38:
		case 66:
		case 70:
		case 71:
		case 77:
		case 78:
		case 79:
		case 87:
		case 89:
		case 93:
		case 94:
		case 97:
		case 101:
		case 107:
		case 111:
		case 113:
		case 140:
		case 145:
		case 146:
		case 148:
		case 149:
		case 152:
		case 180:
		case 184:
		case 185: mapper_create(ctx);  break;

		default:
			NES_Log("Mapper %u is unsupported", ctx->hdr.mapper);
			return false;
	}

	return true;
}

void cart_create(const void *rom, size_t rom_size, const void *sram, size_t sram_size,
	const NES_CartDesc *desc, struct cart **cart)
{
	bool r = true;

	struct cart *ctx = *cart = calloc(1, sizeof(struct cart));
	ctx->prg.mask = PRG_SLOT - 1;
	ctx->chr.mask = CHR_SLOT - 1;
	ctx->prg.shift = PRG_SHIFT;
	ctx->chr.shift = CHR_SHIFT;

	if (desc) {
		ctx->hdr = *desc;

	} else {
		if (rom_size < 16) {
			r = false;
			NES_Log("ROM is less than 16 bytes");
			goto except;
		}

		r = cart_parse_header(rom, &ctx->hdr);
		if (!r)
			goto except;
	}

	ctx->prg.rom.size = ctx->hdr.prgROMSize;
	ctx->chr.rom.size = ctx->hdr.chrROMSize;
	ctx->chr.ciram.size = 0x4000;
	ctx->chr.exram.size = 0x400; // MMC5 exram lives with chr since it can be mapped to CIRAM

	ctx->prg.wram = ctx->hdr.prgWRAMSize;
	ctx->prg.sram = ctx->hdr.prgSRAMSize;
	ctx->chr.wram = ctx->hdr.chrWRAMSize;
	ctx->chr.sram = ctx->hdr.chrSRAMSize;

	cart_log_desc(&ctx->hdr, ctx->prg.wram > 0 || ctx->prg.sram > 0 ||
		ctx->chr.wram > 0 || ctx->chr.sram > 0);

	// Defaults to be safe with poor iNES headers
	if (ctx->prg.sram == 0)
		ctx->prg.sram = 0x2000;

	if (ctx->prg.wram == 0)
		ctx->prg.wram = 0x1E000;

	if (ctx->chr.wram == 0)
		ctx->chr.wram = 0x8000;

	ctx->prg.ram.size = ctx->prg.wram + ctx->prg.sram;
	ctx->chr.ram.size = ctx->chr.wram + ctx->chr.sram;

	if (ctx->hdr.offset + ctx->prg.rom.size > rom_size) {
		r = false;
		NES_Log("PRG ROM size is incorrect");
		goto except;
	}

	if (ctx->hdr.offset + ctx->prg.rom.size + ctx->chr.rom.size > rom_size) {
		r = false;
		NES_Log("CHR ROM size is incorrect");
		goto except;
	}

	ctx->dynamic_size = ctx->prg.rom.size + ctx->prg.ram.size +
		ctx->chr.rom.size + ctx->chr.ram.size + ctx->chr.ciram.size + ctx->chr.exram.size;
	ctx->mem = calloc(ctx->dynamic_size, 1);

	cart_set_data_pointers(ctx);

	memcpy(ctx->prg.rom.data, (uint8_t *) rom + ctx->hdr.offset, ctx->prg.rom.size);
	memcpy(ctx->chr.rom.data, (uint8_t *) rom + ctx->hdr.offset + ctx->prg.rom.size, ctx->chr.rom.size);

	if (sram && sram_size > 0 && sram_size <= ctx->prg.sram)
		memcpy(ctx->prg.ram.data, sram, sram_size);

	r = cart_init_mapper(ctx);
	if (!r)
		goto except;

	except:

	if (!r)
		cart_destroy(cart);
}

void cart_destroy(struct cart **cart)
{
	if (!cart || !*cart)
		return;

	struct cart *ctx = *cart;

	free(ctx->mem);

	free(ctx);
	*cart = NULL;
}

void cart_reset(struct cart *cart)
{
	cart->sram_dirty = 0;
	cart->read_counter = 0;
	cart->cycle = 0;
	cart->ram_enable = 0;
	cart->prg_mode = 0;
	cart->chr_mode = 0;

	memset(cart->REG, 0, sizeof(cart->REG));
	memset(cart->PRG, 0, sizeof(cart->PRG));
	memset(cart->CHR, 0, sizeof(cart->CHR));
	memset(&cart->irq, 0, sizeof(cart->irq));
	memset(&cart->mmc1, 0, sizeof(cart->mmc1));
	memset(&cart->mmc3, 0, sizeof(cart->mmc3));
	memset(&cart->mmc5, 0, sizeof(cart->mmc5));
	memset(&cart->vrc, 0, sizeof(cart->vrc));

	memset(cart->chr.ram.data, 0, cart->chr.ram.size);
	memset(cart->chr.ciram.data, 0, cart->chr.ciram.size);
	memset(cart->chr.exram.data, 0, cart->chr.exram.size);
	memset(cart->prg.ram.data, 0, cart->prg.ram.size);

	cart_init_mapper(cart);
}

size_t cart_set_state(struct cart *cart, const void *state, size_t size)
{
	if (size >= sizeof(struct cart)) {
		struct cart *new_cart = (struct cart *) state;

		if (size >= sizeof(struct cart) + new_cart->dynamic_size ) {
			free(cart->mem);
			*cart = *new_cart;

			cart->mem = calloc(cart->dynamic_size, 1);
			memcpy(cart->mem, (uint8_t *) state + sizeof(struct cart), cart->dynamic_size);

			cart_set_data_pointers(cart);

			return sizeof(struct cart) + cart->dynamic_size;
		}
	}

	return 0;
}

void *cart_get_state(struct cart *cart, size_t *size)
{
	*size = sizeof(struct cart) + cart->dynamic_size;

	struct cart *state = malloc(*size);
	*state = *cart;

	memcpy((uint8_t *) state + sizeof(struct cart), cart->mem, cart->dynamic_size);

	return state;
}
