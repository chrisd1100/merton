#include "lib.h"

#include <pthread.h>

#include <AudioToolbox/AudioToolbox.h>

#define AUDIO_CHANNELS    2
#define AUDIO_SAMPLE_SIZE sizeof(int16_t)

#define AUDIO_BUFS     24
#define AUDIO_BUF_SIZE (44100 * AUDIO_CHANNELS * AUDIO_SAMPLE_SIZE)

struct audio {
	pthread_mutex_t mutex;
	AudioQueueRef q;
	AudioQueueBufferRef audio_buf[AUDIO_BUFS];
	size_t queued;
	uint32_t sample_rate;
	bool playing;
};

static void audio_queue_buffer(struct audio *ctx, AudioQueueBufferRef buf, const void *data, size_t size)
{
	if (data) {
		memcpy(buf->mAudioData, data, size);

	} else {
		memset(buf->mAudioData, 0, size);
	}

	buf->mAudioDataByteSize = size;
	buf->mUserData = "FILLED";
	ctx->queued += size;

	AudioQueueEnqueueBuffer(ctx->q, buf, 0, NULL);
}

static void audio_queue_callback(void *opaque, AudioQueueRef q, AudioQueueBufferRef buf)
{
	struct audio *ctx = (struct audio *) opaque;

	pthread_mutex_lock(&ctx->mutex);

	if (buf->mUserData) {
		ctx->queued -= buf->mAudioDataByteSize;
		buf->mUserData = NULL;
	}

	// Queue silence
	if (ctx->queued == 0)
		audio_queue_buffer(ctx, buf, NULL, AUDIO_CHANNELS * AUDIO_SAMPLE_SIZE * (ctx->sample_rate / 10));

	pthread_mutex_unlock(&ctx->mutex);
}

enum lib_status audio_create(struct audio **audio, uint32_t sample_rate)
{
	struct audio *ctx = *audio = calloc(1, sizeof(struct audio));
	ctx->sample_rate = sample_rate;

	pthread_mutex_init(&ctx->mutex, NULL);

	AudioStreamBasicDescription format = {0};
	format.mSampleRate = sample_rate;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	format.mFramesPerPacket = 1;
	format.mChannelsPerFrame = AUDIO_CHANNELS;
	format.mBitsPerChannel = AUDIO_SAMPLE_SIZE * 8;
	format.mBytesPerPacket = AUDIO_SAMPLE_SIZE * AUDIO_CHANNELS;;
	format.mBytesPerFrame = format.mBytesPerPacket;

	AudioQueueNewOutput(&format, audio_queue_callback, ctx, NULL, NULL, 0, &ctx->q);

	for (int32_t x = 0; x < AUDIO_BUFS; x++)
		AudioQueueAllocateBuffer(ctx->q, AUDIO_BUF_SIZE, &ctx->audio_buf[x]);

	return LIB_OK;
}

uint32_t audio_queued_frames(struct audio *ctx)
{
	pthread_mutex_lock(&ctx->mutex);

	uint32_t r = ctx->queued / (AUDIO_CHANNELS * AUDIO_SAMPLE_SIZE);

	pthread_mutex_unlock(&ctx->mutex);

	return r;
}

bool audio_playing(struct audio *ctx)
{
	return ctx->playing;
}

void audio_play(struct audio *ctx)
{
	AudioQueueStart(ctx->q, NULL);
	ctx->playing = true;
}

void audio_stop(struct audio *ctx)
{
	AudioQueueStop(ctx->q, true);
	ctx->playing = false;
}

void audio_queue(struct audio *ctx, const int16_t *frames, uint32_t count)
{
	size_t size = count * AUDIO_CHANNELS * AUDIO_SAMPLE_SIZE;

	if (size <= AUDIO_BUF_SIZE) {
		pthread_mutex_lock(&ctx->mutex);

		for (int32_t x = 0; x < AUDIO_BUFS; x++) {
			AudioQueueBufferRef buf = ctx->audio_buf[x];

			if (!buf->mUserData) {
				audio_queue_buffer(ctx, buf, frames, size);
				break;
			}
		}

		pthread_mutex_unlock(&ctx->mutex);
	}
}

void audio_destroy(struct audio **audio)
{
	if (!audio || !*audio)
		return;

	struct audio *ctx = *audio;

	for (int32_t x = 0; x < AUDIO_BUFS; x++) {
		if (ctx->audio_buf[x])
			AudioQueueFreeBuffer(ctx->q, ctx->audio_buf[x]);
	}

	if (ctx->q)
		AudioQueueDispose(ctx->q, true);

	pthread_mutex_destroy(&ctx->mutex);

	free(audio);
	*audio = NULL;
}
