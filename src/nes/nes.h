// Copyright (c) 2019-2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

#define NES_FRAME_WIDTH  256
#define NES_FRAME_HEIGHT 240

#define NES_CONFIG_DEFAULTS \
	{NES_PALETTE_SMOOTH, 44100, NES_CHANNEL_ALL, 0, 0, 8, true}

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	NES_BUTTON_A       = 0x01,
	NES_BUTTON_B       = 0x02,
	NES_BUTTON_SELECT  = 0x04,
	NES_BUTTON_START   = 0x08,
	NES_BUTTON_UP      = 0x10,
	NES_BUTTON_DOWN    = 0x20,
	NES_BUTTON_LEFT    = 0x40,
	NES_BUTTON_RIGHT   = 0x80,
} NES_Button;

typedef enum {
	NES_CHANNEL_PULSE_0  = 0x01,
	NES_CHANNEL_PULSE_1  = 0x02,
	NES_CHANNEL_EXT_0    = 0x04,
	NES_CHANNEL_EXT_1    = 0x08,
	NES_CHANNEL_EXT_2    = 0x10,
	NES_CHANNEL_TRIANGLE = 0x20,
	NES_CHANNEL_NOISE    = 0x40,
	NES_CHANNEL_DMC      = 0x80,
	NES_CHANNEL_ALL      = 0xFF,
} NES_Channel;

typedef enum {
	NES_MIRROR_HORIZONTAL = 0x00110011,
	NES_MIRROR_VERTICAL   = 0x01010101,
	NES_MIRROR_SINGLE1    = 0x00000000,
	NES_MIRROR_SINGLE0    = 0x11111111,
	NES_MIRROR_FOUR       = 0x01230123,
	NES_MIRROR_FOUR8      = 0x01234567,
	NES_MIRROR_FOUR16     = 0x89ABCDEF,
} NES_Mirror;

typedef enum {
	NES_PALETTE_SMOOTH    = 0,
	NES_PALETTE_CLASSIC   = 1,
	NES_PALETTE_COMPOSITE = 2,
	NES_PALETTE_PVM_D93   = 3,
	NES_PALETTE_PC10      = 4,
	NES_PALETTE_SONY_CXA  = 5,
	NES_PALETTE_WAVEBEAM  = 6,
} NES_Palette;

typedef struct {
	size_t offset;
	uint32_t prgROMSize;
	uint32_t chrROMSize;
	uint32_t prgWRAMSize;
	uint32_t prgSRAMSize;
	uint32_t chrWRAMSize;
	uint32_t chrSRAMSize;
	NES_Mirror mirror;
	uint16_t mapper;
	uint8_t submapper;
	bool battery;
} NES_CartDesc;

typedef struct {
	NES_Palette palette;
	uint32_t sampleRate;
	uint32_t channels;
	uint16_t preNMI;
	uint16_t postNMI;
	uint8_t maxSprites;
	bool stereo;
} NES_Config;

typedef struct NES NES;

typedef void (*NES_AudioCallback)(const int16_t *frames, uint32_t count, void *opaque);
typedef void (*NES_VideoCallback)(const uint32_t *frame, void *opaque);
typedef void (*NES_LogCallback)(const char *msg);

// Cart
void NES_LoadCart(NES *ctx, const void *rom, size_t romSize, const void *sram, size_t sramSize, const NES_CartDesc *hdr);
bool NES_CartLoaded(NES *ctx);

// Step
uint32_t NES_NextFrame(NES *ctx, NES_VideoCallback videoCallback,
	NES_AudioCallback audioCallback, const void *opaque);

// Input
void NES_ControllerButton(NES *nes, uint8_t player, NES_Button button, bool pressed);
void NES_ControllerState(NES *nes, uint8_t player, uint8_t state);

// Configuration
void NES_SetConfig(NES *ctx, const NES_Config *cfg);
void NES_APUClockDrift(NES *ctx, uint32_t clock, bool over);

// SRAM
size_t NES_SRAMDirty(NES *ctx);
void NES_GetSRAM(NES *ctx, void *sram, size_t size);

// Lifecycle
void NES_Create(const NES_Config *cfg, NES **nes);
void NES_Destroy(NES **nes);
void NES_Reset(NES *ctx, bool hard);
bool NES_SetState(NES *ctx, const void *state, size_t size);
void *NES_GetState(NES *ctx, size_t *size);

// Logging
void NES_SetLogCallback(NES_LogCallback logCallback);
void NES_Log(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
