#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <unistd.h>

#include "etask.h"

#if GCC_VERSION
  #define likely(x)     __builtin_expect(!!(x), 1)
  #define unlikely(x)   __builtin_expect(!!(x), 0)
#else
  #define likely(x)     (!!(x))
  #define unlikely(x)   (!!(x))
#endif

int eventfd_xrecv(int efd, eventfd_t *value)
{
	int ret;

	do {
		ret = eventfd_read(efd, value);
	} while (unlikely(ret < 0) && errno == EINTR);

	if ((ret != 0) && unlikely(errno != EAGAIN)) {
		// x_printf(E, "eventfd_read() failed");
		abort();
	}

	return ret;
}

int eventfd_xsend(int efd, eventfd_t value)
{
	int ret;

	do {
		ret = eventfd_write(efd, value);
	} while (unlikely(ret < 0) && (errno == EINTR || errno == EAGAIN));

	if (unlikely(ret < 0)) {
		// x_printf(E, "eventfd_write() failed");
		abort();
	}

	return ret;
}

/*
 * timeout 毫秒
 */
int eventfd_xwait(int efds[], int nums, int timeout)
{
	int             max = nums;
	struct pollfd   pfds[max];

	for (int i = 0; i < max; i++) {
		pfds[i].fd = efds[i];
		pfds[i].events = POLLIN;
	}

	do {
		int have = poll(pfds, max, timeout);

		if (have < 0) {
			if ((errno == EINTR) || (errno == EAGAIN)) {
				continue;
			}

			assert(0);
		} else if (have == 0) {
			/*timeout*/
			return 0;
		} else {
			int done = 0;

			/* An event on one of the fds has occurred. */
			for (int i = 0; i < max; i++) {
				int ev = pfds[i].revents;

				if (ev & (POLLERR | POLLHUP | POLLNVAL)) {
					assert(0);
				}

				if (ev & POLLIN) {
					efds[done] = pfds[i].fd;

					done++;

					if (done == have) {
						break;
					}
				}
			}

			return have;
		}
	} while (1);
}

// --------------------------------------//

struct etask *etask_make(struct etask *etask)
{
	if (etask) {
		etask->freeable = false;
	} else {
		etask = calloc(1, sizeof(*etask));
		assert(etask);
		etask->freeable = true;
	}

	etask->efd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
	return etask;
}

void etask_free(struct etask *etask)
{
	assert(etask);

	close(etask->efd);

	if (etask->freeable) {
		free(etask);
	}
}

void etask_awake(struct etask *etask)
{
	assert(etask);

	eventfd_xsend(etask->efd, 1);
}

void etask_sleep(struct etask *etask)
{
	assert(etask);

	eventfd_t val = 0;
	do {
		int efd = etask->efd;
		eventfd_xwait(&efd, 1, -1);

		eventfd_xrecv(etask->efd, &val);
	} while (val == 0);
}

bool etask_twait(struct etask *etask, int msec)
{
	assert(etask);

	int     efd = etask->efd;
	bool    have = eventfd_xwait(&efd, 1, msec) ? true : false;

	if (have) {
		eventfd_t val = 0;
		eventfd_xrecv(etask->efd, &val);

		if (val == 0) {
			have = false;
		}
	}

	return have;
}

