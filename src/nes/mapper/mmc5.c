// Copyright (c) 2019-2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// https://wiki.nesdev.com/w/index.php/MMC5

static void mmc5_map_prg16(struct cart *cart, enum mem type, uint16_t addr, uint16_t bank)
{
	cart_map(&cart->prg, type, addr, bank & 0xFE, 8);
	cart_map(&cart->prg, type, addr + 0x2000, (bank & 0xFE) + 1, 8);
}

static void mmc5_map_prg32(struct cart *cart, enum mem type, uint16_t addr, uint16_t bank)
{
	cart_map(&cart->prg, type, addr, bank & 0xFC, 8);
	cart_map(&cart->prg, type, addr + 0x2000, (bank & 0xFC) + 1, 8);
	cart_map(&cart->prg, type, addr + 0x4000, (bank & 0xFC) + 2, 8);
	cart_map(&cart->prg, type, addr + 0x6000, (bank & 0xFC) + 3, 8);
}

static void mmc5_map_prg(struct cart *cart, int32_t slot, uint16_t bank, enum mem type)
{
	if (slot == 0)
		type = RAM;

	if (type == RAM)
		bank = (cart->mmc5.ram_banks > 1 ? (bank & 0x3) : 0) + ((bank & 0x4) >> 2) * cart->mmc5.ram_banks;

	if (slot == 0) {
		cart_map(&cart->prg, RAM, 0x6000, bank, 8);

	} else {
		switch (cart->prg_mode) {
			case 0:
				if (slot == 4)
					mmc5_map_prg32(cart, type, 0x8000, bank);
				break;
			case 1:
				if (slot == 2) {
					mmc5_map_prg16(cart, type, 0x8000, bank);
				} else if (slot == 4) {
					mmc5_map_prg16(cart, type, 0xC000, bank);
				}
				break;
			case 2:
				if (slot == 2) {
					mmc5_map_prg16(cart, type, 0x8000, bank);

				} else if (slot > 2) {
					cart_map(&cart->prg, type, 0x6000 + (uint16_t) (slot * 0x2000), bank, 8);
				}
				break;
			case 3:
				cart_map(&cart->prg, type, 0x6000 + (uint16_t) (slot * 0x2000), bank, 8);
				break;
		}
	}
}

static void mmc5_map_chr(struct cart *cart, int32_t slot, uint16_t bank, enum mem type)
{
	bank |= cart->mmc5.chr_bank_upper;
	enum mem ram = cart->chr.rom.size == 0 ? RAM : UNMAPPED;

	switch (cart->chr_mode) {
		case 0:
			cart_map(&cart->chr, type | ram, 0x0000, bank, 8);
			break;
		case 1:
			cart_map(&cart->chr, type | ram, slot == 3 ? 0x0000 : 0x1000, bank, 4);
			break;
		case 3:
			cart_map(&cart->chr, type | ram, (uint16_t) (slot * 0x0400), bank, 1);
			break;
		default:
			NES_Log("Unsupported CHR mode %x", cart->chr_mode);
	}
}

static void mmc5_create(struct cart *cart)
{
	mmc5_map_prg16(cart, ROM, 0xC000, 0xFF);

	cart->prg_mode = 3;
	cart->mmc5.active_map = SPRROM;

	enum mem ram = cart->chr.rom.size == 0 ? RAM : UNMAPPED;
	cart_map(&cart->chr, SPRROM | ram, 0x0000, 0, 8);
	cart_map(&cart->chr, BGROM | ram, 0x0000, 0, 8);

	if (cart->prg.ram.size > 0)
		cart_map(&cart->prg, RAM, 0x6000, 0, 8);

	cart->mmc5.ram_banks = cart->prg.ram.size <= 0x4000 ? 1 : 4;
}

static void mmc5_prg_write(struct cart *cart, struct apu *apu, uint16_t addr, uint8_t v)
{
	if (addr >= 0x5C00 && addr < 0x6000) {
		cart->chr.exram.data[addr - 0x5C00] = v;

	} else if (addr < 0x6000) {
		switch (addr) {
			case 0x5000: // MMC5 audio pulse, status
			case 0x5002:
			case 0x5003:
			case 0x5004:
			case 0x5006:
			case 0x5007:
			case 0x5015:
				apu_write(apu, NULL, addr - 0x1000, v, EXT_MMC5);
				break;
			case 0x5001: // MMC5 audio unused pulse sweep
			case 0x5005:
				break;
			case 0x5010: // MMC5 audio PCM
			case 0x5011:
				break;
			case 0x5100: // PRG mode
				cart->prg_mode = v & 0x03;
				break;
			case 0x5101: // CHR mode
				cart->chr_mode = v & 0x03;
				break;
			case 0x5102: // PRG RAM protect
			case 0x5103:
				break;
			case 0x5104: // EXRAM mode
				cart->mmc5.exram_mode = v & 0x03;
				break;
			case 0x5105: // Mirroring mode
				for (uint8_t x = 0; x < 4; x++) {
					switch ((v >> (x * 2)) & 0x03) {
						case 0: cart_map_ciram_slot(&cart->chr, x, 0);          break;
						case 1: cart_map_ciram_slot(&cart->chr, x, 1);          break;
						case 2: cart_map_ciram_offset(&cart->chr, x, EXRAM, 0); break;
						case 3: cart_unmap_ciram(&cart->chr, x);                break;
					}
				}
				break;
			case 0x5106: // Fill mode tile
				cart->mmc5.fill_tile = v;
				break;
			case 0x5107: // Fill mode color
				cart->mmc5.fill_attr = v & 0x03;
				cart->mmc5.fill_attr |= cart->mmc5.fill_attr << 2;
				cart->mmc5.fill_attr |= cart->mmc5.fill_attr << 4;
				break;
			case 0x5113: // PRG bankswitch
			case 0x5114:
			case 0x5115:
			case 0x5116:
			case 0x5117: {
				bool ram = !(v & 0x80) && (addr == 0x5114 || addr == 0x5115 || addr == 0x5116);
				mmc5_map_prg(cart, addr - 0x5113, v & 0x7F, ram ? RAM : ROM);
				break;
			}
			case 0x5120: // CHR bankswitch
			case 0x5121:
			case 0x5122:
			case 0x5123:
			case 0x5124:
			case 0x5125:
			case 0x5126:
			case 0x5127:
				cart->mmc5.active_map = SPRROM;
				mmc5_map_chr(cart, addr - 0x5120, v, SPRROM);
				break;
			case 0x5128:
			case 0x5129:
			case 0x512A:
			case 0x512B:
				cart->mmc5.active_map = BGROM;
				mmc5_map_chr(cart, addr - 0x5128, v, BGROM);
				mmc5_map_chr(cart, (addr - 0x5128) + 4, v, BGROM);
				break;
			case 0x5130:
				cart->mmc5.chr_bank_upper = (uint16_t) (v & 0x03) << 8;
				break;
			case 0x5200: // Vertical split mode
				cart->mmc5.vs.enable = v & 0x80;
				cart->mmc5.vs.right = v & 0x40;
				cart->mmc5.vs.tile = v & 0x1F;
				break;
			case 0x5201:
				cart->mmc5.vs.scroll_reload = v;
				break;
			case 0x5202:
				cart->mmc5.vs.bank = v;
				break;
			case 0x5203: // IRQ line number
				cart->irq.scanline = v;
				break;
			case 0x5204: // IRQ enable
				cart->irq.enable = v & 0x80;
				break;
			case 0x5205: // Math
				cart->mmc5.multiplicand = v;
				break;
			case 0x5206:
				cart->mmc5.multiplier = v;
				break;
			case 0x5800: // Just Breed unknown
				break;
			default:
				NES_Log("Uncaught MMC5 write %x", addr);
		}

	} else {
		map_write(&cart->prg, 0, addr, v);
		cart->sram_dirty = cart->prg.sram;
	}
}

static uint8_t mmc5_prg_read(struct cart *cart, struct apu *apu, uint16_t addr, bool *mem_hit)
{
	*mem_hit = true;

	if (addr >= 0x6000) {
		return map_read(&cart->prg, 0, addr, mem_hit);

	} else if (addr >= 0x5C00 && addr < 0x6000) {
		return cart->chr.exram.data[addr - 0x5C00];

	} else {
		switch (addr) {
			case 0x5000: // MMC5 audio pulse
			case 0x5002:
			case 0x5003:
			case 0x5004:
			case 0x5006:
			case 0x5007:
				break;
			case 0x5001: // MMC5 audio unused pulse sweep
			case 0x5005:
				break;
			case 0x5015: // MMC5 audio status
				return apu_read_status(apu, EXT_MMC5);
			case 0x5010: // MMC5 audio PCM
			case 0x5011:
				break;
			case 0x5113: // PRG bankswitch
			case 0x5114:
			case 0x5115:
			case 0x5116:
			case 0x5117:
			case 0x5120: // CHR bankswitch
			case 0x5121:
			case 0x5122:
			case 0x5123:
			case 0x5124:
			case 0x5125:
			case 0x5126:
			case 0x5127:
			case 0x5128:
			case 0x5129:
			case 0x512A:
			case 0x512B:
				break;
			case 0x5204: {
				uint8_t r = 0;

				if (cart->mmc5.in_frame) r |= 0x40;
				if (cart->irq.pending) r |= 0x80;

				cart->irq.pending = false;

				return r;
			}
			case 0x5205:
				return (uint8_t) (cart->mmc5.multiplier * cart->mmc5.multiplicand);
			case 0x5206:
				return (cart->mmc5.multiplier * cart->mmc5.multiplicand) >> 8;
			default:
				NES_Log("Uncaught MMC5 read %x", addr);
				break;
		}
	}

	*mem_hit = false;

	return 0;
}

static void mmc5_scanline(struct cart *cart, uint16_t addr)
{
	// Should be true on the first attribute byte fetch of the scanline (cycle 3)
	if (cart->irq.counter == 2) {
		if (!cart->mmc5.in_frame) {
			cart->mmc5.in_frame = true;
			cart->mmc5.scanline = 0;
		} else {
			cart->mmc5.scanline++;
		}

		cart->irq.pending = cart->irq.scanline == cart->mmc5.scanline && cart->irq.scanline != 0;

		cart->mmc5.vs.scroll++;

		if (cart->mmc5.scanline == 0)
			cart->mmc5.vs.scroll = cart->mmc5.vs.scroll_reload;

		cart->irq.counter = 0;
		cart->irq.value = 0xFFFF;
	}

	if (addr == cart->irq.value)
		cart->irq.counter++;

	cart->irq.value = addr;
}

static uint8_t mmc5_nt_read_hook(struct cart *cart, uint16_t addr, enum mem type, bool nt)
{
	cart->mmc5.last_ppu_read = 0;

	mmc5_scanline(cart, addr);

	if (type == BGROM) {
		if (nt) {
			cart->mmc5.exram_latch = false;
			cart->mmc5.nt_latch = false;
			cart->mmc5.vs.htile++;

			if (cart->mmc5.vs.htile > 34)
				cart->mmc5.vs.htile = 1;
		}

		uint16_t htile = cart->mmc5.vs.htile >= 32 ? cart->mmc5.vs.htile - 32 : cart->mmc5.vs.htile + 1;
		bool vs_in_range = cart->mmc5.vs.right ? htile >= cart->mmc5.vs.tile : htile < cart->mmc5.vs.tile;

		cart->mmc5.vs.fetch =
			vs_in_range &&
			cart->mmc5.vs.enable &&
			cart->mmc5.exram_mode <= 1;

		if (cart->mmc5.vs.fetch) {
			uint16_t vtile = cart->mmc5.vs.scroll / 8;

			if (vtile >= 30)
				vtile -= 30;

			if (!cart->mmc5.exram_latch) {
				cart->mmc5.exram_latch = true;
				return cart->chr.exram.data[vtile * 32 + htile];

			} else {
				cart->mmc5.exram_latch = false;
				return cart->chr.exram.data[0x03C0 + vtile / 32 + htile / 4];
			}

		} else if (cart->mmc5.exram_mode == 1) {
			if (!cart->mmc5.exram_latch) {
				cart->mmc5.exram_latch = true;
				cart->mmc5.exram1 = cart->chr.exram.data[addr % 0x0400];

			} else {
				cart->mmc5.exram_latch = false;

				uint8_t exattr = (cart->mmc5.exram1 & 0xC0) >> 6;
				exattr |= exattr << 2;
				exattr |= exattr << 4;

				return exattr;
			}
		}
	}

	bool hit = false;
	uint8_t v = map_read(&cart->chr, 0, addr, &hit);

	if (!hit) { // Unmapped falls through to fill mode
		v = !cart->mmc5.nt_latch ? cart->mmc5.fill_tile : cart->mmc5.fill_attr;
		cart->mmc5.nt_latch = true;
	}

	return v;
}

static void mmc5_ppu_write_hook(struct cart *cart, uint16_t addr, uint8_t v)
{
	switch (addr) {
		case 0x2000: // PPUCTRL
			cart->mmc5.large_sprites = v & 0x20;
			break;
	}
}

static uint8_t mmc5_chr_read(struct cart *cart, uint16_t addr, enum mem type)
{
	cart->mmc5.last_ppu_read = 0;

	if (cart->mmc5.exram_mode != 1 && !cart->mmc5.large_sprites)
		type = SPRROM;

	switch (type) {
		case BGROM:
			if (cart->mmc5.vs.fetch) {
				uint16_t fine_y = cart->mmc5.vs.scroll & 0x07;
				return cart->chr.rom.data[(cart->mmc5.vs.bank * 0x1000 + (addr & 0x0FF8) + fine_y) % cart->chr.rom.size];

			} else if (cart->mmc5.exram_mode == 1) {
				uint16_t exbank = (cart->mmc5.chr_bank_upper >> 2) | (cart->mmc5.exram1 & 0x3F);
				return cart->chr.rom.data[(exbank * 0x1000 + (addr & 0x0FFF)) % cart->chr.rom.size];
			}

			return map_read(&cart->chr, BGROM & 0xF, addr, NULL);
		case SPRROM:
			return map_read(&cart->chr, SPRROM & 0xF, addr, NULL);
		case ROM:
			return map_read(&cart->chr, cart->mmc5.active_map & 0xF, addr, NULL);
		default:
			break;
	}

	return 0;
}

static void mmc5_step(struct cart *cart, struct cpu *cpu)
{
	if (++cart->mmc5.last_ppu_read >= 3)
		cart->mmc5.in_frame = false;

	cpu_irq(cpu, IRQ_MAPPER, cart->irq.pending && cart->irq.enable && cart->mmc5.scanline != 0);
}
