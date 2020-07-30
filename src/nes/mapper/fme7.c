// https://wiki.nesdev.com/w/index.php/Sunsoft_FME-7

static void fme7_create(struct cart *cart)
{
	uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x2000) - 1;

	cart_map(&cart->prg, ROM, 0xE000, last_bank, 8);

	if (cart->prg.ram.size > 0)
		cart_map(&cart->prg, RAM, 0x6000, 0, 8);

	cart_map_ciram(&cart->chr, NES_MIRROR_VERTICAL);
}

static void fme7_prg_write(struct cart *cart, struct apu *apu, uint16_t addr, uint8_t v)
{
	if (addr >= 0x6000 && addr < 0x8000 && cart->ram_enable) {
		map_write(&cart->prg, 0, addr, v);

	} else if (addr >= 0x8000 && addr < 0xA000) { // CMD
		cart->REG[0] = v & 0x0F;

	} else if (addr >= 0xA000 && addr < 0xC000) { // Parameter
		switch (cart->REG[0]) {
			case 0x0: // CHR
			case 0x1:
			case 0x2:
			case 0x3:
			case 0x4:
			case 0x5:
			case 0x6:
			case 0x7:
				cart_map(&cart->chr, ROM, cart->REG[0] * 0x0400, v, 1);
				break;
			case 0x8: { // PRG0 (RAM or ROM)
				enum mem type = v & 0x40 ? RAM : ROM;
				cart->ram_enable = v & 0x80;
				cart_map(&cart->prg, type, 0x6000, v & 0x3F, 8);

				if (type == RAM && !cart->ram_enable) { // Unmap address space
					map_unmap(&cart->prg, 0, 0x6000);
					map_unmap(&cart->prg, 0, 0x7000);
				}
				break;
			}
			case 0x9: // PRG1-3 (ROM)
			case 0xA:
			case 0xB:
				cart_map(&cart->prg, ROM, 0x8000 + (cart->REG[0] - 0x9) * 0x2000, v & 0x3F, 8);
				break;
			case 0xC: // Mirroring
				switch (v & 0x03) {
					case 0: cart_map_ciram(&cart->chr, NES_MIRROR_VERTICAL);    break;
					case 1: cart_map_ciram(&cart->chr, NES_MIRROR_HORIZONTAL);  break;
					case 2: cart_map_ciram(&cart->chr, NES_MIRROR_SINGLE0);     break;
					case 3: cart_map_ciram(&cart->chr, NES_MIRROR_SINGLE1);     break;
				}
				break;
			case 0xD: // IRQ
				cart->irq.enable = v & 0x01;
				cart->irq.cycle = v & 0x80;
				cart->irq.ack = true;
				break;
			case 0xE:
				cart->irq.value = (cart->irq.value & 0xFF00) | v;
				break;
			case 0xF:
				cart->irq.value = (cart->irq.value & 0x00FF) | ((uint16_t) v << 8);
				break;
		}
	} else if (addr >= 0xC000 && addr < 0xE000) { // Sunsoft 5B Audio CMD
		cart->REG[1] = v & 0x0F;

	} else if (addr >= 0xE000) { // Sunsoft 5B Audio
		switch (cart->REG[1]) {
			case 0x00: // Channel A, B, C Low Period
			case 0x02:
			case 0x04:
			case 0x01: // Channel A, B, C High Period
			case 0x03:
			case 0x05:

			case 0x07: // Channel A, B, C Tone Disable

			case 0x08: // Channel A, B, C, Volume
			case 0x09:
			case 0x0A:
				apu_write(apu, NULL, 0xE000 | cart->REG[1], v, EXT_SS5B);
				break;
		}
	}
}

static void fme7_step(struct cart *cart, struct cpu *cpu)
{
	if (cart->irq.ack) {
		cpu_irq(cpu, IRQ_MAPPER, false);
		cart->irq.ack = false;
	}

	if (cart->irq.cycle) {
		if (--cart->irq.value == 0xFFFF)
			cpu_irq(cpu, IRQ_MAPPER, cart->irq.enable);
	}
}
