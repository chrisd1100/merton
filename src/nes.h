#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

#define NES_FRAME_WIDTH  256
#define NES_FRAME_HEIGHT 240
#define NES_CLOCK        1789773

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

typedef struct {
	size_t offset;
	uint8_t prg;
	uint8_t chr;
	NES_Mirror mirror;
	uint16_t mapper;
	uint8_t submapper;
	bool battery;
	bool useRAMSizes;
	struct {
		uint32_t wram;
		uint32_t sram;
	} prgSize;
	struct {
		uint32_t wram;
		uint32_t sram;
	} chrSize;
} NES_CartDesc;

typedef struct NES NES;

typedef void (*NES_AudioCallback)(const int16_t *frames, uint32_t count, void *opaque);
typedef void (*NES_VideoCallback)(const uint32_t *frame, void *opaque);
typedef void (*NES_LogCallback)(const char *msg);

void NES_Create(NES_VideoCallback videoCallback, NES_AudioCallback audioCallback, const void *opaque,
	uint32_t sampleRate, bool stereo, NES **nes);
void NES_LoadCart(NES *ctx, const void *rom, size_t romSize, const void *sram, size_t sramSize, const NES_CartDesc *hdr);
bool NES_CartLoaded(NES *ctx);
uint32_t NES_NextFrame(NES *ctx);
void NES_ControllerButton(NES *nes, uint8_t player, NES_Button button, bool pressed);
void NES_ControllerState(NES *nes, uint8_t player, uint8_t state);
void NES_Reset(NES *ctx, bool hard);
void NES_SetStereo(NES *ctx, bool stereo);
void NES_SetSampleRate(NES *ctx, uint32_t sampleRate);
void NES_SetAPUClock(NES *ctx, uint32_t hz);
void NES_SetChannels(NES *ctx, uint32_t channels);
size_t NES_SRAMDirty(NES *ctx);
void NES_GetSRAM(NES *ctx, void *sram, size_t size);
void NES_Destroy(NES **nes);

void NES_SetLogCallback(NES_LogCallback logCallback);
void NES_Log(const char *fmt, ...);

void *NES_GetState(NES *ctx, size_t *size);
void NES_SetState(NES *ctx, const void *state, size_t size);

#ifdef __cplusplus
}
#endif
