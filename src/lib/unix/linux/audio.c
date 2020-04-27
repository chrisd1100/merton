#include "lib.h"

enum lib_status audio_create(struct audio **audio, uint32_t sample_rate)
{
	return LIB_OK;
}

uint32_t audio_queued_frames(struct audio *ctx)
{
	return 0;
}

bool audio_playing(struct audio *ctx)
{
	return true;
}

void audio_play(struct audio *ctx)
{
}

void audio_queue(struct audio *ctx, int16_t *samples, uint32_t count)
{
}

void audio_destroy(struct audio **audio)
{
}
