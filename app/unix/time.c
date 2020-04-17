#include "lib.h"

#include <time.h>

void time_sleep(uint32_t ms)
{
	struct timespec ts = {0};
	ts.tv_nsec = ms * 1000 * 1000;

	nanosleep(&ts, NULL);
}

int64_t time_stamp(void)
{
	struct timespec ts = {0};
	clock_gettime(CLOCK_MONOTONIC, &ts);

	uint64_t r = ts.tv_sec * 1000 * 1000;
	r += ts.tv_nsec / 1000;

	return r;
}

double time_diff(int64_t begin, int64_t end)
{
	return (double) (end - begin) / 1000.0;
}
