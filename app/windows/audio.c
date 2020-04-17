#include "lib.h"

#include <stdlib.h>

#include <windows.h>

#include <initguid.h>
DEFINE_GUID(CLSID_MMDeviceEnumerator,  0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator,   0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient,          0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioRenderClient,    0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);
DEFINE_GUID(IID_IMMNotificationClient, 0x7991eec9, 0x7e89, 0x4d85, 0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0);

#define COBJMACROS
#include <mmdeviceapi.h>
#include <audioclient.h>

#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLE_SIZE sizeof(int16_t)
#define AUDIO_BUFFER_SIZE ((1 * 1000 * 1000 * 1000) / 100) // 1 second

struct audio {
	bool coinit;
	bool playing;
	bool notification_init;
	uint32_t sample_rate;
	UINT32 buffer_size;
	IMMDeviceEnumerator *enumerator;
	IMMNotificationClient notification;
	IAudioClient *client;
	IAudioRenderClient *render;
};

static bool DEFAULT_CHANGED;


/*** "OVERRIDDEN" IMMNotificationClient ***/

static HRESULT STDMETHODCALLTYPE LIB_IMMNotificationClient_QueryInterface(IMMNotificationClient *This,
	REFIID riid, void **ppvObject)
{
	This;

	if (IsEqualGUID(&IID_IMMNotificationClient, riid) || IsEqualGUID(&IID_IUnknown, riid)) {
		*ppvObject = This;
		return S_OK;

	} else {
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
}

static ULONG STDMETHODCALLTYPE LIB_IMMNotificationClient_AddRef(IMMNotificationClient *This)
{
	This;

	return 1;
}

static ULONG STDMETHODCALLTYPE LIB_IMMNotificationClient_Release(IMMNotificationClient *This)
{
	This;

	return 1;
}

static HRESULT STDMETHODCALLTYPE LIB_IMMNotificationClient_OnDeviceStateChanged(IMMNotificationClient *This,
	LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
	This; pwstrDeviceId; dwNewState;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE LIB_IMMNotificationClient_OnDeviceAdded(IMMNotificationClient *This, LPCWSTR pwstrDeviceId)
{
	This; pwstrDeviceId;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE LIB_IMMNotificationClient_OnDeviceRemoved(IMMNotificationClient *This, LPCWSTR pwstrDeviceId)
{
	This; pwstrDeviceId;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE LIB_IMMNotificationClient_OnDefaultDeviceChanged(IMMNotificationClient *This,
	EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
{
	This; flow; pwstrDeviceId;

	if (role == eConsole)
		DEFAULT_CHANGED = true;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE LIB_IMMNotificationClient_OnPropertyValueChanged(IMMNotificationClient *This,
	LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
	This; pwstrDeviceId; key;

	return S_OK;
}

static CONST_VTBL IMMNotificationClientVtbl LIB_IMMNotificationClient = {
	.QueryInterface         = LIB_IMMNotificationClient_QueryInterface,
	.AddRef                 = LIB_IMMNotificationClient_AddRef,
	.Release                = LIB_IMMNotificationClient_Release,
	.OnDeviceStateChanged   = LIB_IMMNotificationClient_OnDeviceStateChanged,
	.OnDeviceAdded          = LIB_IMMNotificationClient_OnDeviceAdded,
	.OnDeviceRemoved        = LIB_IMMNotificationClient_OnDeviceRemoved,
	.OnDefaultDeviceChanged = LIB_IMMNotificationClient_OnDefaultDeviceChanged,
	.OnPropertyValueChanged = LIB_IMMNotificationClient_OnPropertyValueChanged,
};


/*** AUDIO ***/

static void audio_device_destroy(struct audio *ctx)
{
	if (ctx->client && ctx->playing)
		IAudioClient_Stop(ctx->client);

	if (ctx->render)
		IAudioRenderClient_Release(ctx->render);

	if (ctx->client)
		IAudioClient_Release(ctx->client);

	ctx->client = NULL;
	ctx->render = NULL;
	ctx->playing = false;
}

static HRESULT audio_device_create(struct audio *ctx)
{
	IMMDevice *device = NULL;
	HRESULT e = IMMDeviceEnumerator_GetDefaultAudioEndpoint(ctx->enumerator, eRender, eConsole, &device);
	if (e != S_OK) goto except;

	e = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, &ctx->client);
	if (e != S_OK) goto except;

	WAVEFORMATEX pwfx = {0};
	pwfx.wFormatTag = WAVE_FORMAT_PCM;
	pwfx.nChannels = AUDIO_CHANNELS;
	pwfx.nSamplesPerSec = ctx->sample_rate;
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

	if (device)
		IMMDevice_Release(device);

	if (e != S_OK)
		audio_device_destroy(ctx);

	return e;
}

enum lib_status audio_create(struct audio **audio, uint32_t sample_rate)
{
	struct audio *ctx = *audio = calloc(1, sizeof(struct audio));
	ctx->sample_rate = sample_rate;

	ctx->coinit = true;
	CoInitialize(NULL);

	HRESULT e = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
		&IID_IMMDeviceEnumerator, &ctx->enumerator);
	if (e != S_OK) goto except;

	ctx->notification.lpVtbl = &LIB_IMMNotificationClient;
	e = IMMDeviceEnumerator_RegisterEndpointNotificationCallback(ctx->enumerator, &ctx->notification);
	if (e != S_OK) goto except;
	ctx->notification_init = true;

	e = audio_device_create(ctx);
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
	// Reinit device if changed on the fly
	if (DEFAULT_CHANGED) {
		audio_device_destroy(ctx);
		if (audio_device_create(ctx) != S_OK)
			return;

		audio_play(ctx);
		DEFAULT_CHANGED = false;
	}

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

	audio_device_destroy(ctx);

	if (ctx->notification_init)
		IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(ctx->enumerator, &ctx->notification);

	if (ctx->enumerator)
		IMMDeviceEnumerator_Release(ctx->enumerator);

	if (ctx->coinit)
		CoUninitialize();

	free(ctx);
	*audio = NULL;
}
