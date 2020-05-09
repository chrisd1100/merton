#include "ppu.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

// VRAM address helpers
#define GET_CX(reg)       ((reg) & 0x001F)
#define GET_CY(reg)       (((reg) & 0x03E0) >> 5)
#define GET_NT(reg)       (((reg) & 0x0C00) >> 10)
#define GET_FY(reg)       (((reg) & 0x7000) >> 12)

#define SET_CX(reg, cx)   ((reg) = ((reg) & 0x7FE0) | ((uint16_t) ((cx) & 0x1F)))
#define SET_CY(reg, cy)   ((reg) = ((reg) & 0x7C1F) | ((uint16_t) ((cy) & 0x1F) << 5))
#define SET_NT_H(reg, nt) ((reg) = ((reg) & 0x7BFF) | ((uint16_t) ((nt) & 0x01) << 10))
#define SET_NT_V(reg, nt) ((reg) = ((reg) & 0x77FF) | ((uint16_t) ((nt) & 0x02) << 10))
#define SET_NT(reg, nt)   ((reg) = ((reg) & 0x73FF) | ((uint16_t) ((nt) & 0x03) << 10))
#define SET_FY(reg, fy)   ((reg) = ((reg) & 0x0FFF) | ((uint16_t) ((fy) & 0x07) << 12))

#define SET_H(reg, h)     ((reg) = ((reg) & 0x00FF) | ((uint16_t) ((h) & 0x3F) << 8))
#define SET_L(reg, l)     ((reg) = ((reg) & 0x7F00) | ((uint16_t) (l)))

#define FLIP_NT_H(reg)    ((reg) ^= 0x0400)
#define FLIP_NT_V(reg)    ((reg) ^= 0x0800)

// PPUSTATUS $2002
enum ppu_status_flags {
	FLAG_STATUS_O = 0x20, // Sprite overflow
	FLAG_STATUS_S = 0x40, // Sprite 0 hit
	FLAG_STATUS_V = 0x80, // Vblank
};

struct sprite {
	uint16_t addr;
	uint8_t low_tile;
	uint8_t id;
};

struct spr {
	uint8_t color;
	bool priority;
	bool sprite0;
};

struct ppu {
	NES_Config cfg;

	uint8_t output[256];
	uint32_t pixels[240][256];
	uint32_t palettes[8][64];

	uint8_t palette_ram[32];
	uint8_t oam[256];
	uint8_t soam[64][4];

	struct {
		bool nmi_enabled;
		uint8_t incr;
		uint8_t sprite_h;
		uint8_t nt;
		uint16_t bg_table;
		uint16_t sprite_table;
	} CTRL;

	struct {
		uint8_t grayscale;
		uint8_t emphasis;
		bool show_bg;
		bool show_sprites;
		bool show_left_bg;
		bool show_left_sprites;
	} MASK;

	uint8_t STATUS;
	uint8_t OAMADDR;

	uint16_t bus_v; // The current ppu address bus -- important for A12 toggling signal for MMC3
	uint16_t v;     // Current vram address
	uint16_t t;     // Temporary vram address holds state between STATUS and/or write latch
	uint8_t x;      // Fine x scroll
	bool w;         // Write latch
	bool f;         // Even/odd frame flag

	uint16_t tmp_v;
	uint8_t set_v;

	uint8_t bgl;
	uint8_t bgh;
	uint8_t nt;
	uint8_t attr;
	uint8_t bg[272];

	uint8_t oam_n;
	uint8_t soam_n;
	uint8_t eval_step;
	bool overflow;
	bool has_sprites;
	struct sprite sprites[64];
	struct spr spr[256];

	uint8_t open_bus;
	uint8_t read_buffer;
	uint8_t decay_high2;
	uint8_t decay_low5;

	uint16_t scanline;
	uint16_t dot;
	bool rendering;
	bool supress_nmi;
	bool output_v;
	bool new_frame;
	bool palette_write;
};

static const uint32_t PALETTES[7][64] = {
	[NES_PALETTE_SMOOTH]    = {0xFF6A6D6A, 0xFF801300, 0xFF8A001E, 0xFF7A0039, 0xFF560055, 0xFF18005A, 0xFF00104F, 0xFF001C3D, 0xFF003225, 0xFF003D00, 0xFF004000, 0xFF243900, 0xFF552E00, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFB9BCB9, 0xFFC75018, 0xFFE3304B, 0xFFD62273, 0xFFA91F95, 0xFF5C289D, 0xFF003798, 0xFF004C7F, 0xFF00645E, 0xFF007722, 0xFF027E02, 0xFF457600, 0xFF8A6E00, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFA668, 0xFFFF9C8C, 0xFFFF86B5, 0xFFFD75D9, 0xFFB977E3, 0xFF688DE5, 0xFF299DD4, 0xFF0CAFB3, 0xFF11C27B, 0xFF47CA55, 0xFF81CB46, 0xFFC5C147, 0xFF4A4D4A, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFEACC, 0xFFFFDEDD, 0xFFFFDAEC, 0xFFFED7F8, 0xFFF5D6FC, 0xFFCFDBFD, 0xFFB5E7F9, 0xFFAAF0F1, 0xFFA9FADA, 0xFFBCFFC9, 0xFFD7FBC3, 0xFFF6F6C4, 0xFFBEC1BE, 0xFF000000, 0xFF000000},
	[NES_PALETTE_CLASSIC]   = {0xFF616161, 0xFF880000, 0xFF990D1F, 0xFF791337, 0xFF601256, 0xFF10005D, 0xFF000E52, 0xFF08233A, 0xFF0C3521, 0xFF0E410D, 0xFF174417, 0xFF1F3A00, 0xFF572F00, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFAAAAAA, 0xFFC44D0D, 0xFFDE244B, 0xFFCF1269, 0xFFAD1490, 0xFF481C9D, 0xFF043492, 0xFF055073, 0xFF13695D, 0xFF117A16, 0xFF088013, 0xFF497612, 0xFF91661C, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFCFCFC, 0xFFFC9A63, 0xFFFC7E8A, 0xFFFC6AB0, 0xFFF26DDD, 0xFFAB71E7, 0xFF5886E3, 0xFF229ECC, 0xFF00B1A8, 0xFF00C172, 0xFF4ECD5A, 0xFF8EC234, 0xFFCEBE4F, 0xFF424242, 0xFF000000, 0xFF000000, 0xFFFCFCFC, 0xFFFCD4BE, 0xFFFCCACA, 0xFFFCC4D9, 0xFFFCC1EC, 0xFFE7C3FA, 0xFFC3CEF7, 0xFFA7CDE2, 0xFF9CDBDA, 0xFF9EE3C8, 0xFFB8E5BF, 0xFFC8EBB2, 0xFFEBE5B7, 0xFFACACAC, 0xFF000000, 0xFF000000},
	[NES_PALETTE_COMPOSITE] = {0xFF656565, 0xFF7D1200, 0xFF8E0018, 0xFF820036, 0xFF5D0056, 0xFF18005A, 0xFF00054F, 0xFF001938, 0xFF00311D, 0xFF003D00, 0xFF004100, 0xFF173B00, 0xFF552E00, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFAFAFAF, 0xFFC84E19, 0xFFE32F47, 0xFFD71F6B, 0xFFAE1B93, 0xFF5E1A9E, 0xFF003299, 0xFF004B7B, 0xFF00675B, 0xFF007A26, 0xFF008200, 0xFF3E7A00, 0xFF8A6E00, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFA964, 0xFFFF898E, 0xFFFF76B6, 0xFFFF6FE0, 0xFFC46CEF, 0xFF6A80F0, 0xFF2C98D8, 0xFF0AB4B9, 0xFF0CCB83, 0xFF3FD65B, 0xFF7ED14A, 0xFFCBC74D, 0xFF4C4C4C, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFE5C7, 0xFFFFD9D9, 0xFFFFD1E9, 0xFFFFCEF9, 0xFFF1CCFF, 0xFFCBD4FF, 0xFFB1DFF8, 0xFFA4EAED, 0xFFA4F4D6, 0xFFB8F8C5, 0xFFD3F6BE, 0xFFF1F1BF, 0xFFB9B9B9, 0xFF000000, 0xFF000000},
	[NES_PALETTE_PVM_D93]   = {0xFF636B69, 0xFF741700, 0xFF87001E, 0xFF730034, 0xFF570056, 0xFF13005E, 0xFF001A53, 0xFF00243B, 0xFF003024, 0xFF003A06, 0xFF003F00, 0xFF1E3B00, 0xFF4E3300, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFB3BBB9, 0xFFB95314, 0xFFDA2C4D, 0xFFDE1E67, 0xFF9C1898, 0xFF44239D, 0xFF003EA0, 0xFF00558D, 0xFF006D65, 0xFF00792C, 0xFF008100, 0xFF427D00, 0xFF8A7800, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFA869, 0xFFFF9196, 0xFFFA8AB2, 0xFFFA7DEA, 0xFFC77BF3, 0xFF598EF2, 0xFF27ADE6, 0xFF05C8D7, 0xFF07DF90, 0xFF3CE564, 0xFF7DE245, 0xFFD9D548, 0xFF48504E, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFEAD2, 0xFFFFE2E2, 0xFFFFD8E9, 0xFFFFD2F5, 0xFFEAD9F8, 0xFFB9DEFA, 0xFF9BE8F9, 0xFF8CF2F3, 0xFF91FAD3, 0xFFA8FCB8, 0xFFCAFAAE, 0xFFF3F3CA, 0xFFB8C0BE, 0xFF000000, 0xFF000000},
	[NES_PALETTE_PC10]      = {0xFF6D6D6D, 0xFF922400, 0xFFDB0000, 0xFFDB496D, 0xFF6D0092, 0xFF6D00B6, 0xFF0024B6, 0xFF004992, 0xFF00496D, 0xFF004924, 0xFF246D00, 0xFF009200, 0xFF494900, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFB6B6B6, 0xFFDB6D00, 0xFFFF4900, 0xFFFF0092, 0xFFFF00B6, 0xFF9200FF, 0xFF0000FF, 0xFF006DDB, 0xFF006D92, 0xFF009224, 0xFF009200, 0xFF6DB600, 0xFF929200, 0xFF242424, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFB66D, 0xFFFF9292, 0xFFFF6DDB, 0xFFFF00FF, 0xFFFF6DFF, 0xFF0092FF, 0xFF00B6FF, 0xFF00DBDB, 0xFF00DB6D, 0xFF00FF00, 0xFFDBFF49, 0xFFFFFF00, 0xFF494949, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFDBB6, 0xFFFFB6DB, 0xFFFFB6FF, 0xFFFF92FF, 0xFFB6B6FF, 0xFF92DBFF, 0xFF49FFFF, 0xFF6DFFFF, 0xFF49FFB6, 0xFF6DFF92, 0xFFDBFF49, 0xFFFFDB92, 0xFF929292, 0xFF000000, 0xFF000000},
	[NES_PALETTE_SONY_CXA]  = {0xFF585858, 0xFF8C2300, 0xFF9B1300, 0xFF85052D, 0xFF52005D, 0xFF17007A, 0xFF00087A, 0xFF00185F, 0xFF002A35, 0xFF003909, 0xFF003F00, 0xFF223C00, 0xFF5D3200, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFA1A1A1, 0xFFEE5300, 0xFFFE3C15, 0xFFE42860, 0xFF981DA9, 0xFF411ED4, 0xFF002CD2, 0xFF0044AA, 0xFF005E6C, 0xFF00732D, 0xFF067D00, 0xFF527800, 0xFFA96900, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFEA51F, 0xFFFE895E, 0xFFFE72B5, 0xFFF665FE, 0xFF9067FE, 0xFF3C77FE, 0xFF0893FE, 0xFF00B2C4, 0xFF10CA79, 0xFF4AD53A, 0xFFA4D111, 0xFFFEBF06, 0xFF424242, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFED9A0, 0xFFFECCBD, 0xFFFEC2E1, 0xFFFBBCFE, 0xFFD0BDFE, 0xFFA9C5FE, 0xFF8ED1FE, 0xFF86DEE9, 0xFF92E9C7, 0xFFB0EEA8, 0xFFD9EC95, 0xFFFEE491, 0xFFACACAC, 0xFF000000, 0xFF000000},
	[NES_PALETTE_WAVEBEAM]  = {0xFF6B6B6B, 0xFF881B00, 0xFF9A0021, 0xFF8C0040, 0xFF670060, 0xFF1E0064, 0xFF000859, 0xFF001648, 0xFF003628, 0xFF004500, 0xFF084900, 0xFF1D4200, 0xFF593600, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFB4B4B4, 0xFFD35515, 0xFFEF3743, 0xFFDF2574, 0xFFB9199C, 0xFF640FAC, 0xFF002CAA, 0xFF004B8A, 0xFF006B66, 0xFF008321, 0xFF008A00, 0xFF448100, 0xFF917600, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFB263, 0xFFFF9C7C, 0xFFFE7DC0, 0xFFFF77E9, 0xFFCD72F5, 0xFF6B88F4, 0xFF29A0DD, 0xFF0ABDBD, 0xFF0ED289, 0xFF3EDE5C, 0xFF86D84B, 0xFFD2CF4D, 0xFF525252, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFDFBC, 0xFFFFD2D2, 0xFFFFC8E1, 0xFFFFC7EF, 0xFFE1C3FF, 0xFFC6CAFF, 0xFFADDAF2, 0xFFA0E3EB, 0xFFA2EDD2, 0xFFB4F4BC, 0xFFCEF1B5, 0xFFF1ECB6, 0xFFBFBFBF, 0xFF000000, 0xFF000000},
};

static const float EMPHASIS[8][3] = {
	{1.00f, 1.00f, 1.00f}, // 000 Black
	{1.00f, 0.85f, 0.85f}, // 001 Red
	{0.85f, 1.00f, 0.85f}, // 010 Green
	{0.85f, 0.85f, 0.70f}, // 011 Yellow
	{0.85f, 0.85f, 1.00f}, // 100 Blue
	{0.85f, 0.70f, 0.85f}, // 101 Magenta
	{0.70f, 0.85f, 0.85f}, // 110 Cyan
	{0.70f, 0.70f, 0.70f}, // 111 White
};

static const uint8_t POWER_UP_PALETTE[32] = {
	0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0D, 0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2C,
	0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14, 0x08, 0x3A, 0x00, 0x02, 0x00, 0x20, 0x2C, 0x08,
};


// NMI

void ppu_assert_nmi(struct ppu *ppu, struct cpu *cpu)
{
	cpu_nmi(cpu, ppu->CTRL.nmi_enabled && GET_FLAG(ppu->STATUS, FLAG_STATUS_V));
}


// VRAM

static void ppu_scroll_h(struct ppu *ppu);
static void ppu_scroll_v(struct ppu *ppu);

static bool ppu_visible(struct ppu *ppu)
{
	return ppu->rendering && (ppu->scanline <= 239 || ppu->scanline == 261);
}

static void ppu_set_bus_v(struct ppu *ppu, struct cart *cart, uint16_t v)
{
	if (!(ppu->bus_v & 0x1000) && (v & 0x1000))
		cart_ppu_a12_toggle(cart);

	ppu->bus_v = v;
}

static void ppu_set_v(struct ppu *ppu, struct cart *cart, uint16_t v, bool glitch)
{
	// https://wiki.nesdev.com/w/index.php/PPU_scrolling#.242007_reads_and_writes
	if (glitch && ppu_visible(ppu)) {
		ppu_scroll_h(ppu);
		ppu_scroll_v(ppu);

	} else {
		ppu->v = v;
		ppu->output_v = ppu->v >= 0x3F00 && ppu->v < 0x4000;

		if (!ppu_visible(ppu))
			ppu_set_bus_v(ppu, cart, v);
	}
}

static uint8_t ppu_read_palette(struct ppu *ppu, uint16_t addr)
{
	addr &= (addr % 4 == 0) ? 0x0F : 0x1F;

	return ppu->palette_ram[addr];
}

static uint8_t ppu_read_vram(struct ppu *ppu, struct cart *cart, uint16_t addr, enum mem type, bool nt)
{
	if (addr < 0x3F00) {
		if (addr < 0x2000)
			ppu_set_bus_v(ppu, cart, addr);

		return cart_chr_read(cart, addr, type, nt);

	} else {
		return ppu_read_palette(ppu, addr) & ppu->MASK.grayscale;
	}
}

static void ppu_write_vram(struct ppu *ppu, struct cart *cart, uint16_t addr, uint8_t v)
{
	if (addr < 0x3F00) {
		if (addr < 0x2000)
			ppu_set_bus_v(ppu, cart, addr);

		cart_chr_write(cart, addr, v);

	} else {
		addr &= (addr % 4 == 0) ? 0x0F : 0x1F;

		ppu->palette_ram[addr] = v;
		ppu->palette_write = true;
	}
}


// IO

uint8_t ppu_read(struct ppu *ppu, struct cart *cart, uint16_t addr)
{
	uint8_t v = ppu->open_bus;

	switch (addr) {
		case 0x2002:
			ppu->decay_high2 = 0;

			// https://wiki.nesdev.com/w/index.php/PPU_frame_timing#VBL_Flag_Timing
			if (ppu->scanline == 241 && ppu->dot == 1)
				ppu->supress_nmi = true;

			v = ppu->open_bus = (ppu->open_bus & 0x1F) | ppu->STATUS;
			UNSET_FLAG(ppu->STATUS, FLAG_STATUS_V);
			ppu->w = false;
			break;

		case 0x2004:
			ppu->decay_high2 = ppu->decay_low5 = 0;

			if (ppu_visible(ppu)) {
				int32_t pos = ppu->dot - 258;
				int32_t n = pos / 8;
				int32_t m = (pos % 8 > 3) ? 3 : pos % 8;

				v = ppu->open_bus =
					(pos >= 0 && n < 8) ? ppu->soam[n][m] :
					(ppu->dot < 65 || (ppu->soam_n == 8 && (ppu->dot & 0x01) == 0)) ? ppu->soam[0][0] :
					ppu->oam[ppu->OAMADDR];

			} else {
				v = ppu->open_bus = ppu->oam[ppu->OAMADDR];
			}

			break;

		case 0x2007: {
			uint16_t waddr = ppu->v & 0x3FFF;
			ppu->decay_high2 = ppu->decay_low5 = 0;

			// Buffered read from CHR
			if (waddr < 0x3F00) {
				v = ppu->open_bus = ppu->read_buffer;
				ppu->read_buffer = ppu_read_vram(ppu, cart, waddr, ROM, false);

			} else {
				// Read buffer gets ciram byte
				ppu->read_buffer = ppu_read_vram(ppu, cart, waddr - 0x1000, ROM, false);

				// Upper 2 bits get preserved from decay value
				v = (ppu->open_bus & 0xC0) | (ppu_read_vram(ppu, cart, waddr, ROM, false) & 0x3F);
			}

			ppu_set_v(ppu, cart, ppu->v + ppu->CTRL.incr, true);
			break;
		}
	}

	return v;
}

void ppu_write(struct ppu *ppu, struct cart *cart, uint16_t addr, uint8_t v)
{
	ppu->decay_high2 = ppu->decay_low5 = 0;
	ppu->open_bus = v;

	switch (addr) {
		case 0x2000:
			ppu->CTRL.nt = v & 0x03;
			ppu->CTRL.incr = (v & 0x04) ? 32 : 1;
			ppu->CTRL.sprite_table = (v & 0x08) ? 0x1000 : 0;
			ppu->CTRL.bg_table = (v & 0x10) ? 0x1000 : 0;
			ppu->CTRL.sprite_h = (v & 0x20) ? 16 : 8;
			ppu->CTRL.nmi_enabled = v & 0x80;

			SET_NT(ppu->t, ppu->CTRL.nt);
			break;

		case 0x2001:
			ppu->MASK.grayscale = (v & 0x01) ? 0x30 : 0x3F;
			ppu->MASK.show_left_bg = v & 0x02;
			ppu->MASK.show_left_sprites = v & 0x04;
			ppu->MASK.show_bg = v & 0x08;
			ppu->MASK.show_sprites = v & 0x10;
			ppu->MASK.emphasis = (v & 0xE0) >> 5;
			break;

		case 0x2003:
			ppu->OAMADDR = v;
			break;

		case 0x2004:
			// https://wiki.nesdev.com/w/index.php/PPU_registers#OAM_data_.28.242004.29_.3C.3E_read.2Fwrite

			if (!ppu_visible(ppu)) {
				if ((ppu->OAMADDR + 2) % 4 == 0)
					v &= 0xE3;

				ppu->oam[ppu->OAMADDR++] = v;

			} else {
				ppu->OAMADDR += 4;
			}
			break;

		case 0x2005:
			if (!ppu->w) {
				ppu->x = v & 0x07;
				SET_CX(ppu->t, v >> 3);

			} else {
				SET_FY(ppu->t, v);
				SET_CY(ppu->t, v >> 3);
			}

			ppu->w = !ppu->w;
			break;

		case 0x2006:
			if (!ppu->w) {
				SET_H(ppu->t, v);

			} else {
				SET_L(ppu->t, v);
				ppu->set_v = 0x8; // 3 PPU cycles
				ppu->tmp_v = ppu->t;
			}

			ppu->w = !ppu->w;
			break;

		case 0x2007:
			ppu_write_vram(ppu, cart, ppu->v & 0x3FFF, v);
			ppu_set_v(ppu, cart, ppu->v + ppu->CTRL.incr, true);
			break;
	}
}


// Scrolling
// https://wiki.nesdev.com/w/index.php/PPU_scrolling

static void ppu_scroll_h(struct ppu *ppu)
{
	uint16_t cx = GET_CX(ppu->v);

	if (cx == 31) {
		SET_CX(ppu->v, 0);
		FLIP_NT_H(ppu->v);

	} else {
		SET_CX(ppu->v, cx + 1);
	}
}

static void ppu_scroll_v(struct ppu *ppu)
{
	uint16_t fy = GET_FY(ppu->v);

	if (fy < 7) {
		SET_FY(ppu->v, fy + 1);

	} else {
		SET_FY(ppu->v, 0);

		uint16_t cy = GET_CY(ppu->v);

		if (cy == 29) {
			SET_CY(ppu->v, 0);
			FLIP_NT_V(ppu->v);

		} else if (cy == 31) {
			SET_CY(ppu->v, 0);

		} else {
			SET_CY(ppu->v, cy + 1);
		}
	}
}

static void ppu_scroll_copy_x(struct ppu *ppu)
{
	SET_CX(ppu->v, GET_CX(ppu->t));
	SET_NT_H(ppu->v, GET_NT(ppu->t));
}

static void ppu_scroll_copy_y(struct ppu *ppu)
{
	SET_CY(ppu->v, GET_CY(ppu->t));
	SET_FY(ppu->v, GET_FY(ppu->t));
	SET_NT_V(ppu->v, GET_NT(ppu->t));
}


// Background

static uint8_t ppu_read_nt_byte(struct ppu *ppu, struct cart *cart, enum mem type)
{
	return ppu_read_vram(ppu, cart, 0x2000 | (ppu->v & 0x0FFF), type, true);
}

static uint8_t ppu_read_attr_byte(struct ppu *ppu, struct cart *cart, enum mem type)
{
	uint16_t addr = 0x23C0 | (ppu->v & 0x0C00) | ((ppu->v >> 4) & 0x0038) | ((ppu->v >> 2) & 0x0007);

	uint8_t attr = ppu_read_vram(ppu, cart, addr, type, false);
	if (GET_CY(ppu->v) & 0x02) attr >>= 4;
	if (GET_CX(ppu->v) & 0x02) attr >>= 2;

	return attr;
}

static uint8_t ppu_read_tile_byte(struct ppu *ppu, struct cart *cart, uint8_t nt, uint8_t offset)
{
	uint16_t addr = ppu->CTRL.bg_table + (nt * 16) + GET_FY(ppu->v);

	return ppu_read_vram(ppu, cart, addr + offset, BGROM, false);
}

static uint8_t ppu_color(uint8_t low_tile, uint8_t high_tile, uint8_t attr, uint8_t shift)
{
	uint8_t color = ((high_tile >> shift) << 1) & 0x02;
	color |= (low_tile >> shift) & 0x01;

	if (color > 0)
		color |= (attr << 2) & 0x0C;

	return color;
}

static void ppu_store_bg(struct ppu *ppu, uint16_t bg_dot)
{
	for (uint8_t x = 0; x < 8; x++)
		ppu->bg[bg_dot + (7 - x)] = ppu_color(ppu->bgl, ppu->bgh, ppu->attr, x);
}

static void ppu_fetch_bg(struct ppu *ppu, struct cart *cart, uint16_t bg_dot)
{
	switch (ppu->dot % 8) {
		case 1:
			ppu->nt = ppu_read_nt_byte(ppu, cart, BGROM);
			break;
		case 3:
			ppu->attr = ppu_read_attr_byte(ppu, cart, BGROM);
			break;
		case 5:
			ppu->bgl = ppu_read_tile_byte(ppu, cart, ppu->nt, 0);
			break;
		case 7:
			ppu->bgh = ppu_read_tile_byte(ppu, cart, ppu->nt, 8);
			break;
		case 0:
			ppu_store_bg(ppu, bg_dot);
			ppu_scroll_h(ppu);
			break;
	}
}


// Sprites
// https://wiki.nesdev.com/w/index.php/PPU_OAM
// https://wiki.nesdev.com/w/index.php/PPU_sprite_evaluation

#define SPRITE_Y(oam, n)               ((oam)[(n) + 0]) // Y position of top of sprite
#define SPRITE_INDEX(oam, n)           ((oam)[(n) + 1]) // 8x8 this is the tile number, 8x16 uses bit 0 as pattern table
#define SPRITE_ATTR(oam, n)            ((oam)[(n) + 2]) // Sprite attributes defined below
#define SPRITE_X(oam, n)               ((oam)[(n) + 3]) // X position of the left side of the sprite

#define SPRITE_ATTR_PALETTE(attr)      ((attr) & 0x03) // 4 to 7
#define SPRITE_ATTR_PRIORITY(attr)     ((attr) & 0x20) // 0 in front of bg, 1 behind bg
#define SPRITE_ATTR_FLIP_H(attr)       ((attr) & 0x40) // Flip the sprite horizontally
#define SPRITE_ATTR_FLIP_V(attr)       ((attr) & 0x80) // Flip the sprite vertically

#define SPRITE_INDEX_8X16_TABLE(index) ((index) & 0x01) // 8x16 sprites use the bit 0 as the table
#define SPRITE_INDEX_8X16_TILE(index)  ((index) & 0xFE) // 8x16 sprites ignore bit 0 for the tile number

static uint16_t ppu_sprite_addr(struct ppu *ppu, uint16_t row, uint8_t index, uint8_t attr)
{
	uint16_t table = 0;
	uint8_t tile = 0;

	if (SPRITE_ATTR_FLIP_V(attr))
		row = (ppu->CTRL.sprite_h - 1) - row;

	if (ppu->CTRL.sprite_h == 8) {
		table = ppu->CTRL.sprite_table;
		tile = index;

	} else {
		table = SPRITE_INDEX_8X16_TABLE(index) ? 0x1000 : 0x0000;
		tile = SPRITE_INDEX_8X16_TILE(index);

		if (row > 7) {
			tile++;
			row -= 8;
		}
	}

	return table + tile * 16 + row;
}

static void ppu_store_sprite_colors(struct ppu *ppu, uint8_t attr, uint8_t sprite_x, uint8_t id,
	uint8_t low_tile, uint8_t high_tile)
{
	for (uint8_t x = 0; x < 8; x++) {
		uint8_t shift = SPRITE_ATTR_FLIP_H(attr) ? x : 7 - x;
		uint8_t color = ppu_color(low_tile, high_tile, SPRITE_ATTR_PALETTE(attr), shift);
		uint16_t offset = sprite_x + x;

		if (offset < 256 && color != 0) {
			ppu->has_sprites = true;

			if (!ppu->spr[offset].sprite0)
				ppu->spr[offset].sprite0 = id == 0 && offset != 255;

			if (ppu->spr[offset].color == 0) {
				ppu->spr[offset].color = color + 16;
				ppu->spr[offset].priority = SPRITE_ATTR_PRIORITY(attr);
			}
		}
	}
}

static void ppu_eval_sprites(struct ppu *ppu)
{
	switch (ppu->eval_step) {
		case 1:
			if (ppu->oam_n < 64) {
				uint8_t y = SPRITE_Y(ppu->oam, ppu->OAMADDR);
				int32_t row = ppu->scanline - y;

				if (ppu->soam_n < ppu->cfg.maxSprites)
					ppu->soam[ppu->soam_n][0] = y;

				if (row >= 0 && row < ppu->CTRL.sprite_h) {
					if (ppu->soam_n == ppu->cfg.maxSprites) {
						SET_FLAG(ppu->STATUS, FLAG_STATUS_O);
						ppu->overflow = true;

					} else {
						ppu->soam[ppu->soam_n][1] = SPRITE_INDEX(ppu->oam, ppu->OAMADDR);
						ppu->soam[ppu->soam_n][2] = SPRITE_ATTR(ppu->oam, ppu->OAMADDR);
						ppu->soam[ppu->soam_n][3] = SPRITE_X(ppu->oam, ppu->OAMADDR);

						ppu->sprites[ppu->soam_n].id = ppu->oam_n;
					}

					ppu->eval_step++;
					ppu->OAMADDR++;
					return;

				} else if (ppu->soam_n == ppu->cfg.maxSprites && !ppu->overflow) {
					ppu->OAMADDR = (ppu->OAMADDR & 0xFC) + ((ppu->OAMADDR + 1) & 0x03);
				}
			}

			ppu->eval_step = 0;
			ppu->oam_n++;
			ppu->OAMADDR += 4;
			break;
		case 0:
		case 2:
		case 4:
		case 6:
			ppu->eval_step++;
			break;
		case 3:
		case 5:
			ppu->eval_step++;
			ppu->OAMADDR++;
			break;
		case 7:
			if (ppu->soam_n < ppu->cfg.maxSprites)
				ppu->soam_n++;

			ppu->eval_step = 0;
			ppu->oam_n++;
			ppu->OAMADDR++;
			ppu->OAMADDR &= 0xFC;
			break;
	}
}

static void ppu_fetch_sprite(struct ppu *ppu, struct cart *cart, uint16_t dot)
{
	int32_t n = (dot - 257) / 8;
	struct sprite *s = &ppu->sprites[n];

	switch (dot % 8) {
		case 1:
			ppu_read_nt_byte(ppu, cart, SPRROM);
			break;
		case 3:
			ppu_read_attr_byte(ppu, cart, SPRROM);
			break;
		case 5: {
			int32_t row = ppu->scanline - ppu->soam[n][0];
			s->addr = ppu_sprite_addr(ppu, (uint16_t) (row > 0 ? row : 0), ppu->soam[n][1], ppu->soam[n][2]);
			s->low_tile = ppu_read_vram(ppu, cart, s->addr, SPRROM, false);
			break;
		} case 7: {
			uint8_t high_tile = ppu_read_vram(ppu, cart, s->addr + 8, SPRROM, false);

			if (n < ppu->soam_n)
				ppu_store_sprite_colors(ppu, ppu->soam[n][2], ppu->soam[n][3], s->id, s->low_tile, high_tile);
			break;
		}
	}
}

static void ppu_oam_glitch(struct ppu *ppu)
{
	// https://wiki.nesdev.com/w/index.php/PPU_registers#OAMADDR

	if (ppu->OAMADDR >= 8)
		memcpy(ppu->oam, ppu->oam + ppu->OAMADDR, 8);
}


// Rendering
// https://wiki.nesdev.com/w/index.php/PPU_rendering#Preface

static void ppu_render(struct ppu *ppu, uint16_t dot)
{
	uint16_t addr = 0x3F00;

	if (ppu->rendering) {
		uint8_t bg_color = 0;

		if (ppu->MASK.show_bg && (dot > 7 || ppu->MASK.show_left_bg)) {
			bg_color = ppu->bg[dot + ppu->x];
			addr = 0x3F00 + bg_color;
		}

		if (ppu->has_sprites && ppu->MASK.show_sprites && (dot > 7 || ppu->MASK.show_left_sprites)) {
			struct spr *spr = &ppu->spr[dot];

			if (spr->sprite0 && bg_color != 0)
				SET_FLAG(ppu->STATUS, FLAG_STATUS_S);

			if (spr->color != 0 && (bg_color == 0 || !spr->priority))
				addr = 0x3F00 + spr->color;
		}

	} else if (ppu->output_v) {
		addr = ppu->v;
	}

	ppu->output[dot] = ppu_read_palette(ppu, addr);
}

static void ppu_output(struct ppu *ppu, uint16_t dot)
{
	uint8_t color = ppu->output[dot] & ppu->MASK.grayscale;

	ppu->pixels[ppu->scanline][dot] = ppu->palettes[ppu->MASK.emphasis][color];
}


// Step
// https://wiki.nesdev.com/w/index.php/PPU_rendering#Line-by-line_timing

static void ppu_clock(struct ppu *ppu)
{
	if (++ppu->dot > 340) {
		ppu->dot = 0;

		// Add additional cfg.preNMI + cfg.postNMI scanlines to give the CPU more time (emulator hack)
		if (++ppu->scanline > 261 + ppu->cfg.preNMI + ppu->cfg.postNMI) {
			ppu->scanline = 0;
			ppu->supress_nmi = false;
			ppu->f = !ppu->f;

			// Decay the open bus after 58 frames (~1s)
			if (ppu->decay_high2++ == 58)
				ppu->open_bus &= 0x3F;

			if (ppu->decay_low5++ == 58)
				ppu->open_bus &= 0xC0;
		}
	}
}

static void ppu_memory_access(struct ppu *ppu, struct cart *cart)
{
	if (ppu->dot >= 1 && ppu->dot <= 256) {
		if (ppu->dot == 1)
			ppu_oam_glitch(ppu);

		ppu_fetch_bg(ppu, cart, ppu->dot + 8);

		if (ppu->dot >= 65 && ppu->scanline != 261)
			ppu_eval_sprites(ppu);

		if (ppu->dot == 256) {
			ppu_scroll_v(ppu);

			// Squeeze in more sprite evaluation if cfg.maxSprites > 32 (emulator hack)
			while (ppu->oam_n < ppu->cfg.maxSprites && ppu->scanline != 261)
				ppu_eval_sprites(ppu);
		}

	} else if (ppu->dot >= 257 && ppu->dot <= 320) {
		ppu_fetch_sprite(ppu, cart, ppu->dot);

		ppu->OAMADDR = 0;

		if (ppu->dot == 257) {
			if (ppu->has_sprites) {
				memset(ppu->spr, 0, sizeof(struct spr) * 256);
				ppu->has_sprites = false;
			}
			ppu_scroll_copy_x(ppu);

		// Squeeze in additional sprite fetches if cfg.maxSprites > 8 (emulator hack)
		} else if (ppu->dot == 320) {
			for (uint16_t n = 321; n < 321 + (ppu->cfg.maxSprites - 8) * 8; n++)
				ppu_fetch_sprite(ppu, cart, n);
		}

	} else if (ppu->dot >= 321 && ppu->dot <= 336) {
		ppu_fetch_bg(ppu, cart, ppu->dot - 328);

	// Dummy nametable fetches, important for MMC5
	} else if (ppu->dot == 337 || ppu->dot == 339) {
		ppu_read_nt_byte(ppu, cart, SPRROM);
	}
}

void ppu_step(struct ppu *ppu, struct cart *cart)
{
	if (ppu->dot == 0) {
		ppu->oam_n = ppu->soam_n = ppu->eval_step = 0;
		ppu->overflow = false;
		memset(ppu->soam, 0xFF, 64 * 4);
	}

	if (ppu->scanline <= 239) {
		if (ppu->dot >= 1 && ppu->dot <= 256)
			ppu_render(ppu, ppu->dot - 1);

		// Delayed pixel output @Kitrinx
		if (ppu->dot >= 3 && ppu->dot <= 258)
			ppu_output(ppu, ppu->dot - 3);

		if (ppu->rendering)
			ppu_memory_access(ppu, cart);

	} else if (ppu->scanline == 240) {
		if (ppu->dot == 0) {
			ppu_set_bus_v(ppu, cart, ppu->v);

			if (!ppu->palette_write)
				memset(ppu->pixels, 0, 256 * 240 * 4);

			ppu->new_frame = true;
		}

	} else if (ppu->scanline == 241 + ppu->cfg.preNMI) {
		if (ppu->dot == 1 && !ppu->supress_nmi)
			SET_FLAG(ppu->STATUS, FLAG_STATUS_V);

	} else if (ppu->scanline == 261 + ppu->cfg.postNMI) {
		if (ppu->dot == 0) {
			UNSET_FLAG(ppu->STATUS, FLAG_STATUS_O);
			UNSET_FLAG(ppu->STATUS, FLAG_STATUS_S);
		}

		if (ppu->dot == 1)
			UNSET_FLAG(ppu->STATUS, FLAG_STATUS_V);

		if (ppu->rendering) {
			if (ppu->dot >= 280 && ppu->dot <= 304)
				ppu_scroll_copy_y(ppu);

			ppu_memory_access(ppu, cart);

			if (ppu->dot == 339 && ppu->f)
				ppu->dot++;
		}
	}

	// Delayed VRAM update address @Kitrinx Visual NES
	ppu->set_v >>= 1;
	if (ppu->set_v & 1)
		ppu_set_v(ppu, cart, ppu->tmp_v, false);

	// Delayed rendering flag @Kitrinx Visual NES
	ppu->rendering = ppu->MASK.show_bg || ppu->MASK.show_sprites;

	ppu_clock(ppu);
}

bool ppu_new_frame(struct ppu *ppu)
{
	return ppu->new_frame;
}

const uint32_t *ppu_pixels(struct ppu *ppu)
{
	ppu->new_frame = false;

	return (const uint32_t *) ppu->pixels;
}


// Configuration

static void ppu_generate_emphasis_tables(struct ppu *ppu, NES_Palette palette)
{
	memcpy(ppu->palettes[0], PALETTES[palette], sizeof(uint32_t) * 64);

	for (uint8_t x = 1; x < 8; x++) {
		for (uint8_t y = 0; y < 64; y++) {
			uint32_t rgba = PALETTES[palette][y];

			uint32_t r = (uint32_t) ((float) (rgba & 0x000000FF) * EMPHASIS[x][0]);
			uint32_t g = (uint32_t) ((float) ((rgba & 0x0000FF00) >> 8) * EMPHASIS[x][1]);
			uint32_t b = (uint32_t) ((float) ((rgba & 0x00FF0000) >> 16) * EMPHASIS[x][2]);

			ppu->palettes[x][y] = r | (g << 8) | (b << 16) | 0xFF000000;
		}
	}
}

void ppu_set_config(struct ppu *ppu, const NES_Config *cfg)
{
	ppu->cfg = *cfg;

	ppu_generate_emphasis_tables(ppu, ppu->cfg.palette);
}


// Lifecycle

void ppu_create(const NES_Config *cfg, struct ppu **ppu)
{
	struct ppu *ctx = *ppu = calloc(1, sizeof(struct ppu));

	ctx->cfg = *cfg;
}

void ppu_destroy(struct ppu **ppu)
{
	if (!ppu || *ppu)
		return;

	free(*ppu);
	*ppu = NULL;
}

void ppu_reset(struct ppu *ppu)
{
	NES_Config cfg = ppu->cfg;

	memset(ppu, 0, sizeof(struct ppu));
	ppu_set_config(ppu, &cfg);

	memcpy(ppu->palette_ram, POWER_UP_PALETTE, 32);

	ppu->CTRL.incr = 1;
	ppu->CTRL.sprite_h = 8;
	ppu->MASK.grayscale = 0x3F;
}

size_t ppu_set_state(struct ppu *ppu, const void *state, size_t size)
{
	if (size >= sizeof(struct ppu)) {
		*ppu = *((struct ppu *) state);

		return sizeof(struct ppu);
	}

	return 0;
}

void *ppu_get_state(struct ppu *ppu, size_t *size)
{
	*size = sizeof(struct ppu);

	struct ppu *state = malloc(*size);
	*state = *ppu;

	return state;
}
