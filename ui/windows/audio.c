#include "lib.h"

#include <stdlib.h>

#include <windows.h>

#include <initguid.h>
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator,  0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient,         0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioRenderClient,   0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);

#define COBJMACROS
#include <mmdeviceapi.h>
#include <audioclient.h>

#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLE_SIZE sizeof(int16_t)
#define AUDIO_BUFFER_SIZE ((1 * 1000 * 1000 * 1000) / 100) // 1 second

struct audio {
	bool coinit;
	bool playing;
	UINT32 buffer_size;
	IAudioClient *client;
	IAudioRenderClient *render;
};

enum lib_status audio_create(struct audio **audio, uint32_t sample_rate)
{
	struct audio *ctx = *audio = calloc(1, sizeof(struct audio));

	ctx->coinit = true;
	CoInitialize(NULL);

	IMMDeviceEnumerator *enumerator = NULL;
	HRESULT e = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
		&IID_IMMDeviceEnumerator, &enumerator);
	if (e != S_OK) goto except;

	IMMDevice *device = NULL;
	e = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device);
	if (e != S_OK) goto except;

	e = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, &ctx->client);
	if (e != S_OK) goto except;

	WAVEFORMATEX pwfx = {0};
	pwfx.wFormatTag = WAVE_FORMAT_PCM;
	pwfx.nChannels = AUDIO_CHANNELS;
	pwfx.nSamplesPerSec = sample_rate;
	pwfx.wBitsPerSample = AUDIO_SAMPLE_SIZE * 8;
	pwfx.nBlockAlign = pwfx.nChannels * pwfx.wBitsPerSample / 8;
	pwfx.nAvgBytesPerSec = pwfx.nSamplesPerSec * pwfx.nBlockAlign;

	e = IAudioClient_Initialize(ctx->client, AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
		AUDIO_BUFFER_SIZE, 0, &pwfx, NULL);
	if (e != S_OK) goto except;

	e = IAudioClient_GetBufferSize(ctx->client, &ctx->buffer_size);
	if (e != S_OK) goto except;

	e = IAudioClient_GetService(ctx->client, &IID_IAudioRenderClient, &ctx->render);
	if (e != S_OK) goto except;

	except:

	if (e != S_OK)
		audio_destroy(audio);

	return e == S_OK ? LIB_OK : LIB_ERR;
}

uint32_t audio_queued_frames(struct audio *ctx)
{
	UINT32 padding = 0;
	if (IAudioClient_GetCurrentPadding(ctx->client, &padding) == S_OK)
		return padding;

	return ctx->buffer_size;
}

bool audio_playing(struct audio *ctx)
{
	return ctx->playing;
}

void audio_play(struct audio *ctx)
{
	if (!ctx->playing) {
		HRESULT e = IAudioClient_Start(ctx->client);

		if (e == S_OK)
			ctx->playing = true;
	}
}

void audio_stop(struct audio *ctx)
{
	if (ctx->playing) {
		HRESULT e = IAudioClient_Stop(ctx->client);

		if (e == S_OK)
			ctx->playing = false;
	}
}

void audio_queue(struct audio *ctx, const int16_t *frames, uint32_t count)
{
	if (ctx->buffer_size - audio_queued_frames(ctx) >= count) {
		BYTE *buffer = NULL;
		HRESULT e = IAudioRenderClient_GetBuffer(ctx->render, count, &buffer);

		if (e == S_OK) {
			memcpy(buffer, frames, count * AUDIO_CHANNELS * AUDIO_SAMPLE_SIZE);
			IAudioRenderClient_ReleaseBuffer(ctx->render, count, 0);
		}
	}
}

void audio_destroy(struct audio **audio)
{
	if (!audio || !*audio)
		return;

	struct audio *ctx = *audio;

	if (ctx->client && ctx->playing)
		IAudioClient_Stop(ctx->client);

	if (ctx->render)
		IAudioRenderClient_Release(ctx->render);

	if (ctx->client)
		IAudioClient_Release(ctx->client);

	if (ctx->coinit)
		CoUninitialize();

	free(ctx);
	*audio = NULL;
}
