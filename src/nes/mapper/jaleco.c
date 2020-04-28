// https://wiki.nesdev.com/w/index.php/INES_Mapper_018

static void jaleco_create(struct cart *cart)
{
	cart->ram_enable = true;

	uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x2000) - 1;
	cart_map(&cart->prg, ROM, 0xE000, last_bank, 8);

	if (cart->prg.ram.size > 0)
		cart_map(&cart->prg, RAM, 0x6000, 0, 8);
}

static void jaleco_map_prg(struct cart *cart, uint8_t n, uint8_t v)
{
	cart->PRG[n] = v;
	cart_map(&cart->prg, ROM, 0x8000 + (n / 2) * 0x2000,
		(cart->PRG[n & 0xE] & 0xF) | ((cart->PRG[(n & 0xE) + 1] & 0x3) << 4), 8);
}

static void jaleco_map_chr(struct cart *cart, uint8_t n, uint8_t v)
{
	cart->CHR[n] = v;
	cart_map(&cart->chr, ROM, 0x400 * (n / 2),
		(cart->CHR[n & 0xE] & 0xF) | ((cart->CHR[(n & 0xE) + 1] & 0xF) << 4), 1);
}

static void jaleco_prg_write(struct cart *cart, struct cpu *cpu, uint16_t addr, uint8_t v)
{
	if (addr >= 0x6000 && addr < 0x8000 && cart->ram_enable) {
		map_write(&cart->prg, 0, addr, v);
		cart->sram_dirty = cart->prg.sram;

	} else if (addr >= 0x8000) {
		switch (addr & 0xF003) {
			case 0x8000: jaleco_map_prg(cart, 0, v); break;
			case 0x8001: jaleco_map_prg(cart, 1, v); break;
			case 0x8002: jaleco_map_prg(cart, 2, v); break;
			case 0x8003: jaleco_map_prg(cart, 3, v); break;
			case 0x9000: jaleco_map_prg(cart, 4, v); break;
			case 0x9001: jaleco_map_prg(cart, 5, v); break;

			case 0x9002:
				cart->ram_enable = v & 0x3;
				break;

			case 0xA000: jaleco_map_chr(cart, 0, v); break;
			case 0xA001: jaleco_map_chr(cart, 1, v); break;
			case 0xA002: jaleco_map_chr(cart, 2, v); break;
			case 0xA003: jaleco_map_chr(cart, 3, v); break;
			case 0xB000: jaleco_map_chr(cart, 4, v); break;
			case 0xB001: jaleco_map_chr(cart, 5, v); break;
			case 0xB002: jaleco_map_chr(cart, 6, v); break;
			case 0xB003: jaleco_map_chr(cart, 7, v); break;
			case 0xC000: jaleco_map_chr(cart, 8, v); break;
			case 0xC001: jaleco_map_chr(cart, 9, v); break;
			case 0xC002: jaleco_map_chr(cart, 10, v); break;
			case 0xC003: jaleco_map_chr(cart, 11, v); break;
			case 0xD000: jaleco_map_chr(cart, 12, v); break;
			case 0xD001: jaleco_map_chr(cart, 13, v); break;
			case 0xD002: jaleco_map_chr(cart, 14, v); break;
			case 0xD003: jaleco_map_chr(cart, 15, v); break;

			case 0xE000:
			case 0xE001:
			case 0xE002:
			case 0xE003:
				cart->REG[addr & 0x3] = v;
				break;

			case 0xF000:
				cpu_irq(cpu, IRQ_MAPPER, false);
				cart->irq.counter = cart->REG[0] | (cart->REG[1] << 4) | (cart->REG[2] << 8) |
					(cart->REG[3] << 12);
				break;

			case 0xF001:
				cpu_irq(cpu, IRQ_MAPPER, false);
				cart->irq.enable = v & 0x1;
				cart->irq.value = (v & 0x8) ? 0xF : (v & 0x4) ? 0xFF : (v & 0x2) ? 0xFFF : 0xFFFF;
				break;

			case 0xF002:
				switch (v & 0x3) {
					case 0: cart_map_ciram(&cart->chr, NES_MIRROR_HORIZONTAL); break;
					case 1: cart_map_ciram(&cart->chr, NES_MIRROR_VERTICAL);   break;
					case 2: cart_map_ciram(&cart->chr, NES_MIRROR_SINGLE0);    break;
					case 3: cart_map_ciram(&cart->chr, NES_MIRROR_SINGLE1);    break;
				}
				break;
		}
	}
}

static void jaleco_step(struct cart *cart, struct cpu *cpu)
{
	if (cart->irq.enable) {
		uint16_t counter = cart->irq.counter & cart->irq.value;

		if (--counter == 0)
			cpu_irq(cpu, IRQ_MAPPER, true);

		cart->irq.counter = (cart->irq.counter & ~cart->irq.value) | (counter & cart->irq.value);
	}
}
