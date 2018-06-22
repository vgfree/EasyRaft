#pragma once

#include <stdint.h>
#include <time.h>
#include <sys/time.h>

uint64_t clock_get_time(void);

static inline void time_now(struct timespec *ts)
{
	clock_gettime(CLOCK_MONOTONIC, ts);
}

long time_diff(struct timespec *begin, struct timespec *end);

