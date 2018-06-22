#include "timeopt.h"

uint64_t clock_get_time(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	return (uint64_t)ts.tv_sec * 1000000000LL + (uint64_t)ts.tv_nsec;
}

long time_diff(struct timespec *begin, struct timespec *end)
{
	long            elapsed = 0;
	struct timespec now = {};

	if ((begin == NULL) && (end == NULL)) {
		return elapsed;
	}

	if (begin == NULL) {
		time_now(&now);
		begin = &now;
	}

	if (end == NULL) {
		time_now(&now);
		end = &now;
	}

	elapsed = end->tv_sec - begin->tv_sec;
	elapsed *= 1000 * 1000 * 1000;

	if (end->tv_nsec > begin->tv_nsec) {
		elapsed += end->tv_nsec - begin->tv_nsec;
	} else {
		elapsed += end->tv_nsec;
		elapsed -= begin->tv_nsec;
	}

	return elapsed;
}

