#include "lib/lib.h"

#include <windows.h>

static bool INIT;
static double FREQUENCY;

void time_sleep(uint32_t ms)
{
	Sleep(ms);
}

int64_t time_stamp(void)
{
	LARGE_INTEGER ts;
	QueryPerformanceCounter(&ts);

	return ts.QuadPart;
}

double time_diff(int64_t begin, int64_t end)
{
	if (!INIT) {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		FREQUENCY = (double) frequency.QuadPart / 1000.0;
		INIT = true;
	}

	return (double) (end - begin) / FREQUENCY;
}
