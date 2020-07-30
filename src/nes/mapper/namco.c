// https://wiki.nesdev.com/w/index.php/INES_Mapper_019
// https://wiki.nesdev.com/w/index.php/INES_Mapper_210

static void namco_create(struct cart *cart)
{
	uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x2000) - 1;

	cart_map(&cart->prg, ROM, 0xE000, last_bank, 8);

	if (cart->prg.ram.size > 0) {
		if (cart->hdr.mapper == 210 && cart->hdr.submapper == 1) {
			cart_map(&cart->prg, RAM, 0x6000, 0, 2);
			cart_map(&cart->prg, RAM, 0x6800, 0, 2);
			cart_map(&cart->prg, RAM, 0x7000, 0, 2);
			cart_map(&cart->prg, RAM, 0x7800, 0, 2);

		} else if (cart->hdr.mapper == 19) {
			cart->ram_enable = true;
			cart_map(&cart->prg, RAM, 0x6000, 0, 8);
		}
	}
}

static void namco_map_ppu(struct cart *cart)
{
	for (uint8_t x = 0; x < 4; x++) {
		enum mem type = (cart->CHR[x] >= 0xE0 && !(cart->chr_mode & 0x01)) ? CIRAM : ROM;
		uint8_t v = type == CIRAM ? (cart->CHR[x] & 0x01) : cart->CHR[x];
		cart_map(&cart->chr, type, x * 0x0400, v, 1);

		type = (cart->CHR[x + 4] >= 0xE0 && !(cart->chr_mode & 0x02)) ? CIRAM : ROM;
		v = type == CIRAM ? (cart->CHR[x + 4] & 0x01) : cart->CHR[x + 4];
		cart_map(&cart->chr, type, 0x1000 + x * 0x0400, v, 1);

		if (cart->REG[x] >= 0xE0) {
			cart_map_ciram_slot(&cart->chr, x, cart->REG[x] & 0x01);
		} else {
			cart_map_ciram_offset(&cart->chr, x, ROM, 0x400 * cart->REG[x]);
		}
	}
}

static void namco_prg_write(struct cart *cart, uint16_t addr, uint8_t v)
{
	if (cart->hdr.mapper == 210 && addr < 0x8000)
		return;

	if (addr >= 0x6000 && addr < 0x8000 && cart->ram_enable) {
		map_write(&cart->prg, 0, addr, v);

	} else if (addr >= 0x4800) {
		switch (addr & 0xF800) {
			case 0x4800: // Expansion audio
				break;
			case 0x5000: // IRQ
				cart->irq.counter = (cart->irq.counter & 0xFF00) | v;
				cart->irq.ack = true;
				break;
			case 0x5800:
				cart->irq.enable = v & 0x80;
				cart->irq.counter = (cart->irq.counter & 0x00FF) | (((uint16_t) v & 0x7F) << 8);
				cart->irq.ack = true;
				break;
			case 0x8000: // CHR
			case 0x8800:
			case 0x9000:
			case 0x9800:
			case 0xA000:
			case 0xA800:
			case 0xB000:
			case 0xB800:
				if (cart->hdr.mapper == 210) {
					cart_map(&cart->chr, ROM, ((addr - 0x8000) / 0x800) * 0x400, v, 1);

				} else {
					cart->CHR[(addr - 0x8000) / 0x800] = v;
					namco_map_ppu(cart);
				}
				break;
			case 0xC000: // Nametables (mapper 19), RAM enable (mapper 210.1)
				if (cart->hdr.mapper == 210 && cart->hdr.submapper == 1)
					cart->ram_enable = v & 0x1;
				break;
			case 0xC800:
			case 0xD000:
			case 0xD800:
				if (cart->hdr.mapper == 19) {
					cart->REG[(addr - 0xC000) / 0x800] = v;
					namco_map_ppu(cart);
				}
				break;
			case 0xE000: //PRG, mirroring (mapper 210.2)
				if (cart->hdr.mapper == 210 && cart->hdr.submapper == 2) {
					switch ((v & 0xC0) >> 6) {
						case 0: cart_map_ciram(&cart->chr, NES_MIRROR_SINGLE0);    break;
						case 1: cart_map_ciram(&cart->chr, NES_MIRROR_VERTICAL);   break;
						case 2: cart_map_ciram(&cart->chr, NES_MIRROR_HORIZONTAL); break;
						case 3: cart_map_ciram(&cart->chr, NES_MIRROR_SINGLE1);    break;
					}
				}

				cart_map(&cart->prg, ROM, 0x8000, v & 0x3F, 8);
				break;
			case 0xE800:
				cart_map(&cart->prg, ROM, 0xA000, v & 0x3F, 8);

				if (cart->hdr.mapper == 19) {
					cart->chr_mode = (v & 0xC0) >> 6;
					namco_map_ppu(cart);
				}
				break;
			case 0xF000:
				cart_map(&cart->prg, ROM, 0xC000, v & 0x3F, 8);
				break;
			case 0xF800: // Expansion audio etc.
				break;
			default:
				NES_Log("Uncaught Namco 163/129/175/340 write %x: %x", addr, v);
		}
	}
}

static uint8_t namco_prg_read(struct cart *cart, uint16_t addr, bool *mem_hit)
{
	*mem_hit = true;

	if (addr >= 0x6000) {
		return map_read(&cart->prg, 0, addr, mem_hit);

	} else {
		switch (addr & 0xF800) {
			case 0x4800: // Expansion audio
				break;
			case 0x5000: // IRQ
				return cart->irq.counter & 0xFF;
			case 0x5800:
				return cart->irq.counter >> 8;
		}
	}

	*mem_hit = false;

	return 0;
}

static void namco_step(struct cart *cart, struct cpu *cpu)
{
	if (cart->irq.ack) {
		cpu_irq(cpu, IRQ_MAPPER, false);
		cart->irq.ack = false;
	}

	if (cart->irq.enable) {
		if (++cart->irq.counter == 0x7FFE) {
			cpu_irq(cpu, IRQ_MAPPER, true);
			cart->irq.enable = false;
		}
	}
}
